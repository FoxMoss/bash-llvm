#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "codegen.h"
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
  llvm::Value* v = state.named_values[name];

  if (!v) return std::unexpected("Unknown variable name");
  return v;
}

#define UNWRAP_EXPECTED(val) \
  if (!val.has_value()) return std::unexpected(val.error());

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
    case OP_UNK:
      return std::unexpected("Math operator is unknown");
  }
  std::unreachable();
}

std::expected<llvm::Value*, std::string> CallExprAST::codegen(
    CodegenState& state) {
  llvm::Function* program_called = state.module->getFunction(program);
  if (!program_called) return std::unexpected("Unknown function referenced");

  // If argument mismatch error.
  if (program_called->arg_size() != 2)
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

  std::vector<llvm::Value*> arg_values = {
      llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(*state.context),
                             args_array_type->getNumElements()),
      stack_args};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isFloatingPointTy()) {
    printf("is float %i \n", val->getType()->isIntegerTy());
    return state.builder->CreateFCmpOGT(
        val, llvm::ConstantFP::get(*state.context, llvm::APFloat(0.0)));
  } else if (val->getType()->isIntegerTy(1)) {
    return val;
  } else if (val->getType()->isIntegerTy()) {
    return state.builder->CreateICmpSGT(
        val, llvm::ConstantInt::get(val->getType(), 0));
  } else if (val->getType()->isVoidTy()) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*state.context), 1);
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
      llvm::BasicBlock::Create(*state.context, "short", parent_func);

  auto both_path = llvm::BasicBlock::Create(*state.context, "both");
  auto merge = llvm::BasicBlock::Create(*state.context, "merge");

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
  for (auto member : values) {
    values_llvm.push_back(state.builder->CreateGlobalString(member));
  }

  return llvm::ConstantVector::get(
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
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

  for (uint64_t i = 0; i < first_array_type->getNumElements(); i++) {
    auto value =
        state.builder->CreateExtractElement(first_constant, uint64_t{i});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    values_llvm.push_back(static_value);
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

  for (uint64_t i = 0; i < second_array_type->getNumElements(); i++) {
    auto value =
        state.builder->CreateExtractElement(second_constant, uint64_t{i});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    values_llvm.push_back(static_value);
  }

  return llvm::ConstantVector::get(
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
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
  return state.named_values[identifier] = val.value();
}

std::expected<llvm::Value*, std::string> ForAST::codegen(CodegenState& state) {
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

  auto loop = llvm::BasicBlock::Create(*state.context, "loop", parent_func);
  auto body_block = llvm::BasicBlock::Create(*state.context, "body");
  auto merge = llvm::BasicBlock::Create(*state.context, "merge");

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

  state.named_values[index] = variable_state;

  auto body_value = body->codegen(state);
  UNWRAP_EXPECTED(body_value)

  auto next_var = state.builder->CreateAdd(variable, step);
  variable->addIncoming(next_var, state.builder->GetInsertBlock());

  state.builder->CreateBr(loop);

  parent_func->insert(parent_func->end(), merge);
  state.builder->SetInsertPoint(merge);

  return llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context));
}

int main() {
  std::string source_file =
      "for n in {1..100}; do\n  ((( n % 15 == 0 )) && echo "
      "'FizzBuzz') ||\n  "
      "((( n % 5 == 0 )) && echo 'Buzz') ||\n  ((( n % 3 == 0 )) && echo "
      "'Fizz') ||\n  echo $n;\n done";
  size_t cursor = 0;

  std::optional<BashLexerSegment> last_token;
  std::vector<BashLexerSegment> lexer_segments;
  ParenMap paren_map;

  do {
    last_token = BashLexerSegment::munch_token(
        source_file, cursor,
        last_token.has_value() ? last_token->token : TOK_UNK, paren_map);

    // must have value so we don't need to check
    lexer_segments.push_back(last_token.value());
  } while (last_token->token != TOK_EOF);

  lexer_segments = paren_map_fusing(lexer_segments, paren_map);

  for (auto token : lexer_segments) {
    std::print("[{}] {}\n", token.str, token.get_token_name());
  }

  size_t ast_cursor = 0;
  auto base = parse_expression(lexer_segments, ast_cursor);
  base.value()->print_name(0);

  CodegenState state;
  auto value = base.value()->codegen(state);
  if (!value.has_value()) {
    std::print("Error: {}\n", value.error());
    return 1;
  }
  state.builder->CreateRetVoid();

  std::error_code error;
  llvm::raw_fd_ostream out_file("out.ll", error);
  // std::ofstream out_file("out.ll");
  state.module->print(out_file, NULL);

  return 0;
}
