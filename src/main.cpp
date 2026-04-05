#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
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
}

std::expected<llvm::Value*, std::string> CallExprAST::codegen(
    CodegenState& state) {
  llvm::Function* program_called = state.module->getFunction(program);
  if (!program_called) return std::unexpected("Unknown function referenced");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected("Program " + program + " is illdefined");

  auto args_codegen = args->codegen(state);
  UNWRAP_EXPECTED(args_codegen)
  std::vector<llvm::Value*> arg_values = {args_codegen.value()};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFCmpOGT(
        val, llvm::ConstantFP::get(*state.context, llvm::APFloat(0.0)));
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
          llvm::Type::getDoubleTy(*state.context), 2, "andtmp");

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
          llvm::Type::getDoubleTy(*state.context), 2, "ortmp");

      phinode->addIncoming(left.value(), both_path);
      phinode->addIncoming(right.value(), short_path);
      return phinode;
    };
    case STATEMENT_OP_UNK:
      return std::unexpected("Statment operator unknown");
  }
}

std::expected<llvm::Value*, std::string> RangeArrayExprAST ::codegen(
    CodegenState& state) {
  std::vector<llvm::Constant*> values_llvm(values.size(), {});

  llvm::ConstantDataArray::get(
      state.context,
      llvm::ArrayRef<llvm::Constant*>(values_llvm.data(), values_llvm.size()));
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

  return 0;
}
