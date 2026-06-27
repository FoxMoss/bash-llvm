#include "codegen.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ast/ast.h"
#include "codegen.h"
#include "helper.h"
#include "lexer.h"

std::expected<llvm::Value*, std::string> NumericExprAST::codegen(
    CodegenState& state) {
  return llvm::ConstantFP::get(*state.context, llvm::APFloat(value));
}

std::expected<llvm::Value*, std::string> StringExprAST::codegen(
    CodegenState& state) {
  return state.builder->CreateGlobalString(val);
}

std::expected<llvm::Value*, std::string> IdentifierExprAST::codegen(
    CodegenState& state) {
  return get_variable_memory(state, name);
}

std::expected<llvm::Value*, std::string> CallExprAST::codegen(
    CodegenState& state) {
  llvm::Function* program_called =
      state.module->getFunction(std::format("bash_{}", program));
  if (state.sandboxing.block_external_programs && program_called == nullptr)
    return std::unexpected("unknown function referenced");

  bool external_program = false;
  if (program_called == nullptr) {
    program_called =
        state.module->getFunction(std::format("external_program", program));
    external_program = true;
  }

  // If argument mismatch error.
  if (external_program) {
    if (program_called->arg_size() != 4)
      return std::unexpected("External program " + program + " is illdefined");
  } else {
    if (program_called->arg_size() != 3)
      return std::unexpected("Program " + program + " is illdefined");
  }

  std::vector<llvm::Value*> arg_values;
  if (args.has_value()) {
    auto args_codegen = args->get()->codegen(state);
    UNWRAP_EXPECTED(args_codegen)

    if (!args_codegen.value()->getType()->isVectorTy()) {
      return std::unexpected("Args value not an array");
    }

    auto args_array = static_cast<llvm::ConstantVector*>(args_codegen.value());

    auto args_array_type =
        static_cast<llvm::FixedVectorType*>(args_codegen.value()->getType());

    if (args_array == nullptr || args_array_type == nullptr) {
      return std::unexpected("Args value not an array");
    }
    auto stack_args = state.builder->CreateAlloca(
        llvm::PointerType::get(*state.context, 0),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                               args_array_type->getNumElements()));

    Align prefered_align =
        state.builder->GetInsertBlock()->getDataLayout().getPrefTypeAlign(
            llvm::PointerType::get(*state.context, 0));

    state.builder->CreateAlignedStore(args_array, stack_args, prefered_align);

    if (!state.named_values["variable_memory"].has_value()) {
      return std::unexpected("Variable map does not exist");
    }

    if (external_program) {
      arg_values = {
          state.named_values["variable_memory"].value(),
          state.builder->CreateGlobalString(program),
          llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(*state.context),
                                 args_array_type->getNumElements()),
          stack_args};

    } else {
      arg_values = {
          state.named_values["variable_memory"].value(),
          llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(*state.context),
                                 args_array_type->getNumElements()),
          stack_args};
    }
  } else {
    if (external_program) {
      arg_values = {state.named_values["variable_memory"].value(),
                    state.builder->CreateGlobalString(program),
                    llvm::ConstantInt::get(
                        llvm::IntegerType::getInt64Ty(*state.context), 0),
                    llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(*state.context, 0))};
    } else {
      arg_values = {state.named_values["variable_memory"].value(),
                    llvm::ConstantInt::get(
                        llvm::IntegerType::getInt64Ty(*state.context), 0),
                    llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(*state.context, 0))};
    }
  }
  return state.builder->CreateICmpEQ(
      state.builder->CreateCall(program_called, arg_values),
      llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(*state.context), 0));
}

std::expected<llvm::Value*, std::string> PipeExprAST::codegen(
    CodegenState& state) {
  return std::unexpected("pipes are not implemented");
}

