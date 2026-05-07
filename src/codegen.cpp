#include "codegen.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
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

#include "ast.h"
#include "codegen.h"
#include "lexer.h"

#define UNWRAP_EXPECTED(val) \
  if (!val.has_value()) return std::unexpected(val.error());

std::expected<llvm::Value*, std::string> NumericExprAST::codegen(
    CodegenState& state) {
  return llvm::ConstantFP::get(*state.context, llvm::APFloat(value));
}
std::expected<llvm::Value*, std::string> StringExprAST::codegen(
    CodegenState& state) {
  return state.builder->CreateGlobalString(val);
}
std::expected<llvm::Value*, std::string> store_variable_memory(
    CodegenState& state, std::string name, llvm::Value* value) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("store_variable_memory");
  if (!program_called) return std::unexpected("Variable getter does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 5)
    return std::unexpected("Helper store_variable_memory is illdefined");

  auto str_length = runtime_strlen(state, value);
  UNWRAP_EXPECTED(str_length)

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      state.builder->CreateGlobalString(name),
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                             name.size()),
      value, str_length.value()};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> get_variable_memory(
    CodegenState& state, std::string name) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("get_variable_memory");
  if (!program_called) return std::unexpected("Variable getter does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Helper get_variable_memory is illdefined");

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      state.builder->CreateGlobalString(name),
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                             name.size())};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> IdentifierExprAST::codegen(
    CodegenState& state) {
  return get_variable_memory(state, name);
}

std::expected<llvm::Value*, std::string> runtime_strlen(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isPointerTy()) {
    llvm::Function* program_called = state.module->getFunction("str_to_len");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program str_to_len is illdefined");

    std::vector<llvm::Value*> arg_values = {val};

    return state.builder->CreateCall(program_called, arg_values);

  } else {
    return std::unexpected("Could not get strlen");
  }
}

std::expected<llvm::Value*, std::string> cast_to_int(CodegenState& state,
                                                     llvm::Value* val) {
  if (val->getType()->isIntegerTy()) {
    return val;
  } else if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFPToSI(val,
                                       llvm::Type::getInt64Ty(*state.context));
  }
  return std::unexpected("Could not reduce to int");
}

std::expected<llvm::Value*, std::string> cast_to_str(CodegenState& state,
                                                     llvm::Value* val) {
  if (val->getType()->isPointerTy()) {
    return val;
  } else if (val->getType()->isFloatingPointTy() ||
             val->getType()->isIntegerTy()) {
    auto int_val = cast_to_int(state, val);
    UNWRAP_EXPECTED(int_val)

    llvm::Function* program_called = state.module->getFunction("int_len");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program int_len is illdefined");

    std::vector<llvm::Value*> arg_values = {int_val.value()};

    auto int_len = state.builder->CreateCall(program_called, arg_values);

    auto int_len_padded = state.builder->CreateAdd(
        int_len,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    auto stack_str = state.builder->CreateAlloca(
        llvm::Type::getInt8Ty(*state.context), int_len_padded);

    program_called = state.module->getFunction("int_to_str");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 3)
      return std::unexpected("Program int_to_str is illdefined");

    arg_values = {int_val.value(), stack_str, int_len_padded};

    state.builder->CreateCall(program_called, arg_values);
    return stack_str;
  }
  return std::unexpected("Type not support for str conversion");
}

