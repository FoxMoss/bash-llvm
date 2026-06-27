#include "ast.h"

#include <cassert>
#include <cstdio>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "args.h"
#include "helper.h"
#include "lexer.h"

std::optional<std::unique_ptr<ExprAST>> parse_paren_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  get_next_segment(lexer_segments, cursor);  // eat (

  auto body = parse_expression(lexer_segments, cursor);

  if (!body.has_value()) {
    std::print(stderr, "{}", body.error());
    RETURN_WITH_WARNING();
  }

  if (!body.value().has_value()) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments, cursor);  // eat )
  return std::move(body.value());
}

std::optional<std::unique_ptr<ExprAST>> parse_operator_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    int lhs_prec, std::unique_ptr<ExprAST> lefthandside) {
  std::optional<BashLexerSegment> current_segment;

  while (true) {
    skip_whitespace_and_newline(lexer_segments, cursor);
    current_segment = get_current_segment(lexer_segments, cursor);
    int16_t prec = current_segment->get_token_precidence();

    // https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl02.html#binary-expression-parsing
    // check to prevent non op tokens iirc?
    if (prec < lhs_prec) {
      return lefthandside;
    }

    auto binop = current_segment;

    get_next_segment(lexer_segments, cursor);  // eat op
    skip_whitespace(lexer_segments, cursor);

    if (!binop->get_operator_multiop()) {
      if (!lefthandside->get_ident_str().has_value()) {
        RETURN_WITH_WARNING()
      }

      return std::make_unique<MathSingleOpExprAST>(binop->get_math_op(),
                                                   std::move(lefthandside));
    }

    current_segment = get_current_segment(lexer_segments, cursor);

    std::optional<std::unique_ptr<ExprAST>> righthandside;
    if (current_segment->token == TOK_OPEN_PAREN ||
        current_segment->token == TOK_OPEN_PAREN_PAREN) {
      righthandside = parse_paren_math_expression(lexer_segments, cursor);
    } else {
      righthandside = parse_identifier_or_numeric(lexer_segments, cursor);
    }
    if (!righthandside.has_value()) {
      RETURN_WITH_WARNING();
    }

    skip_whitespace(lexer_segments, cursor);
    current_segment = get_current_segment(lexer_segments, cursor);

    if (prec < current_segment->get_token_precidence()) {
      righthandside = parse_operator_math_expression(
          lexer_segments, cursor, prec, std::move(righthandside.value()));
      if (righthandside.has_value()) {
        RETURN_WITH_WARNING();
      }
    }

    lefthandside = std::make_unique<MathOpExprAST>(
        binop->get_math_op(), std::move(lefthandside),
        std::move(righthandside.value()));
  }

  RETURN_WITH_WARNING()
}

std::optional<std::unique_ptr<ExprAST>> parse_paren_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  get_next_segment(lexer_segments, cursor);  // eat ((
  skip_whitespace(lexer_segments, cursor);

  auto lefthandside = parse_identifier_or_numeric(lexer_segments, cursor);
  if (!lefthandside.has_value()) {
    RETURN_WITH_WARNING()
  }
  auto ret = parse_operator_math_expression(lexer_segments, cursor, 0,
                                            std::move(lefthandside.value()));

  get_next_segment(lexer_segments, cursor);  // eat ))

  return ret;
}

