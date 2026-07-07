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

  virtual std::optional<std::string> get_ident_str() { return {}; }
  virtual std::vector<std::string> get_functions_defined() { return {}; }
};

class StringExprAST : public ExprAST {
 public:
  std::string val;

  StringExprAST(std::string val) : val(std::move(val)) {}
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
  std::optional<std::unique_ptr<ExprAST>> args;

 public:
  CallExprAST(std::string program, std::unique_ptr<ExprAST> args)
      : program(std::move(program)), args(std::move(args)) {}
  CallExprAST(std::string program) : program(std::move(program)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("CallExprAST \"{}\"\n", program);

    if (args.has_value()) {
      args->get()->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class UnknownExprAST : public ExprAST {
 public:
  UnknownExprAST() = default;
};

class IdentifierExprAST : public ExprAST {
 public:
  // name is public incase we need to do manipulation
  std::string name;

  IdentifierExprAST(std::string name) : name(std::move(name)) {}

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
  std::optional<std::string> get_ident_str() override { return name; }
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

class PipeExprAST : public ExprAST {
 public:
  PipeExprAST(std::unique_ptr<ExprAST> first, std::unique_ptr<ExprAST> second)
      : first(std::move(first)), second(std::move(second)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("PipeExprAST\n");

    first->print_name(level + 1);
    second->print_name(level + 1);
  }

  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;

 private:
  std::unique_ptr<ExprAST> first;
  std::unique_ptr<ExprAST> second;
};

class StatementOpExprAST : public ExprAST {
 public:
#define OP(op) op,
  enum StatementOp {
#include "statementops.inc"
  };
#undef OP

  static std::string get_op_name(StatementOp op);
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
class MathSingleOpExprAST : public ExprAST {
  MathOp op;
  std::unique_ptr<ExprAST> first;

 public:
  MathSingleOpExprAST(MathOp op, std::unique_ptr<ExprAST> first)
      : op(op), first(std::move(first)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }
    std::print("MathSingleOpExprAST {}\n", math_op_to_string(op));

    first->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
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
class ConvertToStringExprAST : public ExprAST {
  std::unique_ptr<ExprAST> val;

 public:
  ConvertToStringExprAST(std::unique_ptr<ExprAST> val) : val(std::move(val)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConvertToArrayExprAST\n");
    val->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ConvertToArrayExprAST : public ExprAST {
  std::unique_ptr<ExprAST> val;

 public:
  ConvertToArrayExprAST(std::unique_ptr<ExprAST> val) : val(std::move(val)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ConvertToArrayExprAST\n");
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
  RangeExprAST(std::string first_value, std::string second_value,
               const int32_t& step)
      : first_value(std::move(first_value)),
        second_value(std::move(second_value)),
        step(step) {}

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
  std::vector<std::string> get_functions_defined() override {
    std::vector<std::string> ret;
    for (auto& expr : exprs) {
      auto expr_funcs = expr->get_functions_defined();
      ret.insert(ret.end(), expr_funcs.begin(), expr_funcs.end());
    }
    return ret;
  }
};

class ForExprAST : public ExprAST {
 public:
  ForExprAST() = default;
  void virtual add_body(std::unique_ptr<ExprAST>) {}
};

class CStyleForExprAST : public ForExprAST {
  std::unique_ptr<ExprAST> assignment;
  std::unique_ptr<ExprAST> check;
  std::unique_ptr<ExprAST> increment;

  std::optional<std::unique_ptr<ExprAST>> body;

 public:
  CStyleForExprAST(std::unique_ptr<ExprAST> assignment,
                   std::unique_ptr<ExprAST> check,
                   std::unique_ptr<ExprAST> increment)
      : assignment(std::move(assignment)),
        check(std::move(check)),
        increment(std::move(increment)) {}

  void add_body(std::unique_ptr<ExprAST> c_body) override {
    body = std::move(c_body);
  }

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("CStyleForInExprAST \n");

    assignment->print_name(level + 1);
    check->print_name(level + 1);
    increment->print_name(level + 1);

    if (body.has_value()) {
      body.value()->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ForInExprAST : public ForExprAST {
  std::string index;
  std::unique_ptr<ExprAST> range;

  std::optional<std::unique_ptr<ExprAST>> body;

 public:
  ForInExprAST(std::string index, std::unique_ptr<ExprAST> range)
      : index(std::move(index)), range(std::move(range)) {}

  void add_body(std::unique_ptr<ExprAST> c_body) override {
    body = std::move(c_body);
  }

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("ForInAST {} in\n", index);

    range->print_name(level + 1);

    if (body.has_value()) {
      body.value()->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class ConditionExprAST : public ExprAST {
 public:
  enum ConditonOperator {
    CONDITION_EQ,
    CONDITION_NE,
    CONDITION_LT,
    CONDITION_LE,
    CONDITION_GT,
    CONDITION_GE,
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

class IfAST : public ExprAST {
  std::unique_ptr<ExprAST> condition;
  std::unique_ptr<ExprAST> then_val;
  std::optional<std::unique_ptr<ExprAST>> else_val;

 public:
  IfAST(std::unique_ptr<ExprAST> condition, std::unique_ptr<ExprAST> then_val,
        std::optional<std::unique_ptr<ExprAST>> else_val)
      : condition(std::move(condition)),
        then_val(std::move(then_val)),
        else_val(std::move(else_val)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("IfAST\n");

    condition->print_name(level + 1);
    then_val->print_name(level + 1);
    if (else_val.has_value()) {
      else_val.value()->print_name(level + 1);
    }
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

class CaseExprAST : public ExprAST {
  std::unique_ptr<ExprAST> var;
  std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                        std::unique_ptr<ExprAST>>>
      condition_map;

 public:
  CaseExprAST(std::unique_ptr<ExprAST> var,
              std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                                    std::unique_ptr<ExprAST>>>
                  condition_map)
      : var(std::move(var)), condition_map(std::move(condition_map)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("CaseExprAST\n");

    size_t index = 0;
    for (auto& condition : condition_map) {
      for (ssize_t i = 0; i < level; i++) {
        std::print(" ");
      }
      if (level != 0) {
        std::print("|-");
      }

      std::println("<{}>", index++);

      for (ssize_t i = 0; i < level + 1; i++) {
        std::print(" ");
      }
      if (level != 0) {
        std::print("|-");
      }

      std::println("<Conditions>");

      for (auto& val : condition.first) {
        val->print_name(level + 3);
      }

      condition.second->print_name(level + 2);
    }
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

class FunctionDefAST : public ExprAST {
  std::string name;
  std::unique_ptr<ExprAST> body;

 public:
  FunctionDefAST(std::string name, std::unique_ptr<ExprAST> body)
      : name(std::move(name)), body(std::move(body)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("FunctionDefAST {}\n", name);

    body->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
  std::vector<std::string> get_functions_defined() override { return {name}; }
};

class InjectIntoStringAST : public ExprAST {
  std::unique_ptr<ExprAST> body;

 public:
  InjectIntoStringAST(std::unique_ptr<ExprAST> body) : body(std::move(body)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("InjectIntoStringAST\n");

    body->print_name(level + 1);
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

class RedirectExprAST : public ExprAST {
  std::unique_ptr<ExprAST> body;

 public:
  std::optional<std::unique_ptr<ExprAST>> in_file;
  bool and_prefix = false;

  bool and_suffix = false;
  std::optional<std::unique_ptr<ExprAST>> out_file;

  RedirectExprAST(std::unique_ptr<ExprAST> body) : body(std::move(body)) {}

  void print_name(ssize_t level) override {
    for (ssize_t i = 0; i < level - 1; i++) {
      std::print(" ");
    }
    if (level != 0) {
      std::print("|-");
    }

    std::print("RedirectExprAST {} {}\n", and_prefix, and_suffix);

    if (in_file.has_value()) {
      in_file.value()->print_name(level + 1);
    }
    body->print_name(level + 1);
    if (out_file.has_value()) {
      out_file.value()->print_name(level + 1);
    }
  }
  std::expected<llvm::Value*, std::string> codegen(
      CodegenState& state) override;
};

std::optional<std::unique_ptr<ExprAST>> parse_compound_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    bool top_level = false);
std::expected<std::optional<std::unique_ptr<ExprAST>>, std::string>
parse_expression(const std::vector<BashLexerSegment>& lexer_segments,
                 size_t& cursor, bool top_level = false, bool parse_ops = true);
std::optional<std::unique_ptr<ExprAST>> parse_paren_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_operator_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    int lhs_prec, std::unique_ptr<ExprAST> lefthandside);

// thank you StackOverflow https://stackoverflow.com/a/36120483
template <typename TO, typename FROM>
std::unique_ptr<TO> static_unique_pointer_cast(std::unique_ptr<FROM>&& old) {
  return std::unique_ptr<TO>{static_cast<TO*>(old.release())};
  // conversion: unique_ptr<FROM>->FROM*->TO*->unique_ptr<TO>
}

