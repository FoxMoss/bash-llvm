#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ast/ast.h"
#include "codegen.h"
#include "helper.h"

std::expected<llvm::Value*, std::string> CStyleForExprAST::codegen(
    CodegenState& state) {
  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  auto loop = llvm::BasicBlock::Create(*state.context, "forloop", parent_func);
  auto body_block = llvm::BasicBlock::Create(*state.context, "forbody");
  auto merge = llvm::BasicBlock::Create(*state.context, "formerge");

  auto assigned = assignment->codegen(state);
  UNWRAP_EXPECTED(assigned)

  state.builder->CreateBr(loop);

  state.builder->SetInsertPoint(loop);

  auto cond_check = check->codegen(state);
  UNWRAP_EXPECTED(cond_check)

  state.builder->CreateCondBr(cond_check.value(), body_block, merge);
  loop = state.builder->GetInsertBlock();

  parent_func->insert(parent_func->end(), body_block);
  state.builder->SetInsertPoint(body_block);

  auto body_val = body.value()->codegen(state);
  UNWRAP_EXPECTED(body_val);

  auto incremented = increment->codegen(state);
  UNWRAP_EXPECTED(incremented)

  state.builder->CreateBr(loop);

  body_block = state.builder->GetInsertBlock();

  parent_func->insert(parent_func->end(), merge);
  state.builder->SetInsertPoint(merge);

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
}

std::expected<llvm::Value*, std::string> ForInExprAST::codegen(
    CodegenState& state) {
  auto range_array = range->codegen(state);
  UNWRAP_EXPECTED(range_array)

  if (!range_array.value()->getType()->isVectorTy()) {
    return std::unexpected("Range is not an array");
  }

  auto range_constant = static_cast<llvm::ConstantVector*>(range_array.value());

  auto range_array_type =
      static_cast<llvm::FixedVectorType*>(range_array.value()->getType());

  if (range_constant == nullptr || range_array_type == nullptr) {
    return std::unexpected("range value not an array");
  }

  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  llvm::BasicBlock* header = state.builder->GetInsertBlock();

  auto loop = llvm::BasicBlock::Create(*state.context, "forloop", parent_func);
  auto body_block = llvm::BasicBlock::Create(*state.context, "forbody");
  auto merge = llvm::BasicBlock::Create(*state.context, "formerge");

  auto start_index =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0);

  auto step = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1);

  auto max_index =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                             range_array_type->getNumElements());

  state.builder->CreateBr(loop);

  state.builder->SetInsertPoint(loop);

  llvm::PHINode* variable = state.builder->CreatePHI(
      llvm::Type::getInt64Ty(*state.context), 2, "indextmp");
  variable->addIncoming(start_index, header);

  // i < max
  auto should_cont = state.builder->CreateICmpULT(variable, max_index);

  state.builder->CreateCondBr(should_cont, body_block, merge);
  loop = state.builder->GetInsertBlock();

  parent_func->insert(parent_func->end(), body_block);
  state.builder->SetInsertPoint(body_block);

  auto variable_state =
      state.builder->CreateExtractElement(range_constant, variable);

  auto stored = store_variable_memory(state, index, variable_state);
  UNWRAP_EXPECTED(stored)

  if (!body.has_value()) {
    return std::unexpected("For body never defined");
  }
  auto body_value = body.value()->codegen(state);
  UNWRAP_EXPECTED(body_value)

  auto next_var = state.builder->CreateAdd(variable, step);
  variable->addIncoming(next_var, state.builder->GetInsertBlock());

  state.builder->CreateBr(loop);

  parent_func->insert(parent_func->end(), merge);
  state.builder->SetInsertPoint(merge);

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
}

std::expected<llvm::Value*, std::string> WhileAST::codegen(
    CodegenState& state) {
  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  auto header_block =
      llvm::BasicBlock::Create(*state.context, "whileheader", parent_func);
  auto body_block = llvm::BasicBlock::Create(*state.context, "whilebody");
  auto merge_block = llvm::BasicBlock::Create(*state.context, "whilemerge");

  state.builder->CreateBr(header_block);

  state.builder->SetInsertPoint(header_block);

  auto cond_v = condition->codegen(state);
  UNWRAP_EXPECTED(cond_v)

  auto cond_bool = reduce_to_bool(state, cond_v.value());
  UNWRAP_EXPECTED(cond_bool)

  state.builder->CreateCondBr(cond_bool.value(), body_block, merge_block);

  header_block = state.builder->GetInsertBlock();

  parent_func->insert(parent_func->end(), body_block);

  state.builder->SetInsertPoint(body_block);

  auto body_v = body->codegen(state);
  UNWRAP_EXPECTED(body_v)

  state.builder->CreateBr(header_block);

  body_block = state.builder->GetInsertBlock();

  parent_func->insert(parent_func->end(), merge_block);

  state.builder->SetInsertPoint(merge_block);

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
}
std::expected<llvm::Value*, std::string> ConditionExprAST::codegen(
    CodegenState& state) {
  auto first_v = first_var->codegen(state);
  UNWRAP_EXPECTED(first_v)
  auto first_f = cast_to_float(state, first_v.value());
  UNWRAP_EXPECTED(first_f)

  auto second_v = second_var->codegen(state);
  UNWRAP_EXPECTED(second_v)
  auto second_f = cast_to_float(state, second_v.value());
  UNWRAP_EXPECTED(second_f)

  switch (op) {
    case CONDITION_EQ:
      return state.builder->CreateFCmpOEQ(first_f.value(), second_f.value());
    case CONDITION_NE:
      return state.builder->CreateFCmpONE(first_f.value(), second_f.value());
    case CONDITION_LT:
      return state.builder->CreateFCmpOLT(first_f.value(), second_f.value());
    case CONDITION_LE:
      return state.builder->CreateFCmpOLE(first_f.value(), second_f.value());
    case CONDITION_GT:
      return state.builder->CreateFCmpOGT(first_f.value(), second_f.value());
    case CONDITION_GE:
      return state.builder->CreateFCmpOGE(first_f.value(), second_f.value());
  }
  std::unreachable();
}
