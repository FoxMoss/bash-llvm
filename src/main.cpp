#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <print>
#include <source_location>
#include <string>
#include <utility>
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

class ExprAST {
 public:
  virtual ~ExprAST() = default;

  void print_name() { std::print("{}\n", typeid(typeof(this)).name()); }
};
class StringExprAST : public ExprAST {
  std::string val;

 public:
  StringExprAST(const std::string& val) : val(val) {}
};
class UnknownExprAST : public ExprAST {
 public:
  UnknownExprAST() {}
};

class IdentifierExprAST : public ExprAST {
  std::string name;

 public:
  IdentifierExprAST(const std::string& name) : name(name) {}
};

class RangeArrayExprAST : public ExprAST {
  std::vector<std::string> values;

 public:
  RangeArrayExprAST(const std::vector<std::string>& values) : values(values) {}
};

class ConcatExprAST : public ExprAST {
  std::unique_ptr<ExprAST> first;
  std::unique_ptr<ExprAST> second;

 public:
  ConcatExprAST(std::unique_ptr<ExprAST> first, std::unique_ptr<ExprAST> second)
      : first(std::move(first)), second(std::move(second)) {}
};

class RangeExprAST : public ExprAST {
  std::string first_value;
  std::string second_value;
  uint32_t step;

 public:
  RangeExprAST(const std::string& first_value, const std::string& second_value,
               const uint32_t& step)
      : first_value(first_value), second_value(second_value), step(step) {}
};

class AssignmentExprAST : public ExprAST {
  std::unique_ptr<IdentifierExprAST> identifier;
  std::unique_ptr<ExprAST> value;

 public:
  AssignmentExprAST(std::unique_ptr<IdentifierExprAST> identifier,
                    std::unique_ptr<ExprAST> value)
      : identifier(std::move(identifier)), value(std::move(value)) {}
};

class ForAST : public ExprAST {
  std::unique_ptr<IdentifierExprAST> index;
  std::unique_ptr<ExprAST> range;
  std::unique_ptr<ExprAST> body;

 public:
  ForAST(std::unique_ptr<IdentifierExprAST> index,
         std::unique_ptr<ExprAST> range, std::unique_ptr<ExprAST> body)
      : index(std::move(index)),
        range(std::move(range)),
        body(std::move(body)) {}
};

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

std::optional<std::unique_ptr<ExprAST>> parse_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    bool parent_paren = false);

std::optional<std::unique_ptr<IdentifierExprAST>> parse_identifier(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment;
  current_segment = get_current_segment(lexer_segments, cursor);

  // skip whitespace
  while (current_segment.has_value() &&
         current_segment->token == TOK_WHITESPACE) {
    current_segment = get_next_segment(lexer_segments, cursor);
  }

  if (!current_segment.has_value() ||
      current_segment->token != TOK_IDENTIFIER) {
    RETURN_WITH_WARNING();
  }

  // eat ident
  current_segment = get_next_segment(lexer_segments, cursor);

  return std::make_unique<IdentifierExprAST>(current_segment->str);
}

std::optional<std::unique_ptr<ExprAST>> parse_paren_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept {
  get_next_segment(lexer_segments, cursor);  // eat (

  auto body = parse_expression(lexer_segments, cursor, true);

  get_next_segment(lexer_segments, cursor);  // eat )
  return body;
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
    return {};
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
          state = SEARCH_FOR_SECOND;
        } else if (state == SEARCH_FOR_SECOND) {
          second = blob;
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
          if (curly.has_value()) {
            RETURN_WITH_WARNING();
          }
          last_expr = std::make_unique<ConcatExprAST>(
              std::move(last_expr.value()), std::move(curly.value()));
        }
        break;
      case TOK_IDENTIFIER: {
        auto ident = parse_identifier(lexer_segments, cursor);
        if (ident.has_value()) {
          RETURN_WITH_WARNING();
        }
        last_expr = std::make_unique<ConcatExprAST>(
            std::move(last_expr.value()), std::move(ident.value()));
      } break;
      case TOK_EOF:
        [[fallthrough]];
      case TOK_SEMI_COLON:
        // eat it
        get_next_segment(lexer_segments, cursor);
        done = true;
        break;
      default:
        RETURN_WITH_WARNING();
    }
  }
  return last_expr;
}

std::optional<std::unique_ptr<ForAST>> parse_for(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) {
  std::optional<BashLexerSegment> current_segment =
      get_next_segment(lexer_segments, cursor);

  auto index = parse_identifier(lexer_segments, cursor);
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

  current_segment = get_next_segment(lexer_segments, cursor);
  if (!current_segment.has_value() || current_segment->token != TOK_DONE) {
    RETURN_WITH_WARNING();
  }

  get_next_segment(lexer_segments, cursor);  // move cursor to continue parsing
  return std::make_unique<ForAST>(std::move(index.value()),
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
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor,
    bool parent_paren) {
  std::optional<BashLexerSegment> current_segment =
      get_current_segment(lexer_segments, cursor);

  bool done = false;
  while (!done) {
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
        if (!parent_paren) {
          return parse_paren_expression(lexer_segments, cursor);
        } else {  // if were in double paren teritory that means
        }
      } break;
      case TOK_EOF:
        done = true;
        break;

        // scraps of a previous expression probably
      case TOK_WHITESPACE:
        [[fallthrough]];
      case TOK_SEMI_COLON:
        [[fallthrough]];
      case TOK_NEWLINE:
        current_segment = get_next_segment(lexer_segments, cursor);
        goto continue_expression_search;
      default:
        // cant start with this token
        RETURN_WITH_MSG("Can't parse " + current_segment->get_token_name());
    }
  continue_expression_search:
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
  // base.value()->print_name();

  return 0;
}