std::optional<std::unique_ptr<CStyleForExprAST>> parse_c_like_for_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  get_next_segment(lexer_segments, cursor);  // eat ((

  skip_whitespace_and_newline(lexer_segments, cursor);
  auto current_segment = get_current_segment(lexer_segments, cursor);
  auto assigned = get_current_segment(lexer_segments, cursor);

  if (current_segment->token != TOK_VALUE) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  skip_whitespace_and_newline(lexer_segments, cursor);
  current_segment = get_current_segment(lexer_segments, cursor);
  if (current_segment->token != TOK_EQ) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  skip_whitespace_and_newline(lexer_segments, cursor);
  auto assignment = parse_identifier_or_numeric(lexer_segments, cursor);
  if (!assignment.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace_and_newline(lexer_segments, cursor);
  current_segment = get_current_segment(lexer_segments, cursor);
  if (current_segment->token != TOK_SEMI_COLON) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  skip_whitespace_and_newline(lexer_segments, cursor);
  auto lefthandside = parse_identifier_or_numeric(lexer_segments, cursor);
  if (!lefthandside.has_value()) {
    RETURN_WITH_WARNING()
  }

  auto cond = parse_operator_math_expression(lexer_segments, cursor, 0,
                                             std::move(lefthandside.value()));
  if (!cond.has_value()) {
    RETURN_WITH_WARNING()
  }

  skip_whitespace(lexer_segments, cursor);
  current_segment = get_current_segment(lexer_segments, cursor);
  if (current_segment->token != TOK_SEMI_COLON) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  skip_whitespace_and_newline(lexer_segments, cursor);
  lefthandside = parse_identifier_or_numeric(lexer_segments, cursor);
  if (!lefthandside.has_value()) {
    RETURN_WITH_WARNING()
  }

  auto increment = parse_operator_math_expression(
      lexer_segments, cursor, 0, std::move(lefthandside.value()));
  if (!increment.has_value()) {
    RETURN_WITH_WARNING()
  }
  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (current_segment->token != TOK_CLOSE_PAREN_PAREN) {
    RETURN_WITH_WARNING()
  }
  current_segment = get_next_segment(lexer_segments,
                                     cursor);  // eat ))

  if (current_segment->token != TOK_SEMI_COLON) {
    RETURN_WITH_WARNING()
  }
  current_segment = get_next_segment(lexer_segments,
                                     cursor);  // eat ;

  return std::make_unique<CStyleForExprAST>(
      std::make_unique<AssignmentExprAST>(assigned->str,
                                          std::move(assignment.value())),
      std::move(cond.value()), std::move(increment.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_for(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_next_segment(lexer_segments, cursor);  // eat for

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);

  std::optional<std::unique_ptr<ForExprAST>> for_in_expr;

  if (current_segment.has_value() &&
      current_segment->token == TOK_OPEN_PAREN_PAREN) {
    for_in_expr = parse_c_like_for_expression(lexer_segments, cursor);
  } else if (current_segment.has_value() &&
             current_segment->token == TOK_VALUE) {
    auto var_tok = get_current_segment(lexer_segments, cursor);

    get_next_segment(lexer_segments, cursor);

    skip_whitespace(lexer_segments, cursor);

    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value() || current_segment->token != TOK_IN) {
      RETURN_WITH_WARNING();
    }
    current_segment = get_next_segment(lexer_segments, cursor);  // eat in

    auto range = parse_floating_expression(lexer_segments, cursor);
    if (!range.has_value()) {
      RETURN_WITH_WARNING();
    }

    for_in_expr =
        std::make_unique<ForInExprAST>(var_tok->str, std::move(range.value()));

    skip_whitespace(lexer_segments, cursor);

    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value() ||
        current_segment->token != TOK_SEMI_COLON) {
      RETURN_WITH_WARNING();
    }
    current_segment = get_next_segment(lexer_segments, cursor);  // eat ;

  } else if (!current_segment.has_value() ||
             current_segment->token != TOK_VALUE) {
    RETURN_WITH_WARNING();
  }

  if (!for_in_expr.has_value()) {
    RETURN_WITH_WARNING()
  }

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DO) {
    RETURN_WITH_WARNING();
  }
  current_segment = get_next_segment(lexer_segments, cursor);  // eat do

  auto body = parse_compound_expression(lexer_segments, cursor);
  if (!body.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DONE) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments,
                   cursor);  // move cursor to continue parsing

  for_in_expr->get()->add_body(std::move(body.value()));
  return std::move(for_in_expr.value());
}

