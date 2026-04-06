#include "ast.h"

#include <memory>
#include <optional>
#include <vector>

#include "lexer.h"

#define RETURN_WITH_WARNING()                                           \
  auto loc = std::source_location::current();                           \
  std::print("warning: {}:{}:{}\nin {}\n", loc.file_name(), loc.line(), \
             loc.column(), loc.function_name());                        \
  return {};

#define RETURN_WITH_MSG(msg)                                                \
  auto loc = std::source_location::current();                               \
  std::print("warning: {}:{}:{}\nin {}\n{}\n", loc.file_name(), loc.line(), \
             loc.column(), loc.function_name(), msg);                       \
  return {};

std::optional<BashLexerSegment> get_current_segment(
    const std::vector<BashLexerSegment>& lexer_segments, const size_t& cursor) {
  if (cursor < lexer_segments.size()) {
    return lexer_segments[cursor];
  }
  RETURN_WITH_WARNING();
}
std::optional<BashLexerSegment> get_next_segment(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  cursor++;
  if (cursor < lexer_segments.size()) {
    return lexer_segments[cursor];
  }
  RETURN_WITH_WARNING();
}
std::optional<BashLexerSegment> peek_segment(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  if (cursor + 1 < lexer_segments.size()) {
    return lexer_segments[cursor + 1];
  }
  RETURN_WITH_WARNING();
}

void skip_whitespace(const std::vector<BashLexerSegment>& lexer_segments,
                     size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);
  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token == TOK_WHITESPACE) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }
}
std::optional<std::unique_ptr<IdentifierExprAST>> parse_identifier(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token == TOK_WHITESPACE) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }

  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  std::optional<std::unique_ptr<IdentifierExprAST>> ret;

  if (current_segment->token != TOK_IDENTIFIER) {
    RETURN_WITH_WARNING()
  }

  ret = std::make_unique<IdentifierExprAST>(current_segment->str);
  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return ret;
}

std::optional<std::unique_ptr<StringExprAST>> parse_value(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token == TOK_WHITESPACE) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }

  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  std::optional<std::unique_ptr<StringExprAST>> ret;

  if (current_segment->token != TOK_VALUE &&
      current_segment->token != TOK_IDENTIFIER) {
    RETURN_WITH_WARNING()
  }

  ret = std::make_unique<StringExprAST>(current_segment->str);
  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return ret;
}

std::optional<std::unique_ptr<NumericExprAST>> parse_numeric(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token == TOK_WHITESPACE) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }

  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  std::optional<std::unique_ptr<NumericExprAST>> ret;

  if (current_segment->token != TOK_NUMERIC) {
    RETURN_WITH_WARNING()
  }

  ret = std::make_unique<NumericExprAST>(
      std::strtod(current_segment->str.data(), NULL));
  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return ret;
}

std::optional<std::unique_ptr<ExprAST>> parse_curly_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  get_next_segment(lexer_segments, cursor);  // eat {

  std::vector<BashLexerSegment> sub_segments;

  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token != TOK_CLOSE_CURLY) {
    sub_segments.push_back(current_segment.value());
    current_segment = get_next_segment(lexer_segments, cursor);
  }
  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING()
  }

  bool has_comma = std::find_if(sub_segments.begin(), sub_segments.end(),
                                [](const BashLexerSegment& a) {
                                  return a.token == TOK_COMMA;
                                }) != sub_segments.end();

  std::vector<std::string> content_array;
  std::optional<std::string> first;
  std::optional<std::string> second;
  uint32_t step = 1;

  // comma takes precident over range
  if (has_comma) {
    sub_segments.emplace_back(TOK_COMMA,
                              ",");  // so every element has a comma after
    std::string blob = "";

    for (auto segment : sub_segments) {
      if (segment.token != TOK_COMMA) {
        blob.append(segment.str);
      } else if (blob != "") {
        content_array.push_back(blob);
      }
    }
  } else {
    sub_segments.emplace_back(TOK_RANGE,
                              "..");  // so every element has a range after
    std::string blob = "";

    enum State {
      SEARCH_FOR_FIRST,
      SEARCH_FOR_SECOND,
      SEARCH_FOR_STEP
    } state = SEARCH_FOR_FIRST;

    for (auto segment : sub_segments) {
      if (segment.token != TOK_RANGE) {
        blob.append(segment.str);

        if (state == SEARCH_FOR_FIRST) {
          first = blob;
          blob = "";
          state = SEARCH_FOR_SECOND;
        } else if (state == SEARCH_FOR_SECOND) {
          second = blob;
          blob = "";
          state = SEARCH_FOR_STEP;
        } else if (state == SEARCH_FOR_STEP) {
          step = std::stoul(blob);
        }

      } else if (blob != "") {
        content_array.push_back(blob);
      }
    }
  }

  // curly expressions either have a range operator or comma operator

  get_next_segment(lexer_segments, cursor);  // eat }
  if (has_comma) {
    return std::make_unique<RangeArrayExprAST>(content_array);
  } else {
    if (!first.has_value() || !second.has_value()) {
      RETURN_WITH_WARNING();
    }
    return std::make_unique<RangeExprAST>(first.value(), second.value(), step);
  }
}

