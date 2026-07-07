
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>

#include "ast.h"
#include "lexer.h"
#include "helper.h"

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

  bool has_comma =
      std::ranges::find_if(sub_segments, [](const BashLexerSegment& a) {
        return a.token == TOK_COMMA;
      }) != sub_segments.end();

  std::vector<std::unique_ptr<ExprAST>> content_array;
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
        content_array.push_back(std::make_unique<StringExprAST>(blob));
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
          step = atol(blob.c_str());
        }

      } else if (blob != "") {
        content_array.push_back(std::make_unique<StringExprAST>(blob));
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

// can never return an array
std::optional<std::unique_ptr<ExprAST>> parse_floating_arg(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) noexcept {
  auto current_segment = get_current_segment(lexer_segments, cursor);
  if (!current_segment.has_value()) {
    return {};
  }

  switch (current_segment->token) {
    case TOK_IDENTIFIER: {
      auto ident_source = parse_identifier(lexer_segments, cursor);
      if (!ident_source.has_value()) {
        RETURN_WITH_WARNING();
      }

      return ident_source;

    } break;
    default:
      [[fallthrough]];
    case TOK_VALUE: {
      auto val = std::make_unique<StringExprAST>(current_segment->str);
      get_next_segment(lexer_segments, cursor);
      return val;
    } break;
    case TOK_INJECT_MATH: {
      // this eats the $(( and the ))
      auto injected_str = parse_paren_math_expression(lexer_segments, cursor);
      if (!injected_str.has_value()) {
        RETURN_WITH_WARNING();
      }

      return std::make_unique<ConvertToStringExprAST>(
          std::move(injected_str.value()));
    } break;

    case TOK_INJECT_STR: {
      get_next_segment(lexer_segments, cursor);  // eat $(
      auto injected_str = parse_expression(lexer_segments, cursor);
      if (!injected_str.has_value()) {
        std::print(stderr, "{}", injected_str.error());
        RETURN_WITH_WARNING();
      }

      if (!injected_str.value().has_value()) {
        RETURN_WITH_WARNING();
      }

      std::unique_ptr<ExprAST> inject_command =
          std::make_unique<InjectIntoStringAST>(
              std::move(injected_str.value().value()));

      skip_whitespace_and_newline(lexer_segments, cursor);
      auto close_paren = get_current_segment(lexer_segments, cursor);
      if (close_paren->token != TOK_CLOSE_PAREN) {
        RETURN_WITH_WARNING();
      }
      get_next_segment(lexer_segments, cursor);  // eat )

      return inject_command;
    } break;
  }

  return {};
}

std::optional<std::unique_ptr<ExprAST>> parse_floating_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) noexcept {
  std::optional<BashLexerSegment> current_segment;
  bool done = false;

  std::optional<std::unique_ptr<ExprAST>> colapsed;
  // colapsed should always be reducible to a array for args
  std::optional<std::unique_ptr<ExprAST>> last_expr;
  // last_expr should always be reducible to a string for string concatiation

  while (!done) {
    current_segment = get_current_segment(lexer_segments, cursor);
    if (!current_segment.has_value()) {
      break;
    }

    switch (current_segment->token) {
      case TOK_WHITESPACE:
        get_next_segment(lexer_segments, cursor);

        if (last_expr.has_value()) {
          if (colapsed.has_value()) {
            colapsed = std::make_unique<ConcatExprAST>(
                std::move(colapsed.value()),
                std::make_unique<ConvertToArrayExprAST>(
                    std::move(last_expr.value())));
            last_expr = {};
          } else {
            colapsed = std::make_unique<ConvertToArrayExprAST>(
                std::move(last_expr.value()));
            last_expr = {};
          }
        }
        break;
      case TOK_OPEN_CURLY: {
        auto curly = parse_curly_expression(lexer_segments, cursor);
        if (!curly.has_value()) {
          RETURN_WITH_WARNING();
        }

        if (last_expr.has_value()) {
          if (colapsed.has_value()) {
            colapsed = std::make_unique<ConcatExprAST>(
                std::move(colapsed.value()),
                std::make_unique<ConvertToArrayExprAST>(
                    std::move(last_expr.value())));
            last_expr = {};
          } else {
            colapsed = std::make_unique<ConvertToArrayExprAST>(
                std::move(last_expr.value()));
            last_expr = {};
          }
        }

        if (colapsed.has_value()) {
          colapsed = std::make_unique<ConcatExprAST>(
              std::move(colapsed.value()), std::move(curly.value()));
        } else {
          colapsed = std::move(curly.value());
        }
      } break;

      case TOK_CONCAT_SILENT: {
        get_next_segment(lexer_segments, cursor);  // skip tok
        if (!last_expr.has_value()) {
          RETURN_WITH_WARNING()
        }

        auto second_arg = parse_floating_arg(lexer_segments, cursor);

        if (!second_arg.has_value()) {
          RETURN_WITH_WARNING();
        }

        if (!last_expr.has_value()) {
          RETURN_WITH_WARNING();
        }
        last_expr = std::make_unique<ConcatStringsAST>(
            std::move(last_expr.value()), std::move(second_arg.value()));
      } break;
      default: {
        auto arg = parse_floating_arg(lexer_segments, cursor);

        if (!arg.has_value()) {
          RETURN_WITH_WARNING();
        }

        if (last_expr.has_value()) {
          last_expr = std::make_unique<ConcatStringsAST>(
              std::move(last_expr.value()), std::move(arg.value()));
        } else {
          last_expr = std::move(arg.value());
        }

      } break;
      case TOK_GREATER:
        [[fallthrough]];
      case TOK_AND:
        [[fallthrough]];
      case TOK_AND_AND:
        [[fallthrough]];
      case TOK_OR:
        [[fallthrough]];
      case TOK_OR_OR:
        [[fallthrough]];
      case TOK_CLOSE_PAREN_PAREN:
        [[fallthrough]];
      case TOK_CLOSE_PAREN:
        [[fallthrough]];
      case TOK_EOF:
        [[fallthrough]];
      case TOK_NEWLINE:
        [[fallthrough]];
      case TOK_SEMI_COLON:
        if (last_expr.has_value()) {
          if (colapsed.has_value()) {
            colapsed = std::make_unique<ConcatExprAST>(
                std::move(colapsed.value()),
                std::make_unique<ConvertToArrayExprAST>(
                    std::move(last_expr.value())));
            last_expr = {};
          } else {
            colapsed = std::make_unique<ConvertToArrayExprAST>(
                std::move(last_expr.value()));

            last_expr = {};
          }
        }
        done = true;
        break;
    }
  }
  return colapsed;
}

