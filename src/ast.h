#pragma once
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

#include "codegen.h"
#include "lexer.h"
#include "llvm/IR/DerivedTypes.h"

class ExprAST {
 public:
  virtual ~ExprAST() = default;

  virtual void print_name(ssize_t level) {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("ExprAST\n");
  }

  virtual std::expected<llvm::Value*, std::string> codegen(CodegenState&) {
    return std::unexpected("Not implemented\n");
  };
};

class StringExprAST : public ExprAST {
 public:
  std::string val;

  StringExprAST(const std::string& val) : val(val) {}
  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("StringExprAST \"{}\"\n", val);
  }

  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class CallExprAST : public ExprAST {
  std::string program;
  std::unique_ptr<ExprAST> args;

 public:
  CallExprAST(std::string program, std::unique_ptr<ExprAST> args)
      : program(program), args(std::move(args)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("CallExprAST \"{}\"\n", program);

    args->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class UnknownExprAST : public ExprAST {
 public:
  UnknownExprAST() {}
};

class IdentifierExprAST : public ExprAST {
  std::string name;

 public:
  IdentifierExprAST(const std::string& name) : name(name) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("IdentifierExprAST \"{}\"\n", name);
  }

  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};
class NumericExprAST : public ExprAST {
  double value;

 public:
  NumericExprAST(const double& value) : value(value) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("NumericExprAST {}\n", value);
  }

  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class StatementOpExprAST : public ExprAST {
 public:
#define OP(op) op,
  enum StatementOp {
#include "statementops.inc"
  };
#undef OP

  static std::string get_op_name(StatementOp op) {
#define OP(x) \
  case x:     \
    return #x;
    switch (op) {
#include "statementops.inc"
    }
#undef TOKEN
    std::unreachable();
  }

  StatementOpExprAST(StatementOp op, std::unique_ptr<ExprAST> first,
                     std::unique_ptr<ExprAST> second)
      : op(op), first(std::move(first)), second(std::move(second)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("StatementOpExprAST {}\n", get_op_name(op));

    first->print_name(level + 1);
    second->print_name(level + 1);
  }

  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;

 private:
  StatementOp op;
  std::unique_ptr<ExprAST> first;
  std::unique_ptr<ExprAST> second;
};

class MathOpExprAST : public ExprAST {
  MathOp op;
  std::unique_ptr<ExprAST> first;
  std::unique_ptr<ExprAST> second;

 public:
  MathOpExprAST(MathOp op, std::unique_ptr<ExprAST> first,
                std::unique_ptr<ExprAST> second)
      : op(op), first(std::move(first)), second(std::move(second)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("MathOpExprAST {}\n", math_op_to_string(op));

    first->print_name(level + 1);
    second->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class RangeArrayExprAST : public ExprAST {
  std::vector<std::unique_ptr<ExprAST>> values;

 public:
  RangeArrayExprAST(std::vector<std::unique_ptr<ExprAST>>& c_values) {
    for (auto& c_val : c_values) {
      values.push_back(std::move(c_val));
    }
  }

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("RangeArrayExprAST\n");

    for (auto& val : values) {
      val->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};
class ConvertToRangeArrayExprAST : public ExprAST {
  std::unique_ptr<ExprAST> val;

 public:
  ConvertToRangeArrayExprAST(std::unique_ptr<ExprAST> val)
      : val(std::move(val)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConvertToRangeArrayExprAST\n");
    val->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ConcatExprAST : public ExprAST {
  std::unique_ptr<ExprAST> first;
  std::unique_ptr<ExprAST> second;

 public:
  ConcatExprAST(std::unique_ptr<ExprAST> first, std::unique_ptr<ExprAST> second)
      : first(std::move(first)), second(std::move(second)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConcatExprAST\n");
    first->print_name(level + 1);
    second->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class RangeExprAST : public ExprAST {
  std::string first_value;
  std::string second_value;
  int32_t step;

 public:
  RangeExprAST(const std::string& first_value, const std::string& second_value,
               const int32_t& step)
      : first_value(first_value), second_value(second_value), step(step) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("RangeExprAST {}..{} +{}\n", first_value, second_value, step);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class AssignmentExprAST : public ExprAST {
  std::string identifier;
  std::unique_ptr<ExprAST> value;

 public:
  AssignmentExprAST(std::string identifier, std::unique_ptr<ExprAST> value)
      : identifier(std::move(identifier)), value(std::move(value)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("AssignmentExprAST \"{}\"\n", identifier);

    value->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class CompoundExprAST : public ExprAST {
  std::vector<std::unique_ptr<ExprAST>> exprs;

 public:
  CompoundExprAST(std::vector<std::unique_ptr<ExprAST>> exprs)
      : exprs(std::move(exprs)) {}

  void push(std::unique_ptr<ExprAST> expr) { exprs.push_back(std::move(expr)); }

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("CompoundExprAST\n");

    for (auto& expr : exprs) {
      expr->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ForAST : public ExprAST {
  std::string index;
  std::unique_ptr<ExprAST> range;
  std::unique_ptr<ExprAST> body;

 public:
  ForAST(std::string index, std::unique_ptr<ExprAST> range,
         std::unique_ptr<ExprAST> body)
      : index(std::move(index)),
        range(std::move(range)),
        body(std::move(body)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ForAST {} in\n", index);

    range->print_name(level + 1);
    body->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ConditionExprAST : public ExprAST {
 public:
  enum ConditonOperator {
    CONDITION_LT,
    CONDITION_GT,
    CONDITION_EQ,
  };

 private:
  std::unique_ptr<ExprAST> first_var;
  ConditonOperator op;
  std::unique_ptr<ExprAST> second_var;

 public:
  ConditionExprAST(std::unique_ptr<ExprAST> first_var, ConditonOperator op,
                   std::unique_ptr<ExprAST> second_var)
      : first_var(std::move(first_var)),
        op(op),
        second_var(std::move(second_var)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConditionExprAST {}\n", (int)op);

    first_var->print_name(level + 1);
    second_var->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class WhileAST : public ExprAST {
  std::unique_ptr<ExprAST> condition;
  std::unique_ptr<ExprAST> body;

 public:
  WhileAST(std::unique_ptr<ExprAST> condition, std::unique_ptr<ExprAST> body)
      : condition(std::move(condition)), body(std::move(body)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("WhileAST\n");

    condition->print_name(level + 1);
    body->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ConcatStringsAST : public ExprAST {
  std::unique_ptr<ExprAST> str1;
  std::unique_ptr<ExprAST> str2;

 public:
  ConcatStringsAST(std::unique_ptr<ExprAST> str1, std::unique_ptr<ExprAST> str2)
      : str1(std::move(str1)), str2(std::move(str2)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConcatStringsAST\n");

    str1->print_name(level + 1);
    str2->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

std::optional<std::unique_ptr<ExprAST>> parse_compound_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_paren_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_operator_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    int lhs_prec, std::unique_ptr<ExprAST> lefthandside);