std::optional<std::unique_ptr<ExprAST>> parse_floating_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  bool done = false;

  std::optional<std::unique_ptr<ExprAST>> last_expr;

  while (!done) {
    current_segment = get_current_segment(lexer_segments, cursor);

    skip_whitespace(lexer_segments, cursor);

    current_segment = get_current_segment(lexer_segments, cursor);
    switch (current_segment->token) {
      case TOK_OPEN_CURLY:
        if (!last_expr.has_value()) {
          last_expr = parse_curly_expression(lexer_segments, cursor);
          if (!last_expr.has_value()) {
            RETURN_WITH_WARNING();
          }
        } else {
          auto curly = parse_curly_expression(lexer_segments, cursor);
          if (!curly.has_value()) {
            RETURN_WITH_WARNING();
          }
          last_expr = std::make_unique<ConcatExprAST>(
              std::move(last_expr.value()), std::move(curly.value()));
        }
        break;
      case TOK_VALUE:
        [[fallthrough]];
      case TOK_IDENTIFIER: {
        auto ident = parse_value(lexer_segments, cursor);
        if (!ident.has_value()) {
          RETURN_WITH_WARNING();
        }
        if (last_expr.has_value()) {
          last_expr = std::make_unique<ConcatExprAST>(
              std::move(last_expr.value()), std::move(ident.value()));
        } else {
          std::vector<std::string> array = {ident.value()->val};
          last_expr = std::make_unique<RangeArrayExprAST>(array);
        }
      } break;

        // skip it
      case TOK_WHITESPACE:
        [[fallthrough]];
      case TOK_NEWLINE:
        break;

      case TOK_SEMI_COLON:
        // eat it
        get_next_segment(lexer_segments, cursor);
        [[fallthrough]];

        // if we don't parse
      default:
        done = true;
        break;
    }
  }
  return last_expr;
}

std::optional<std::unique_ptr<ExprAST>> parse_call_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  skip_whitespace(lexer_segments, cursor);
  std::optional<BashLexerSegment> program_name =
      get_current_segment(lexer_segments, cursor);

  if (!program_name.has_value()) {
    RETURN_WITH_WARNING();
  }
  get_next_segment(lexer_segments, cursor);  // eat token

  auto args = parse_floating_expression(lexer_segments, cursor);
  if (!args.has_value()) {
    RETURN_WITH_WARNING()
  }

  return std::make_unique<CallExprAST>(program_name->str,
                                       std::move(args.value()));
}

std::optional<std::unique_ptr<ExprAST>> parse_paren_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  get_next_segment(lexer_segments, cursor);  // eat (

  auto body = parse_expression(lexer_segments, cursor);

  get_next_segment(lexer_segments, cursor);  // eat )
  return body;
}

std::optional<std::unique_ptr<ExprAST>> parse_paren_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);

std::optional<std::unique_ptr<ExprAST>> parse_operator_math_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    int lhs_prec, std::unique_ptr<ExprAST> lefthandside) {
  std::optional<BashLexerSegment> current_segment;

  while (true) {
    skip_whitespace(lexer_segments, cursor);
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
    current_segment = get_current_segment(lexer_segments, cursor);

    std::optional<std::unique_ptr<ExprAST>> righthandside;
    if (current_segment->token == TOK_OPEN_PAREN ||
        current_segment->token == TOK_OPEN_PAREN) {
      righthandside = parse_paren_math_expression(lexer_segments, cursor);
    } else {
      if (current_segment->token == TOK_IDENTIFIER) {
        righthandside = parse_identifier(lexer_segments, cursor);
      } else if (current_segment->token == TOK_NUMERIC) {
        righthandside = parse_numeric(lexer_segments, cursor);
      }
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

  auto lefthandside = parse_identifier(lexer_segments, cursor);
  if (!lefthandside.has_value()) {
    RETURN_WITH_WARNING()
  }
  auto ret = parse_operator_math_expression(lexer_segments, cursor, 0,
                                            std::move(lefthandside.value()));

  get_next_segment(lexer_segments, cursor);  // eat ))

  return ret;
}

std::optional<std::unique_ptr<ForAST>> parse_for(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_next_segment(lexer_segments, cursor);

  std::optional<std::unique_ptr<StringExprAST>> index =
      parse_value(lexer_segments, cursor);
  if (!index.has_value()) {
    RETURN_WITH_WARNING();
  }

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

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DO) {
    RETURN_WITH_WARNING();
  }
  current_segment = get_next_segment(lexer_segments, cursor);  // eat do

  auto body = parse_expression(lexer_segments, cursor);
  if (!body.has_value()) {
    RETURN_WITH_WARNING();
  }

  skip_whitespace(lexer_segments, cursor);

  current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DONE) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments, cursor);  // move cursor to continue parsing
  return std::make_unique<ForAST>(std::move(index.value()->val),
                                  std::move(range.value()),
                                  std::move(body.value()));
}

