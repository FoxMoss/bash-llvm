#include "helper.h"

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
  return {};
}

void skip_whitespace_and_newline(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);
  // skip whitespace
  while (current_segment.has_value() &&
         (current_segment->token == TOK_WHITESPACE ||
          current_segment->token == TOK_NEWLINE)) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }
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

std::optional<std::unique_ptr<ExprAST>> parse_identifier(
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

  if (current_segment->token != TOK_IDENTIFIER &&
      current_segment->token != TOK_VALUE) {
    RETURN_WITH_WARNING()
  }

  ret = std::make_unique<IdentifierExprAST>(current_segment->str);
  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return ret;
}

std::optional<std::unique_ptr<ExprAST>> parse_value(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  std::optional<std::unique_ptr<ExprAST>> ret;

  if (current_segment->token == TOK_BACKTICK) {
    get_next_segment(lexer_segments, cursor);  // eat tick

    auto ret = parse_expression(lexer_segments, cursor);
    if (!ret.has_value()) {
      std::print(stderr, "{}", ret.error());
      RETURN_WITH_WARNING();
    }
    if (!ret.value().has_value()) {
      RETURN_WITH_WARNING();
    }

    get_next_segment(lexer_segments, cursor);  // eat tick

    return std::move(*ret);
  } else if (current_segment->token == TOK_NEWLINE) {
    return std::make_unique<StringExprAST>("");
  } else if (current_segment->token == TOK_OPEN_PAREN) {
    get_next_segment(lexer_segments, cursor);  // eat (

    auto value = parse_value(lexer_segments, cursor);
    if (!value.has_value()) {
      RETURN_WITH_WARNING();
    }

    auto tok = get_current_segment(lexer_segments, cursor);  // eat )
    if (!tok.has_value() || tok.value().token != TOK_CLOSE_PAREN) {
      RETURN_WITH_WARNING();
    }
    tok = get_next_segment(lexer_segments, cursor);

    if (tok->token == TOK_WHITESPACE || tok->token == TOK_NEWLINE) {
      return value;
    }

    auto after_value = parse_value(lexer_segments, cursor);
    if (!after_value.has_value()) {
      RETURN_WITH_WARNING()
    }

    return std::make_unique<ConcatStringsAST>(
        std::make_unique<ConcatStringsAST>(
            std::make_unique<ConcatStringsAST>(
                std::make_unique<StringExprAST>("("), std::move(value.value())),
            std::make_unique<StringExprAST>(")")),
        std::move(after_value.value()));

  } else {
    std::string str = "";

    std::optional<BashLexerSegment> current_token =
        get_current_segment(lexer_segments, cursor);
    while (current_token.has_value() &&
           current_token->token != TOK_WHITESPACE &&
           current_token->token != TOK_NEWLINE &&
           current_token->token != TOK_EOF &&
           current_token->token != TOK_CLOSE_PAREN &&
           current_token->token != TOK_CLOSE_SQUARE &&
           current_token->token != TOK_CLOSE_PAREN_PAREN &&
           current_token->token != TOK_CLOSE_BRACE) {
      str += current_token->str;
      current_token = get_next_segment(lexer_segments, cursor);
    }
    return std::make_unique<StringExprAST>(str);
  }
}

std::optional<std::unique_ptr<ExprAST>> parse_numeric(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  skip_whitespace(lexer_segments, cursor);

  std::optional<BashLexerSegment> current_segment =
      get_current_segment(lexer_segments, cursor);

  if (!current_segment.has_value()) {
    RETURN_WITH_WARNING();
  }

  std::optional<std::unique_ptr<NumericExprAST>> ret;

  float mult = 1;
  if (current_segment->token == TOK_SUB) {
    mult = -1;
    current_segment = get_next_segment(lexer_segments, cursor);
  }

  if (current_segment->token != TOK_NUMERIC) {
    RETURN_WITH_WARNING()
  }

  ret = std::make_unique<NumericExprAST>(
      std::strtod(current_segment->str.data(), nullptr) * mult);
  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return ret;
}

std::optional<std::unique_ptr<ExprAST>> parse_identifier_or_numeric(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  if (current_segment->token == TOK_IDENTIFIER ||
      current_segment->token == TOK_VALUE) {
    return parse_identifier(lexer_segments, cursor);
  } else if (current_segment->token == TOK_NUMERIC ||
             current_segment->token == TOK_SUB) {
    return parse_numeric(lexer_segments, cursor);
  } else {
    RETURN_WITH_WARNING()
  }
}

std::optional<std::unique_ptr<ExprAST>> parse_identifier_or_value(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  if (current_segment->token == TOK_IDENTIFIER) {
    return parse_identifier(lexer_segments, cursor);
  }
  return parse_value(lexer_segments, cursor);
}