std::expected<llvm::Value*, std::string> cast_to_float(CodegenState& state,
                                                       llvm::Value* val) {
  if (val->getType()->isIntegerTy()) {
    return state.builder->CreateSIToFP(val,
                                       llvm::Type::getFloatTy(*state.context));
  } else if (val->getType()->isFloatTy()) {
    return val;
  } else if (val->getType()->isDoubleTy()) {
    return state.builder->CreateFPCast(val,
                                       llvm::Type::getFloatTy(*state.context));
  } else if (val->getType()->isPointerTy()) {
    llvm::Function* program_called = state.module->getFunction("str_to_float");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program str_to_float is illdefined");

    std::vector<llvm::Value*> arg_values = {val};

    return state.builder->CreateCall(program_called, arg_values);

    // hand written str to float parsing
    // dragons be ware
#if 0
    llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* header = state.builder->GetInsertBlock();

    auto length_search =
        llvm::BasicBlock::Create(*state.context, "length_search", parent_func);
    auto increment = llvm::BasicBlock::Create(*state.context, "increment");
    auto merge = llvm::BasicBlock::Create(*state.context, "merge");

    state.builder->CreateBr(length_search);

    state.builder->SetInsertPoint(length_search);

    llvm::PHINode* length = state.builder->CreatePHI(
        llvm::Type::getInt64Ty(*state.context), 2, "length");
    length->addIncoming(
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0),
        header);

    auto search_ptr = state.builder->CreateIntToPtr(
        state.builder->CreateAdd(
            state.builder->CreatePtrToInt(
                val, llvm::Type::getInt64Ty(*state.context)),
            length),
        llvm::PointerType::get(*state.context, 0));

    auto searched_val = state.builder->CreateLoad(
        llvm::Type::getInt8Ty(*state.context), search_ptr);

    auto was_null = state.builder->CreateICmpEQ(
        searched_val,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), 0));

    auto next_length_test = state.builder->CreateAdd(
        length,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    length->addIncoming(next_length_test, length_search);

    state.builder->CreateCondBr(was_null, increment, length_search);

    length_search = state.builder->GetInsertBlock();

    parent_func->insert(parent_func->end(), increment);
    state.builder->SetInsertPoint(increment);

    llvm::PHINode* index = state.builder->CreatePHI(
        llvm::Type::getInt64Ty(*state.context), 2, "index");
    index->addIncoming(
        // 2 just to offset the fact that we'll be 2 off from the lowest digit
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0),
        length_search);

    llvm::PHINode* return_value = state.builder->CreatePHI(
        llvm::Type::getFloatTy(*state.context), 2, "interpreted_value");
    return_value->addIncoming(
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 0),
        length_search);

    llvm::PHINode* power_to = state.builder->CreatePHI(
        llvm::Type::getFloatTy(*state.context), 2, "power_to");
    power_to->addIncoming(
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 1),
        length_search);

    auto found_char = state.builder->CreateLoad(
        llvm::Type::getInt8Ty(*state.context),
        state.builder->CreateIntToPtr(
            state.builder->CreateSub(

                state.builder->CreateSub(
                    state.builder->CreateAdd(
                        state.builder->CreatePtrToInt(
                            val, llvm::Type::getInt64Ty(*state.context)),
                        length),
                    index),
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                                       1)),
            llvm::PointerType::get(*state.context, 0)));

    auto char_int_val = state.builder->CreateUIToFP(
        state.builder->CreateSub(
            found_char,
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), '0')),
        llvm::Type::getFloatTy(*state.context));

    auto new_return = state.builder->CreateFAdd(
        state.builder->CreateFMul(char_int_val, power_to), return_value);
    return_value->addIncoming(new_return, increment);

    auto new_power = state.builder->CreateFMul(
        power_to,
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 10));

    power_to->addIncoming(new_power, increment);

    auto reached_end = state.builder->CreateICmpEQ(index, length);

    auto new_index = state.builder->CreateAdd(
        index,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    index->addIncoming(new_index, increment);

    state.builder->CreateCondBr(reached_end, merge, increment);

    increment = state.builder->GetInsertBlock();

    parent_func->insert(parent_func->end(), merge);
    state.builder->SetInsertPoint(merge);

    return new_return;
#endif
  } else {
    return std::unexpected("Could not reduce to float");
  }
}

std::expected<llvm::Value*, std::string> MathSingleOpExprAST::codegen(
    CodegenState& state) {
  auto ident_name = first->get_ident_str();

  if (!ident_name.has_value()) {
    return std::unexpected(
        "Single argument operations need a valid identifier");
  }

  switch (op) {
    case OP_INC: {
      auto ident_value = get_variable_memory(state, ident_name.value());
      UNWRAP_EXPECTED(ident_value)
      auto ident_float = cast_to_float(state, ident_value.value());
      UNWRAP_EXPECTED(ident_float)

      auto incremented = state.builder->CreateFAdd(
          ident_float.value(),
          llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 1.0),
          "inctmp");

      auto incremented_str = cast_to_str(state, incremented);
      UNWRAP_EXPECTED(incremented_str)

      auto stored = store_variable_memory(state, ident_name.value(),
                                          incremented_str.value());
      UNWRAP_EXPECTED(stored)

      return ident_value.value();  // ++ returns original
    } break;
    case OP_DEC: {
      auto ident_value = get_variable_memory(state, ident_name.value());
      UNWRAP_EXPECTED(ident_value)
      auto ident_float = cast_to_float(state, ident_value.value());
      UNWRAP_EXPECTED(ident_float)

      auto incremented = state.builder->CreateFSub(
          ident_float.value(),
          llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 1.0),
          "dectmp");

      auto incremented_str = cast_to_str(state, incremented);
      UNWRAP_EXPECTED(incremented_str)

      auto stored = store_variable_memory(state, ident_name.value(),
                                          incremented_str.value());
      UNWRAP_EXPECTED(stored)

      return ident_value.value();  // -- returns original
    } break;
    default:
      break;
  }
  return std::unexpected("Single argument operation does not support operator");
}