std::expected<llvm::Value*, std::string> StatementOpExprAST::codegen(
    CodegenState& state) {
  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  auto left_full = first->codegen(state);
  UNWRAP_EXPECTED(left_full)

  auto left = reduce_to_bool(state, left_full.value());
  UNWRAP_EXPECTED(left)

  auto short_path =
      llvm::BasicBlock::Create(*state.context, "stmtopshort", parent_func);

  auto both_path = llvm::BasicBlock::Create(*state.context, "stmtopboth");
  auto merge = llvm::BasicBlock::Create(*state.context, "stmtopmerge");

  switch (op) {
    case STATEMENT_OP_AND: {
      state.builder->CreateCondBr(left.value(), both_path, short_path);

      state.builder->SetInsertPoint(short_path);

      state.builder->CreateBr(merge);

      short_path = state.builder->GetInsertBlock();

      parent_func->insert(parent_func->end(), both_path);

      state.builder->SetInsertPoint(both_path);

      auto right_full = second->codegen(state);
      UNWRAP_EXPECTED(right_full)

      auto right = reduce_to_bool(state, right_full.value());
      UNWRAP_EXPECTED(right)

      state.builder->CreateBr(merge);

      both_path = state.builder->GetInsertBlock();

      parent_func->insert(parent_func->end(), merge);
      state.builder->SetInsertPoint(merge);

      llvm::PHINode* phinode = state.builder->CreatePHI(
          llvm::Type::getInt1Ty(*state.context), 2, "andtmp");

      phinode->addIncoming(right.value(), both_path);
      phinode->addIncoming(left.value(), short_path);
      return phinode;
    };
      // as a natural outcome of logic these to are the same just with the cond
      // fliped arround
    case STATEMENT_OP_OR: {
      state.builder->CreateCondBr(left.value(), short_path, both_path);

      state.builder->SetInsertPoint(short_path);

      auto left_again = reduce_to_bool(state, left_full.value());
      UNWRAP_EXPECTED(left_again)

      state.builder->CreateBr(merge);

      short_path = state.builder->GetInsertBlock();

      parent_func->insert(parent_func->end(), both_path);

      state.builder->SetInsertPoint(both_path);

      auto right_full = second->codegen(state);
      UNWRAP_EXPECTED(right_full)

      auto right = reduce_to_bool(state, right_full.value());
      UNWRAP_EXPECTED(right)

      state.builder->CreateBr(merge);

      both_path = state.builder->GetInsertBlock();

      parent_func->insert(parent_func->end(), merge);
      state.builder->SetInsertPoint(merge);

      llvm::PHINode* phinode = state.builder->CreatePHI(
          llvm::Type::getInt1Ty(*state.context), 2, "ortmp");

      phinode->addIncoming(right.value(), both_path);
      phinode->addIncoming(left_again.value(), short_path);
      return phinode;
    };
    case STATEMENT_OP_UNK:
      return std::unexpected("Statment operator unknown");
  }
  std::unreachable();
}

std::expected<llvm::Value*, std::string> RangeArrayExprAST::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm;

  std::optional<llvm::Value*> val;

  size_t index = 0;
  for (auto& member : values) {
    auto member_value = member->codegen(state);
    UNWRAP_EXPECTED(member_value)

    auto member_const = static_cast<llvm::Constant*>(member_value.value());
    if (member_const == nullptr) {
      return std::unexpected("range value not constant");
    }

    if (!val.has_value()) {
      val = state.builder->CreateInsertElement(
          llvm::VectorType::get(llvm::PointerType::get(*state.context, 0),
                                llvm::ElementCount::get(values.size(), false)),
          member_const, index);
    } else {
      val =
          state.builder->CreateInsertElement(val.value(), member_const, index);
    }

    index++;
  }

  if (!val.has_value()) {
    return std::unexpected("could not make vector");
  }

  return val.value();
}