// thank you StackOverflow https://stackoverflow.com/a/36120483
template <typename TO, typename FROM>
std::unique_ptr<TO> static_unique_pointer_cast(std::unique_ptr<FROM>&& old) {
  return std::unique_ptr<TO>{static_cast<TO*>(old.release())};
  // conversion: unique_ptr<FROM>->FROM*->TO*->unique_ptr<TO>
}

std::optional<std::unique_ptr<ExprAST>> parse_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_current_segment(lexer_segments, cursor);

  std::optional<std::unique_ptr<ExprAST>> return_expr;
  bool done = false;
  while (!done) {
    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value()) {
      RETURN_WITH_WARNING()
    }

    std::optional<BashLexerSegment> next_segment =
        peek_segment(lexer_segments, cursor);

    switch (current_segment->token) {
      case TOK_FOR: {
        auto for_expr = parse_for(lexer_segments, cursor);
        if (!for_expr.has_value()) {
          RETURN_WITH_WARNING();
        }
        return static_unique_pointer_cast<ExprAST>(std::move(for_expr.value()));
      }
      case TOK_OPEN_PAREN: {
        return_expr = parse_paren_expression(lexer_segments, cursor);
      } break;
      case TOK_OPEN_PAREN_PAREN: {
        return_expr = parse_paren_math_expression(lexer_segments, cursor);
      } break;
      case TOK_AND_AND: {
        current_segment = get_next_segment(lexer_segments, cursor);  // eat op

        if (!return_expr.has_value()) {
          RETURN_WITH_WARNING();
        }

        auto righthandside = parse_expression(lexer_segments, cursor);
        if (!righthandside.has_value()) {
          RETURN_WITH_WARNING();
        }

        return_expr = std::make_unique<StatementOpExprAST>(
            StatementOpExprAST::STATEMENT_OP_AND,
            std::move(return_expr.value()), std::move(righthandside.value()));
      } break;
      case TOK_OR_OR: {
        current_segment = get_next_segment(lexer_segments, cursor);  // eat op

        if (!return_expr.has_value()) {
          RETURN_WITH_WARNING();
        }

        auto righthandside = parse_expression(lexer_segments, cursor);
        if (!righthandside.has_value()) {
          RETURN_WITH_WARNING();
        }

        return_expr = std::make_unique<StatementOpExprAST>(
            StatementOpExprAST::STATEMENT_OP_OR, std::move(return_expr.value()),
            std::move(righthandside.value()));
      } break;
      case TOK_IDENTIFIER: {
        auto call = parse_call_expression(lexer_segments, cursor);
        if (!call.has_value()) {
          RETURN_WITH_WARNING()
        }
        return_expr = std::move(call.value());
      } break;
      case TOK_EOF:
        done = true;
        break;

      case TOK_DONE:
        [[fallthrough]];
      case TOK_CLOSE_PAREN:
        [[fallthrough]];
      case TOK_SEMI_COLON:
        return return_expr;
        // scraps of a previous expression probably
      case TOK_WHITESPACE:
        [[fallthrough]];
      case TOK_NEWLINE:
        current_segment = get_next_segment(lexer_segments, cursor);
        break;
      default:
        // cant start with this token
        RETURN_WITH_MSG("Can't parse " + current_segment->get_token_name() +
                        " " + current_segment->str);
    }
  }

  // THIS IS A WHOLE MINE FIELD HOLY
  // a=b kind of expression
  // if (next_segment.has_value() && next_segment->token == TOK_EQ) {
  //   auto variable = std::make_unique<VariableExprAST>(current_segment->str);
  //   get_next_segment(lexer_segments, cursor);  // eat the eq
  //   current_segment = get_next_segment(lexer_segments, cursor);
  //   if (!current_segment.has_value()) {
  //     return {};
  //   }
  // }

  RETURN_WITH_MSG("Can't parse " + current_segment->get_token_name());
}