std::optional<std::unique_ptr<ExprAST>> parse_condition_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  skip_whitespace(lexer_segments, cursor);

  std::optional<BashLexerSegment> current_segment =
      get_current_segment(lexer_segments, cursor);

  if (current_segment->token != TOK_OPEN_SQUARE) {
    auto expr = parse_expression(lexer_segments, cursor);
    if (!expr.has_value()) {
      std::print(stderr, "{}", expr.error());
      RETURN_WITH_WARNING();
    }
    if (!expr.value().has_value()) {
      RETURN_WITH_WARNING();
    }
    return std::move(expr.value());
  }

  current_segment = get_next_segment(lexer_segments, cursor);

  auto first_op = parse_identifier(lexer_segments, cursor);
  if (!first_op.has_value()) {
    RETURN_WITH_WARNING()
  }

  skip_whitespace(lexer_segments, cursor);
  current_segment = get_current_segment(lexer_segments, cursor);
  switch (current_segment->token) {
    case TOK_SUB: {
      current_segment = get_next_segment(lexer_segments, cursor);
      if (!current_segment.has_value()) {
        RETURN_WITH_WARNING()
      }

      ConditionExprAST::ConditonOperator op;

      if (current_segment->str == "eq") {
        op = ConditionExprAST::CONDITION_EQ;
      } else if (current_segment->str == "ne") {
        op = ConditionExprAST::CONDITION_NE;
      } else if (current_segment->str == "lt") {
        op = ConditionExprAST::CONDITION_LT;
      } else if (current_segment->str == "le") {
        op = ConditionExprAST::CONDITION_LE;
      } else if (current_segment->str == "gt") {
        op = ConditionExprAST::CONDITION_GT;
      } else if (current_segment->str == "ge") {
        op = ConditionExprAST::CONDITION_GE;
      } else {
        RETURN_WITH_WARNING()
      }

      current_segment = get_next_segment(lexer_segments, cursor);

      auto second_op = parse_numeric(lexer_segments, cursor);
      if (!second_op.has_value()) {
        RETURN_WITH_WARNING()
      }

      skip_whitespace(lexer_segments, cursor);
      current_segment = get_current_segment(lexer_segments, cursor);
      if (current_segment->token != TOK_CLOSE_SQUARE) {
        RETURN_WITH_WARNING()
      }
      current_segment = get_next_segment(lexer_segments, cursor);  // eat ]

      skip_whitespace(lexer_segments, cursor);
      current_segment = get_current_segment(lexer_segments, cursor);

      if (current_segment->token == TOK_SEMI_COLON) {
        get_next_segment(lexer_segments, cursor);
      }

      return std::make_unique<ConditionExprAST>(std::move(first_op.value()), op,
                                                std::move(second_op.value()));
    }
    default:
      RETURN_WITH_WARNING()
  }
}