std::expected<llvm::Value*, std::string> ConcatExprAST::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm;

  auto first_array = first->codegen(state);
  UNWRAP_EXPECTED(first_array)

  if (!first_array.value()->getType()->isVectorTy()) {
    return std::unexpected("first value not an array");
  }

  auto first_constant = static_cast<llvm::ConstantVector*>(first_array.value());

  auto first_array_type =
      static_cast<llvm::FixedVectorType*>(first_array.value()->getType());

  if (first_constant == nullptr || first_array_type == nullptr) {
    return std::unexpected("first value not an array");
  }

  auto second_array = second->codegen(state);
  UNWRAP_EXPECTED(second_array)

  if (!second_array.value()->getType()->isVectorTy()) {
    return std::unexpected("second value not an vector");
  }

  auto second_constant =
      static_cast<llvm::ConstantVector*>(second_array.value());

  auto second_array_type =
      static_cast<llvm::FixedVectorType*>(second_array.value()->getType());

  if (second_constant == nullptr || second_array_type == nullptr) {
    return std::unexpected("second value not an array");
  }

  std::optional<llvm::Value*> val;

  for (uint64_t i = 0; i < first_array_type->getNumElements(); i++) {
    auto value =
        state.builder->CreateExtractElement(first_constant, uint64_t{i});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    if (!val.has_value()) {
      val = state.builder->CreateInsertElement(
          llvm::VectorType::get(
              llvm::PointerType::get(*state.context, 0),
              llvm::ElementCount::get(first_array_type->getNumElements() +
                                          second_array_type->getNumElements(),
                                      false)),
          static_value, i);
    } else {
      val = state.builder->CreateInsertElement(val.value(), static_value, i);
    }
  }

  for (uint64_t i = first_array_type->getNumElements();
       i <
       first_array_type->getNumElements() + second_array_type->getNumElements();
       i++) {
    auto value = state.builder->CreateExtractElement(
        second_constant, uint64_t{i - first_array_type->getNumElements()});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    if (!val.has_value()) {
      val = state.builder->CreateInsertElement(
          llvm::VectorType::get(
              llvm::PointerType::get(*state.context, 0),
              llvm::ElementCount::get(first_array_type->getNumElements() +
                                          second_array_type->getNumElements(),
                                      false)),
          static_value, i);
    } else {
      val = state.builder->CreateInsertElement(val.value(), static_value, i);
    }
  }

  if (!val.has_value()) {
    return std::unexpected("could not concatate ranges");
  }

  return val.value();
}

std::expected<llvm::Value*, std::string> ConvertToStringExprAST::codegen(
    CodegenState& state) {
  auto val_val = val->codegen(state);
  UNWRAP_EXPECTED(val_val)
  auto val_str = cast_to_str(state, val_val.value());
  UNWRAP_EXPECTED(val_str)
  return val_str.value();
}

std::expected<llvm::Value*, std::string> ConvertToArrayExprAST::codegen(
    CodegenState& state) {
  auto val_val = val->codegen(state);
  UNWRAP_EXPECTED(val_val)

  return state.builder->CreateInsertElement(
      llvm::VectorType::get(llvm::PointerType::get(*state.context, 0),
                            llvm::ElementCount::get(1, false)),
      val_val.value(), uint64_t{0});
}

std::expected<llvm::Value*, std::string> RangeExprAST::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm;

  int64_t first_value_parsed = 0;

  if (first_value.size() == 1 && is_alpha(first_value[0])) {
    first_value_parsed = first_value[0];
  } else {
    first_value_parsed = std::strtoll(first_value.c_str(), nullptr, 10);
  }

  int64_t second_value_parsed = 0;

  if (second_value.size() == 1 && is_alpha(second_value[0])) {
    second_value_parsed = second_value[0];
  } else {
    second_value_parsed = std::strtoll(second_value.c_str(), nullptr, 10);
  }

  auto min = std::min(first_value_parsed, second_value_parsed);
  auto max = std::max(first_value_parsed, second_value_parsed);
  auto abs_step = std::fabs(step);
  for (int64_t i = min; i <= max; i += abs_step) {
    values_llvm.push_back(state.builder->CreateGlobalString(std::to_string(i)));
  }
  if (values_llvm.size() > 256) {
    return std::unexpected("Too many items in range");
  }

  if (values_llvm.size() == 0) {
    return std::unexpected("Cannot make a range without items");
  }

  return llvm::ConstantVector::get(
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
}

std::expected<llvm::Value*, std::string> AssignmentExprAST::codegen(
    CodegenState& state) {
  auto val = value->codegen(state);
  UNWRAP_EXPECTED(val)

  auto val_str = cast_to_str(state, val.value());
  UNWRAP_EXPECTED(val_str)

  auto stored = store_variable_memory(state, identifier, val_str.value());
  UNWRAP_EXPECTED(stored)

  return val_str.value();
}

