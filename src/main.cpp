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

std::expected<llvm::Value*, std::string> MathOpExprAST::codegen(
    CodegenState& state) {
  auto left = first->codegen(state);
  UNWRAP_EXPECTED(left)

  auto right = second->codegen(state);
  UNWRAP_EXPECTED(right)

  switch (op) {
    case OP_MOD:
      return state.builder->CreateFRem(left.value(), right.value(), "modtmp");
    case OP_EQ_EQ:
      return state.builder->CreateFCmpUEQ(left.value(), right.value(),
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

  if (!args_codegen.value()->getType()->isArrayTy()) {
    return std::unexpected("Args value not an array");
  }

  auto args_array = static_cast<llvm::ConstantArray*>(args_codegen.value());

  auto args_array_type =
      static_cast<llvm::ArrayType*>(args_codegen.value()->getType());

  if (args_array == nullptr || args_array_type == nullptr) {
    return std::unexpected("Args value not an array");
  }

  std::vector<llvm::Value*> arg_values = {
      llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(*state.context),
                             args_array_type->getNumElements()),
      args_codegen.value()};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFCmpOGT(
        val, llvm::ConstantFP::get(*state.context, llvm::APFloat(0.0)));
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

  auto short_path =
      llvm::BasicBlock::Create(*state.context, "short", parent_func);

  auto both_path = llvm::BasicBlock::Create(*state.context, "both");
  auto merge = llvm::BasicBlock::Create(*state.context, "merge");

  auto left_full = first->codegen(state);
  UNWRAP_EXPECTED(left_full)

  auto left = reduce_to_bool(state, left_full.value());
  UNWRAP_EXPECTED(left)

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

      phinode->addIncoming(left.value(), both_path);
      phinode->addIncoming(right.value(), short_path);
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

  return llvm::ConstantArray::get(
      llvm::ArrayType::get(values_llvm.front()->getType(), values_llvm.size()),
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
}

std::expected<llvm::Value*, std::string> ConcatExprAST::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm;

  auto first_array = first->codegen(state);
  UNWRAP_EXPECTED(first_array)

  if (!first_array.value()->getType()->isArrayTy()) {
    return std::unexpected("First value not an array");
  }

  auto first_constant = static_cast<llvm::ConstantArray*>(first_array.value());

  auto first_array_type =
      static_cast<llvm::ArrayType*>(first_array.value()->getType());

  if (first_constant == nullptr || first_array_type == nullptr) {
    return std::unexpected("First value not an array");
  }

  for (uint64_t i = 0; i < first_array_type->getNumElements(); i++) {
    auto value =
        state.builder->CreateExtractElement(first_constant, uint64_t{0});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    values_llvm.push_back(static_value);
  }

  auto second_array = second->codegen(state);
  UNWRAP_EXPECTED(second_array)

  if (!second_array.value()->getType()->isArrayTy()) {
    return std::unexpected("second value not an array");
  }

  auto second_constant =
      static_cast<llvm::ConstantArray*>(second_array.value());

  auto second_array_type =
      static_cast<llvm::ArrayType*>(second_array.value()->getType());

  if (second_constant == nullptr || second_array_type == nullptr) {
    return std::unexpected("second value not an array");
  }

  for (uint64_t i = 0; i < second_array_type->getNumElements(); i++) {
    auto value =
        state.builder->CreateExtractElement(second_constant, uint64_t{0});
    auto static_value = static_cast<llvm::Constant*>(value);
    if (static_value == nullptr) {
      continue;
    }

    values_llvm.push_back(static_value);
  }

  return llvm::ConstantArray::get(
      llvm::ArrayType::get(values_llvm.front()->getType(), values_llvm.size()),
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
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
  for (double i = min; i < max; i += abs_step) {
    values_llvm.push_back(state.builder->CreateGlobalString(std::to_string(i)));
  }

  if (values_llvm.size() == 0) {
    return std::unexpected("Cannot make a range without items");
  }

  return llvm::ConstantArray::get(
      llvm::ArrayType::get(values_llvm.front()->getType(), values_llvm.size()),
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

  if (!range_array.value()->getType()->isArrayTy()) {
    return std::unexpected("Range is not an array");
  }

  auto range_constant = static_cast<llvm::ConstantArray*>(range_array.value());

  auto range_array_type =
      static_cast<llvm::ArrayType*>(range_array.value()->getType());

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
  variable->addIncoming(next_var, body_block);

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
  state.builder->CreateRet(
      llvm::Constant::getNullValue(llvm::Type::getVoidTy(*state.context)));

  std::error_code error;
  llvm::raw_fd_ostream out_file("out.ll", error);
  // std::ofstream out_file("out.ll");
  state.module->print(out_file, NULL);

  return 0;
}