std::optional<std::unique_ptr<ExprAST>> parse_if(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_next_segment(lexer_segments, cursor);  // eat while

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);

  auto cond = parse_condition_expression(lexer_segments, cursor);

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() ||
      current_segment->token != TOK_SEMI_COLON) {
    RETURN_WITH_WARNING();
  }

  current_segment = get_next_segment(lexer_segments, cursor);  // eat ;

  skip_whitespace_and_newline(lexer_segments, cursor);
  current_segment =
      get_current_segment(lexer_segments, cursor);  // prepare then
  if (!current_segment.has_value() || current_segment->token != TOK_THEN) {
    RETURN_WITH_WARNING();
  }
  current_segment = get_next_segment(lexer_segments, cursor);  // eat then

  auto then_val = parse_compound_expression(lexer_segments, cursor);
  if (!then_val.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace_and_newline(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  if (current_segment->token == TOK_FI) {
    get_next_segment(lexer_segments,
                     cursor);  // move cursor to continue parsing
    return std::make_unique<IfAST>(std::move(cond.value()),
                                   std::move(then_val.value()), std::nullopt);
  }

  if (current_segment->token != TOK_ELSE) {
    RETURN_WITH_WARNING()
  }

  get_next_segment(lexer_segments,
                   cursor);  // move cursor to continue parsing
                             //
  auto else_val = parse_compound_expression(lexer_segments, cursor);
  if (!else_val.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace_and_newline(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_FI) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);  // eat fi

  return std::make_unique<IfAST>(std::move(cond.value()),
                                 std::move(then_val.value()),
                                 std::move(else_val.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_while(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_next_segment(lexer_segments, cursor);  // eat while

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);

  auto cond = parse_condition_expression(lexer_segments, cursor);

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DO) {
    RETURN_WITH_WARNING();
  }
  current_segment = get_next_segment(lexer_segments, cursor);  // eat do

  auto body = parse_compound_expression(lexer_segments, cursor);
  if (!body.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DONE) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments,
                   cursor);  // move cursor to continue parsing
  return std::make_unique<WhileAST>(std::move(cond.value()),
                                    std::move(body.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_function_body(
    std::string func_name, const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) {
  skip_whitespace(lexer_segments, cursor);

  auto current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() ||
      current_segment->token != TOK_OPEN_CURLY) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  auto body = parse_compound_expression(lexer_segments, cursor);
  if (!body.has_value()) {
    RETURN_WITH_WARNING()
  }

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() ||
      current_segment->token != TOK_CLOSE_CURLY) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments, cursor);

  return std::make_unique<FunctionDefAST>(func_name, std::move(body.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_assignment(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  auto current_segment = get_current_segment(lexer_segments, cursor);
  auto var_tok = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_VALUE) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_EQ) {
    RETURN_WITH_WARNING()
  }
  current_segment = get_next_segment(lexer_segments, cursor);  // eat =

  auto value = parse_identifier_or_value(lexer_segments, cursor);
  if (!value.has_value()) {
    RETURN_WITH_WARNING();
  }

  return std::make_unique<AssignmentExprAST>(var_tok->str,
                                             std::move(value.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_case(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  auto current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_CASE) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);

  skip_whitespace(lexer_segments, cursor);

  auto var = parse_identifier_or_value(lexer_segments, cursor);
  if (!var.has_value()) {
    RETURN_WITH_WARNING()
  }

  skip_whitespace(lexer_segments, cursor);

  std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                        std::unique_ptr<ExprAST>>>
      condition_map;

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_IN) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);
  skip_whitespace_and_newline(lexer_segments, cursor);
  current_segment = get_current_segment(lexer_segments, cursor);

  while (current_segment.has_value() && current_segment->token != TOK_ESAC) {
    skip_whitespace_and_newline(lexer_segments, cursor);

    std::vector<std::unique_ptr<ExprAST>> options;

    auto option = parse_identifier_or_value(lexer_segments, cursor);
    if (!option.has_value()) {
      RETURN_WITH_WARNING();
    }

    options.push_back(std::move(option.value()));

    skip_whitespace(lexer_segments, cursor);

    current_segment = get_current_segment(lexer_segments, cursor);
    while (current_segment.has_value() &&
           current_segment->token != TOK_CLOSE_PAREN) {
      if (current_segment->token != TOK_OR) {
        RETURN_WITH_WARNING();
      }
      current_segment = get_next_segment(lexer_segments, cursor);
      skip_whitespace_and_newline(lexer_segments, cursor);

      option = parse_identifier_or_value(lexer_segments, cursor);
      if (!option.has_value()) {
        RETURN_WITH_WARNING();
      }

      options.push_back(std::move(option.value()));
      skip_whitespace(lexer_segments, cursor);
      current_segment = get_current_segment(lexer_segments, cursor);
    }

    if (!current_segment.has_value() ||
        current_segment->token != TOK_CLOSE_PAREN) {
      RETURN_WITH_WARNING();
    }

    get_next_segment(lexer_segments, cursor);  // eat )

    auto expr = parse_compound_expression(lexer_segments, cursor);
    if (!expr.has_value()) {
      RETURN_WITH_WARNING();
    }

    condition_map.emplace_back(std::move(options), std::move(expr.value()));

    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value() ||
        current_segment->token != TOK_SEMI_SEMI) {
      RETURN_WITH_WARNING();
    }
    get_next_segment(lexer_segments, cursor);
    skip_whitespace_and_newline(lexer_segments, cursor);
    current_segment = get_current_segment(lexer_segments, cursor);
  }
  if (!current_segment.has_value() || current_segment->token != TOK_ESAC) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);  // eat esac

  return std::make_unique<CaseExprAST>(std::move(var.value()),
                                       std::move(condition_map));
}