std::expected<llvm::Value*, std::string> ConcatStringsAST::codegen(
    CodegenState& state) {
  auto first_v = str1->codegen(state);
  UNWRAP_EXPECTED(first_v)
  auto first_len = runtime_strlen(state, first_v.value());
  UNWRAP_EXPECTED(first_len)

  auto second_v = str2->codegen(state);
  UNWRAP_EXPECTED(second_v)
  auto second_len = runtime_strlen(state, second_v.value());
  UNWRAP_EXPECTED(second_len)

  auto total_size =
      state.builder->CreateAdd(first_len.value(), second_len.value());
  auto total_size_null = state.builder->CreateAdd(
      total_size,
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

  auto first_part_ptr = state.builder->CreateAlloca(
      llvm::Type::getInt8Ty(*state.context), total_size_null);

  auto second_part_ptr = state.builder->CreateGEP(
      llvm::Type::getInt8Ty(*state.context), first_part_ptr,
      llvm::ArrayRef<llvm::Value*>(first_len.value()));

  auto end_ptr = state.builder->CreateGEP(
      llvm::Type::getInt8Ty(*state.context), first_part_ptr,
      llvm::ArrayRef<llvm::Value*>(total_size));

  state.builder->CreateMemCpy(first_part_ptr, llvm::MaybeAlign(0),
                              first_v.value(), llvm::MaybeAlign(0),
                              first_len.value());

  state.builder->CreateMemCpy(second_part_ptr, llvm::MaybeAlign(0),
                              second_v.value(), llvm::MaybeAlign(0),
                              second_len.value());
  state.builder->CreateStore(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0),
      end_ptr);

  return first_part_ptr;
}

std::expected<llvm::Value*, std::string> CompoundExprAST::codegen(
    CodegenState& state) {
  if (exprs.size() == 0) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0);
  } else if (exprs.size() == 1) {
    return exprs.front()->codegen(state);
  }

  for (ssize_t i = 0; i < (ssize_t)exprs.size() - 1; i++) {
    auto v = exprs[i]->codegen(state);
    UNWRAP_EXPECTED(v);
  }

  return exprs.back()->codegen(state);
}

std::expected<llvm::Value*, std::string> FunctionDefAST::codegen(
    CodegenState& state) {
  llvm::BasicBlock* return_block = state.builder->GetInsertBlock();

  llvm::Function* function =
      state.module->getFunction(std::format("bash_{}", name));

  if (!function)  // not defined
    return std::unexpected("Function was not predeclared");

  auto named_values = state.named_values;
  state.named_values.clear();

  std::optional<llvm::Value*> variable_memory;
  std::optional<llvm::Value*> argc;
  std::optional<llvm::Value*> argv;
  for (auto& arg : function->args()) {
    switch (arg.getArgNo()) {
      case 0:
        variable_memory = &arg;
        break;
      case 1:
        argc = &arg;
        break;
      case 2:
        argv = &arg;
        break;
      default:
        return std::unexpected("Too many arguments to function");
    }
  }

  if (!variable_memory.has_value() || !argc.has_value() || !argv.has_value()) {
    return std::unexpected("Too little arguments to function");
  }

  state.named_values["variable_memory"] = variable_memory;

  llvm::BasicBlock* entry_block =
      llvm::BasicBlock::Create(*state.context, "entry", function);
  state.builder->SetInsertPoint(entry_block);

  auto stored =
      runtime_store_args_variable_memory(state, argc.value(), argv.value());

  UNWRAP_EXPECTED(stored);
  auto body_code = body->codegen(state);
  UNWRAP_EXPECTED(body_code)

  state.builder->CreateRetVoid();

  state.named_values = named_values;
  state.builder->SetInsertPoint(return_block);

  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0);
}

