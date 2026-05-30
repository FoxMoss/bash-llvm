#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ast/ast.h"
#include "codegen.h"
#include "helper.h"
#include "lexer.h"

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