std::expected<llvm::Value*, std::string> MathOpExprAST::codegen(
    CodegenState& state) {
  auto left = first->codegen(state);
  UNWRAP_EXPECTED(left)

  auto left_val = cast_to_float(state, left.value());
  UNWRAP_EXPECTED(left_val)

  auto right = second->codegen(state);
  UNWRAP_EXPECTED(right)

  auto right_val = cast_to_float(state, right.value());
  UNWRAP_EXPECTED(right_val)

  switch (op) {
    case OP_MOD:
      return state.builder->CreateFRem(left_val.value(), right_val.value(),
                                       "modtmp");
    case OP_EQ_EQ:
      return state.builder->CreateFCmpUEQ(left_val.value(), right_val.value(),
                                          "eqeqtmp");
    case OP_ADD:
      return state.builder->CreateFAdd(left_val.value(), right_val.value(),
                                       "addtmp");
    case OP_SUB:
      return state.builder->CreateFSub(left_val.value(), right_val.value(),
                                       "subtmp");
    case OP_MUL:
      return state.builder->CreateFMul(left_val.value(), right_val.value(),
                                       "multmp");
    case OP_DIV:
      return state.builder->CreateFDiv(left_val.value(), right_val.value(),
                                       "divtmp");
    case OP_GT:
      return state.builder->CreateFCmpUGT(left_val.value(), right_val.value(),
                                          "gttmp");
    case OP_LT:
      return state.builder->CreateFCmpULT(left_val.value(), right_val.value(),
                                          "lttmp");
    case OP_LE:
      return state.builder->CreateFCmpULE(left_val.value(), right_val.value(),
                                          "letmp");
    case OP_GE:
      return state.builder->CreateFCmpUGE(left_val.value(), right_val.value(),
                                          "getmp");
    case OP_NOT_EQ:
      return state.builder->CreateNot(state.builder->CreateFCmpUEQ(
          left_val.value(), right_val.value(), "noteqmp"));

    case OP_PLUS_EQ: {
      auto ident_name = first->get_ident_str();

      if (!ident_name.has_value()) {
        return std::unexpected(
            "Plus equals need a valid identifier first argument");
      }

      auto added_val = state.builder->CreateFAdd(
          left_val.value(), right_val.value(), "pluseqtmp");

      auto incremented_str = cast_to_str(state, added_val);
      UNWRAP_EXPECTED(incremented_str)

      auto stored = store_variable_memory(state, ident_name.value(),
                                          incremented_str.value());
      UNWRAP_EXPECTED(stored)

      return added_val;
    }
    case OP_MINUS_EQ: {
      auto ident_name = first->get_ident_str();

      if (!ident_name.has_value()) {
        return std::unexpected(
            "Minus equals need a valid identifier first argument");
      }

      auto added_val = state.builder->CreateFSub(
          left_val.value(), right_val.value(), "minuseqtmp");

      auto incremented_str = cast_to_str(state, added_val);
      UNWRAP_EXPECTED(incremented_str)

      auto stored = store_variable_memory(state, ident_name.value(),
                                          incremented_str.value());
      UNWRAP_EXPECTED(stored)

      return added_val;
    }
    case OP_EQ: {
      auto ident_name = first->get_ident_str();

      if (!ident_name.has_value()) {
        return std::unexpected("Equals need a valid identifier first argument");
      }

      auto right_str = cast_to_str(state, right_val.value());
      UNWRAP_EXPECTED(right_str)

      auto stored =
          store_variable_memory(state, ident_name.value(), right_str.value());
      UNWRAP_EXPECTED(stored)

      return right_str;
    }

    default:
    case OP_UNK:
      return std::unexpected("Math operator is unknown");
  }
  std::unreachable();
}