std::expected<llvm::Value*, std::string> IfAST::codegen(CodegenState& state) {
  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  if (else_val.has_value()) {
    llvm::BasicBlock* ifthen_block =
        llvm::BasicBlock::Create(*state.context, "ifthen", parent_func);
    llvm::BasicBlock* ifelse_block =
        llvm::BasicBlock::Create(*state.context, "ifelse");
    llvm::BasicBlock* merge_block =
        llvm::BasicBlock::Create(*state.context, "fi");

    auto condition_val = condition->codegen(state);
    UNWRAP_EXPECTED(condition_val)

    state.builder->CreateCondBr(condition_val.value(), ifthen_block,
                                ifelse_block);

    state.builder->SetInsertPoint(ifthen_block);

    auto then_code = then_val->codegen(state);
    UNWRAP_EXPECTED(then_code)

    state.builder->CreateBr(merge_block);

    parent_func->insert(parent_func->end(), ifelse_block);
    state.builder->SetInsertPoint(ifelse_block);

    auto else_code = else_val.value()->codegen(state);
    UNWRAP_EXPECTED(else_code)

    state.builder->CreateBr(merge_block);

    parent_func->insert(parent_func->end(), merge_block);
    state.builder->SetInsertPoint(merge_block);
  } else {
    llvm::BasicBlock* ifthen_block =
        llvm::BasicBlock::Create(*state.context, "ifthen", parent_func);
    llvm::BasicBlock* merge_block =
        llvm::BasicBlock::Create(*state.context, "fi");

    auto condition_val = condition->codegen(state);
    UNWRAP_EXPECTED(condition_val)

    state.builder->CreateCondBr(condition_val.value(), ifthen_block,
                                merge_block);

    state.builder->SetInsertPoint(ifthen_block);

    auto then_code = then_val->codegen(state);
    UNWRAP_EXPECTED(then_code)

    state.builder->CreateBr(merge_block);

    parent_func->insert(parent_func->end(), merge_block);
    state.builder->SetInsertPoint(merge_block);
  }

  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0);
}

std::expected<llvm::Value*, std::string> InjectIntoStringAST::codegen(
    CodegenState& state) {
  auto pushed_stack = state.runtime_push_output_stack(1 /* 1 == STRING */);
  UNWRAP_EXPECTED(pushed_stack)
  auto body_val = body->codegen(state);
  UNWRAP_EXPECTED(body_val)

  return state.runtime_pop_output_stack();
}

std::expected<llvm::Value*, std::string> CaseExprAST::codegen(
    CodegenState& state) {
  auto var_gen = var->codegen(state);
  UNWRAP_EXPECTED(var_gen);
  auto var_str = cast_to_str(state, var_gen.value());
  UNWRAP_EXPECTED(var_str);

  llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

  llvm::BasicBlock* merge_block =
      llvm::BasicBlock::Create(*state.context, "esac");

  size_t path_index = 0;
  for (auto& path : condition_map) {
    llvm::BasicBlock* next_case =
        llvm::BasicBlock::Create(*state.context, "next_case");

    std::vector<llvm::BasicBlock*> condition_blocks;

    llvm::BasicBlock* activate =
        llvm::BasicBlock::Create(*state.context, "activate");

    size_t condition_index = 0;
    for (auto& condition : path.first) {
      llvm::BasicBlock* condition_block = llvm::BasicBlock::Create(
          *state.context,
          std::format("condition_{}_{}", path_index, condition_index));

      state.builder->CreateBr(condition_block);

      parent_func->insert(parent_func->end(), condition_block);
      state.builder->SetInsertPoint(condition_block);

      auto condition_gen = condition->codegen(state);
      UNWRAP_EXPECTED(condition_gen)

      auto condition_str = cast_to_str(state, condition_gen.value());
      UNWRAP_EXPECTED(condition_str)

      auto is_eq = runtime_strequals(state, var_str.value(),
                                     condition_str.value(), true);
      UNWRAP_EXPECTED(is_eq)

      llvm::BasicBlock* next_condition =
          llvm::BasicBlock::Create(*state.context, "next_condition");

      state.builder->CreateCondBr(is_eq.value(), activate, next_condition);

      parent_func->insert(parent_func->end(), next_condition);
      state.builder->SetInsertPoint(next_condition);

      condition_index++;
    }

    auto last_next_condition = state.builder->GetInsertBlock();

    parent_func->insert(parent_func->end(), activate);
    state.builder->SetInsertPoint(activate);
    UNWRAP_EXPECTED(path.second->codegen(state));
    state.builder->CreateBr(merge_block);

    state.builder->SetInsertPoint(last_next_condition);

    state.builder->CreateBr(next_case);

    parent_func->insert(parent_func->end(), next_case);
    state.builder->SetInsertPoint(next_case);
    path_index++;
  }

  state.builder->CreateBr(merge_block);

  parent_func->insert(parent_func->end(), merge_block);
  state.builder->SetInsertPoint(merge_block);

  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0);
}