std::optional<std::unique_ptr<ExprAST>> parse_call_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  skip_whitespace(lexer_segments, cursor);
  std::string program_name = "";

  std::optional<BashLexerSegment> program_name_tok =
      get_current_segment(lexer_segments, cursor);
  while (program_name_tok.has_value() &&
         program_name_tok->token != TOK_WHITESPACE &&
         program_name_tok->token != TOK_NEWLINE &&
         program_name_tok->token != TOK_EOF) {
    program_name += program_name_tok->str;
    program_name_tok = get_next_segment(lexer_segments, cursor);
  }

  if (program_name == "") {
    RETURN_WITH_WARNING();
  }

  if (program_name != "expr") {
    auto args = parse_floating_expression(lexer_segments, cursor);
    if (!args.has_value()) {
      return std::make_unique<CallExprAST>(program_name);
    } else {
      return std::make_unique<CallExprAST>(program_name,
                                           std::move(args.value()));
    }

  } else {
    skip_whitespace_and_newline(lexer_segments, cursor);
    auto lefthandside = parse_identifier_or_numeric(lexer_segments, cursor);
    if (!lefthandside.has_value()) {
      RETURN_WITH_WARNING()
    }

    return parse_operator_math_expression(lexer_segments, cursor, 0,
                                          std::move(lefthandside.value()));
  }
}