std::expected<llvm::Value*, std::string> runtime_store_args_variable_memory(
    CodegenState& state, llvm::Value* argc, llvm::Value* argv) {
  llvm::Function* program_called =
      state.module->getFunction("store_args_variable_memory");
  if (!program_called)
    return std::unexpected("store_args_variable_memory not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Program store_args_variable_memory is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(), argc, argv};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> CallExprAST::codegen(
    CodegenState& state) {
  llvm::Function* program_called =
      state.module->getFunction(std::format("bash_{}", program));
  if (!program_called) return std::unexpected("Unknown function referenced");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Program " + program + " is illdefined");

  auto args_codegen = args->codegen(state);
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

  state.builder->CreateStore(args_array, stack_args);

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(*state.context),
                             args_array_type->getNumElements()),
      stack_args};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFCmpOGT(
        val, llvm::ConstantFP::get(*state.context, llvm::APFloat(0.0)));
  } else if (val->getType()->isIntegerTy(1)) {
    return val;
  } else if (val->getType()->isIntegerTy()) {
    return state.builder->CreateICmpSGT(
        val, llvm::ConstantInt::get(val->getType(), 0));
  } else if (val->getType()->isVoidTy()) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*state.context), 1);
  } else if (val->getType()->isPointerTy()) {
    auto len = runtime_strlen(state, val);
    UNWRAP_EXPECTED(len)

    return state.builder->CreateICmpSGT(
        len.value(), llvm::ConstantInt::get(len.value()->getType(), 0));
  }

  return std::unexpected("Couldn't reduce to bool");
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
      return std::unexpected("Range value not constant");
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
    return std::unexpected("Could not make vector");
  }

  return val.value();
}

std::expected<llvm::Value*, std::string> ConcatExprAST::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm;

  auto first_array = first->codegen(state);
  UNWRAP_EXPECTED(first_array)

  if (!first_array.value()->getType()->isVectorTy()) {
    return std::unexpected("First value not an array");
  }

  auto first_constant = static_cast<llvm::ConstantVector*>(first_array.value());

  auto first_array_type =
      static_cast<llvm::FixedVectorType*>(first_array.value()->getType());

  if (first_constant == nullptr || first_array_type == nullptr) {
    return std::unexpected("First value not an array");
  }

  auto second_array = second->codegen(state);
  UNWRAP_EXPECTED(second_array)

  if (!second_array.value()->getType()->isVectorTy()) {
    return std::unexpected("Second value not an vector");
  }

  auto second_constant =
      static_cast<llvm::ConstantVector*>(second_array.value());

  auto second_array_type =
      static_cast<llvm::FixedVectorType*>(second_array.value()->getType());

  if (second_constant == nullptr || second_array_type == nullptr) {
    return std::unexpected("Second value not an array");
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
    return std::unexpected("Could not concatate ranges");
  }

  return val.value();
}

std::expected<llvm::Value*, std::string> ConvertToRangeArrayExprAST::codegen(
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
    first_value_parsed = std::strtoll(first_value.c_str(), NULL, 10);
  }

  int64_t second_value_parsed = 0;

  if (second_value.size() == 1 && is_alpha(second_value[0])) {
    second_value_parsed = second_value[0];
  } else {
    second_value_parsed = std::strtoll(second_value.c_str(), NULL, 10);
  }

  auto min = std::min(first_value_parsed, second_value_parsed);
  auto max = std::max(first_value_parsed, second_value_parsed);
  auto abs_step = std::fabs(step);
  for (int64_t i = min; i <= max; i += abs_step) {
    values_llvm.push_back(state.builder->CreateGlobalString(std::to_string(i)));
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
    case CONDITION_GT:
      return state.builder->CreateFCmpOGT(first_f.value(), second_f.value());
    case CONDITION_LT:
      return state.builder->CreateFCmpOLT(first_f.value(), second_f.value());
  }
  std::unreachable();
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
    return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
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

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
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

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
}

std::expected<void, std::string> runtime_push_output_stack(
    CodegenState& state, uint16_t location_type) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("push_output_stack");
  if (!program_called)
    return std::unexpected("push_output_stack does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 2)
    return std::unexpected("Helper push_output_stack is illdefined");

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      llvm::ConstantInt::get(llvm::Type::getInt16Ty(*state.context),
                             location_type)};

  state.builder->CreateCall(program_called, arg_values);
  return std::expected<void, std::string>();
}
std::expected<llvm::Value*, std::string> runtime_pop_output_stack(
    CodegenState& state) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("pop_output_stack");
  if (!program_called)
    return std::unexpected("pop_output_stack does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected("Helper pop_output_stack is illdefined");

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value()};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> InjectIntoStringAST::codegen(
    CodegenState& state) {
  auto pushed_stack = runtime_push_output_stack(state, 1 /* 1 == OUTPUT_STR */);
  UNWRAP_EXPECTED(pushed_stack)
  auto body_val = body->codegen(state);
  UNWRAP_EXPECTED(body_val)

  return runtime_pop_output_stack(state);
}