std::optional<std::unique_ptr<ExprAST>> parse_compound_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    bool top_level) {
  std::vector<std::unique_ptr<ExprAST>> ret;

  auto value = parse_expression(lexer_segments, cursor, top_level);

  if (!value.has_value()) {
    RETURN_WITH_WARNING()
  }

  while (value.value().has_value()) {
    ret.push_back(std::move(value.value().value()));
    value = parse_expression(lexer_segments, cursor, top_level);
    if (!value.has_value()) {
      std::print(stderr, "{}", value.error());
      RETURN_WITH_WARNING()
    }
  }

  return std::make_unique<CompoundExprAST>(std::move(ret));
}

std::expected<std::optional<std::unique_ptr<ExprAST>>, std::string>
parse_expression(const std::vector<BashLexerSegment>& lexer_segments,
                 size_t& cursor, bool top_level, bool parse_ops) {
  std::optional<BashLexerSegment> current_segment =
      get_current_segment(lexer_segments, cursor);

  std::optional<std::unique_ptr<ExprAST>> return_expr;
  std::optional<std::unique_ptr<ExprAST>> last_expr;
  bool done = false;
  while (!done) {
    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value()) {
      UNEXPECTED_RETURN_WITH_WARNING()
    }

    switch (current_segment->token) {
      case TOK_IF: {
        auto if_expr = parse_if(lexer_segments, cursor);
        if (!if_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        return if_expr;
      }
      case TOK_WHILE: {
        auto while_expr = parse_while(lexer_segments, cursor);
        if (!while_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        return while_expr;
      }

      case TOK_FOR: {
        auto for_expr = parse_for(lexer_segments, cursor);
        if (!for_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        return static_unique_pointer_cast<ExprAST>(std::move(for_expr.value()));
      }
      case TOK_CASE: {
        auto case_expr = parse_case(lexer_segments, cursor);
        if (!case_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        return case_expr;
      }
      case TOK_OPEN_PAREN: {
        return_expr = parse_paren_expression(lexer_segments, cursor);
      } break;
      case TOK_OPEN_PAREN_PAREN: {
        return_expr = parse_paren_math_expression(lexer_segments, cursor);
      } break;
      case TOK_OR: {
        if (!parse_ops) {
          return last_expr;
        }

        current_segment = get_next_segment(lexer_segments, cursor);  // eat op

        if (!last_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        auto righthandside =
            parse_expression(lexer_segments, cursor, false, false);
        if (!righthandside.has_value()) {
          std::print(stderr, "{}", righthandside.error());
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        if (!righthandside.value().has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        return_expr = std::make_unique<PipeExprAST >(
             std::move(last_expr.value()),
            std::move(righthandside.value().value()));
        last_expr = {};
      } break;
      case TOK_AND_AND: {
        if (!parse_ops) {
          return last_expr;
        }

        current_segment = get_next_segment(lexer_segments, cursor);  // eat op

        if (!last_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        auto righthandside =
            parse_expression(lexer_segments, cursor, false, false);
        if (!righthandside.has_value()) {
          std::print(stderr, "{}", righthandside.error());
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        if (!righthandside.value().has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        return_expr = std::make_unique<StatementOpExprAST>(
            StatementOpExprAST::STATEMENT_OP_AND, std::move(last_expr.value()),
            std::move(righthandside.value().value()));
        last_expr = {};
      } break;
      case TOK_OR_OR: {
        if (!parse_ops) {
          return last_expr;
        }

        current_segment = get_next_segment(lexer_segments, cursor);  // eat op

        if (!last_expr.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        auto righthandside =
            parse_expression(lexer_segments, cursor, false, false);
        if (!righthandside.has_value()) {
          std::print(stderr, "{}", righthandside.error());
          UNEXPECTED_RETURN_WITH_WARNING();
        }
        if (!righthandside.value().has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING();
        }

        return_expr = std::make_unique<StatementOpExprAST>(
            StatementOpExprAST::STATEMENT_OP_OR, std::move(last_expr.value()),
            std::move(righthandside.value().value()));
        last_expr = {};
      } break;
      case TOK_FUNCTION: {  // the function is optional in bash
        get_next_segment(lexer_segments, cursor);  // eat func

        skip_whitespace(lexer_segments, cursor);
      } break;
      case TOK_TILDE:
        [[fallthrough]];
      case TOK_NUMERIC:
        [[fallthrough]];
      case TOK_DIV:
        [[fallthrough]];
      case TOK_RANGE:
        [[fallthrough]];
      case TOK_DOT:
        [[fallthrough]];
      case TOK_VALUE:
        [[fallthrough]];
      case TOK_IDENTIFIER: {
        auto next_op = peek_segment(lexer_segments, cursor);

        auto lookahead_cursor = cursor;
        get_next_segment(lexer_segments, lookahead_cursor);
        skip_whitespace(lexer_segments, lookahead_cursor);
        auto next_tok = get_current_segment(lexer_segments, lookahead_cursor);

        if (next_tok.has_value() && next_tok->token == TOK_FUNC_INDICATOR) {
          if (!top_level) {
            UNEXPECTED_RETURN_WITH_MSG("Function must be top level");
          }
          cursor = lookahead_cursor;

          get_next_segment(lexer_segments, cursor);  // eat ()
          return_expr = parse_function_body(current_segment.value().str,
                                            lexer_segments, cursor);

        } else if (next_op->token != TOK_EQ) {
          auto call = parse_call_expression(lexer_segments, cursor);
          if (!call.has_value()) {
            UNEXPECTED_RETURN_WITH_WARNING()
          }
          return_expr = std::move(call.value());
        } else {
          return_expr = parse_assignment(lexer_segments, cursor);
        }
      } break;
      case TOK_OPEN_SQUARE: {
        auto cond = parse_condition_expression(lexer_segments, cursor);
        if (!cond.has_value()) {
          UNEXPECTED_RETURN_WITH_WARNING()
        }
        return_expr = std::move(cond.value());
      } break;
      case TOK_EOF:
        [[fallthrough]];
      case TOK_CLOSE_CURLY:
        [[fallthrough]];
      case TOK_SEMI_SEMI:
        [[fallthrough]];
      case TOK_DONE:
        [[fallthrough]];
      case TOK_ELSE:
        [[fallthrough]];
      case TOK_FI:
        [[fallthrough]];
      case TOK_CLOSE_PAREN:
        [[fallthrough]];
      case TOK_BACKTICK:
        return last_expr;

      case TOK_SEMI_COLON:
        [[fallthrough]];
      case TOK_NEWLINE:
        if (last_expr.has_value()) {
          return last_expr;
        }
        current_segment = get_next_segment(lexer_segments, cursor);
        // continue
        break;

        // scraps of a previous expression probably
      case TOK_COMMENT:
        [[fallthrough]];
      case TOK_WHITESPACE:
        current_segment = get_next_segment(lexer_segments, cursor);
        break;
      default:
        UNEXPECTED_RETURN_WITH_MSG("Can't parse " +
                                   current_segment->get_token_name() + " " +
                                   current_segment->str);
    }

    if (return_expr.has_value()) {
      if (last_expr.has_value()) {
        std::vector<std::unique_ptr<ExprAST>> expr_array;
        expr_array.push_back(std::move(last_expr.value()));
        expr_array.push_back(std::move(return_expr.value()));
        last_expr = std::make_unique<CompoundExprAST>(std::move(expr_array));
      } else {
        last_expr = std::move(return_expr);
      }
    }
    return_expr = {};
  }

  UNEXPECTED_RETURN_WITH_MSG("Can't parse " +
                             current_segment->get_token_name());
}

std::string StatementOpExprAST::get_op_name(StatementOp op) {
#define OP(x) \
  case x:     \
    return #x;
  switch (op) {
#include "statementops.inc"
  }
#undef TOKEN
  std::unreachable();
}
