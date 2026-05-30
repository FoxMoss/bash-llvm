#include "lexer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <print>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

bool is_alpha(char c) {
  bool lowercase = ('a' <= c && c <= 'z');
  bool uppercase = ('A' <= c && c <= 'Z');
  bool is_bonus = c < 0;

  return uppercase || lowercase || c == '_' || is_bonus;
}
bool is_numeric(char c) {
  bool num = ('0' <= c && c <= '9');

  return num;
}
bool is_alpha_numeric(char c) {
  bool lowercase = ('a' <= c && c <= 'z');
  bool uppercase = ('A' <= c && c <= 'Z');
  bool num = ('0' <= c && c <= '9');
  bool is_bonus = c < 0;

  return uppercase || lowercase || num || c == '_' || is_bonus;
}
bool is_whitespace(char c) { return c == ' ' || c == '\t'; }

std::optional<char> read_char(const std::string& source, size_t& cursor,
                              std::string& token) {
  if (cursor < source.size()) {
    char c = source[cursor];
    token.push_back(c);
    cursor++;
    return c;
  }
  return {};
}
std::optional<char> peek_char(const std::string& source, size_t cursor) {
  if (cursor < source.size()) {
    return source[cursor];
  }
  return {};
}

std::vector<BashLexerSegment> BashLexerSegment::munch_token(
    std::string& source, size_t& cursor, BashLexerToken prev_token,
    ParenMap& paren_map) {
  ssize_t my_index = paren_map.index_counter;
  std::string token = "";

  std::optional<char> current_char = read_char(source, cursor, token);
  if (!current_char.has_value()) {
    return {BashLexerSegment(TOK_EOF, token)};
  }

  if (is_whitespace(current_char.value())) {
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && is_whitespace(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    return {BashLexerSegment(TOK_WHITESPACE, token)};
  }
  if (is_alpha(current_char.value()) ||
      current_char == '$') {  // value, identifier, or keyword
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && is_alpha_numeric(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    if (!next_char.has_value() || next_char.value() != '=') {
      if (token == "if")
        return {BashLexerSegment(TOK_IF, token)};
      else if (token == "else")
        return {BashLexerSegment(TOK_ELSE, token)};
      else if (token == "fi")
        return {BashLexerSegment(TOK_FI, token)};
      else if (token == "do")
        return {BashLexerSegment(TOK_DO, token)};
      else if (token == "then")
        return {BashLexerSegment(TOK_THEN, token)};
      else if (token == "in")
        return {BashLexerSegment(TOK_IN, token)};
      else if (token == "done")
        return {BashLexerSegment(TOK_DONE, token)};
      else if (token == "for")
        return {BashLexerSegment(TOK_FOR, token)};
      else if (token == "while")
        return {BashLexerSegment(TOK_WHILE, token)};
      else if (token == "function")
        return {BashLexerSegment(TOK_FUNCTION, token)};
      // TODO: finish the LUT
    }

    if (token.starts_with("$")) {
      if (next_char.has_value() && next_char == '(') {
        current_char = read_char(source, cursor, token);

        paren_map.level_counter++;
        paren_map.relevant_indices.emplace_back(my_index,
                                                paren_map.level_counter, true);
        paren_map.level_map[paren_map.level_counter] = {my_index, true, 0};

        return {BashLexerSegment(TOK_INJECT_STR, token)};
      }
      return {
          BashLexerSegment(TOK_IDENTIFIER, token.substr(1, token.size() - 1))};
    } else {
      return {BashLexerSegment(TOK_VALUE, token)};
    }
  } else if (is_numeric(current_char.value())) {
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && is_numeric(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    if (next_char != '.') {
      return {BashLexerSegment(TOK_NUMERIC, token)};
    }
    std::optional<char> next_next_char = peek_char(source, cursor);
    if (next_next_char.has_value() &&
        next_next_char.value() == '.') {  // no double dot
      return {BashLexerSegment(TOK_NUMERIC, token)};
    }

    current_char = read_char(source, cursor, token);
    next_char = peek_char(source, cursor);

    while (next_char.has_value() && is_numeric(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    return {BashLexerSegment(TOK_NUMERIC, token)};

  } else if (current_char == '#') {
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && next_char.value() != '\n') {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }
    return {BashLexerSegment(TOK_COMMENT, token)};
  } else if (current_char == '\'' || current_char == '"' ||
             (paren_map.level_map.contains(paren_map.level_counter) &&
              std::get<2>(
                  paren_map.level_map[paren_map.level_counter].value()) != 0 &&
              current_char == ')')) {
    auto start_char = current_char;
    std::vector<BashLexerSegment> ret;
    if (current_char == ')') {
      paren_map.relevant_indices.emplace_back(my_index, paren_map.level_counter,
                                              false);
      if (paren_map.level_map.contains(paren_map.level_counter)) {
        paren_map.close_map[std::get<0>(
            paren_map.level_map[paren_map.level_counter].value())] = my_index;
      }

      ret.emplace_back(TOK_CLOSE_PAREN, ")");
      ret.emplace_back(TOK_CONCAT_SILENT, "");
      start_char =
          std::get<2>(paren_map.level_map[paren_map.level_counter].value());

      paren_map.level_map[paren_map.level_counter] = {};
      paren_map.level_counter--;
    }
    std::string inner_text = "";
    bool escaping = false;
    do {
      current_char = read_char(source, cursor, token);
      if (!current_char.has_value()) {
        break;
      }

      if (current_char == '\\' && !escaping) {
        escaping = true;
      } else if (current_char == start_char && !escaping) {
      } else if (escaping) {
        inner_text.push_back('\\');
        inner_text.push_back(current_char.value());
        escaping = false;
      } else {
        if (current_char == '$' && !escaping) {
          ret.emplace_back(TOK_VALUE, inner_text);
          inner_text.clear();

          auto next_char = peek_char(source, cursor);
          if (next_char.has_value() && next_char == '(') {
            current_char = read_char(source, cursor, token);

            auto real_index = my_index + ret.size();
            paren_map.level_counter++;
            paren_map.relevant_indices.emplace_back(
                real_index, paren_map.level_counter, true);

            paren_map.level_map[paren_map.level_counter] = {real_index, true,
                                                            start_char.value()};

            ret.emplace_back(TOK_CONCAT_SILENT, "");
            ret.emplace_back(TOK_INJECT_STR, "$(");
            return ret;
          } else {
            while (next_char.has_value() &&
                   is_alpha_numeric(next_char.value())) {
              current_char = read_char(source, cursor, token);
              inner_text.push_back(current_char.value());
              next_char = peek_char(source, cursor);
            }
          }

          ret.emplace_back(TOK_CONCAT_SILENT, "");
          ret.emplace_back(TOK_IDENTIFIER, inner_text);
          ret.emplace_back(TOK_CONCAT_SILENT, "");
          inner_text.clear();
        } else {
          inner_text.push_back(current_char.value());
        }
      }
    } while (!(current_char == start_char && !escaping));
    ret.emplace_back(TOK_VALUE, inner_text);

    return ret;
  } else if (current_char == '=') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '=') {  // ==
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_EQ_EQ, token)};
    }
    return {BashLexerSegment(TOK_EQ, token)};
  } else if (current_char == '<') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '=') {  // <=
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_LESS_EQ, token)};
    }
    return {BashLexerSegment(TOK_LESS, token)};
  } else if (current_char == '>') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '=') {  // >=
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_GREATER_EQ, token)};
    }
    return {BashLexerSegment(TOK_GREATER, token)};
  } else if (current_char == '&') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '&') {  // &&
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_AND_AND, token)};
    }
    return {BashLexerSegment(TOK_AND, token)};
  } else if (current_char == '!') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '=') {  // !=
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_NOT_EQ, token)};
    }
    return {BashLexerSegment(TOK_BANG, token)};
  } else if (current_char == '|') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '|') {  // ||
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_OR_OR, token)};
    }
    return {BashLexerSegment(TOK_OR, token)};
  } else if (current_char == '.') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '.') {  // ..
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_RANGE, token)};
    }
    return {BashLexerSegment(TOK_DOT, token)};
  } else if (current_char == '(') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == ')') {  // ()
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_FUNC_INDICATOR, token)};
    }

    paren_map.level_counter++;
    paren_map.relevant_indices.emplace_back(my_index, paren_map.level_counter,
                                            true);
    paren_map.level_map[paren_map.level_counter] = {my_index, true, 0};
    return {BashLexerSegment(TOK_OPEN_PAREN, token)};
  } else if (current_char == ')') {
    paren_map.relevant_indices.emplace_back(my_index, paren_map.level_counter,
                                            false);
    if (paren_map.level_map.contains(paren_map.level_counter)) {
      paren_map.close_map[std::get<0>(
          paren_map.level_map[paren_map.level_counter].value())] = my_index;
    }

    paren_map.level_map[paren_map.level_counter] = {};
    paren_map.level_counter--;

    return {BashLexerSegment(TOK_CLOSE_PAREN, token)};
  } else if (current_char == '[') {
    return {BashLexerSegment(TOK_OPEN_SQUARE, token)};
  } else if (current_char == ']') {
    return {BashLexerSegment(TOK_CLOSE_SQUARE, token)};
  } else if (current_char == '{') {
    return {BashLexerSegment(TOK_OPEN_CURLY, token)};
  } else if (current_char == '}') {
    return {BashLexerSegment(TOK_CLOSE_CURLY, token)};
  } else if (current_char == '`') {
    return {BashLexerSegment(TOK_BACKTICK, token)};
  } else if (current_char == ':') {
    return {BashLexerSegment(TOK_COLON, token)};
  } else if (current_char == '%') {
    return {BashLexerSegment(TOK_MOD, token)};
  } else if (current_char == '-') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '-') {  // --
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_DEC, token)};
    }
    if (next_char.has_value() && next_char.value() == '=') {  // -=
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_MINUS_EQ, token)};
    }

    return {BashLexerSegment(TOK_SUB, token)};
  } else if (current_char == '+') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '+') {  // ++
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_INC, token)};
    }
    if (next_char.has_value() && next_char.value() == '=') {  // +=
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_PLUS_EQ, token)};
    }
    return {BashLexerSegment(TOK_ADD, token)};
  } else if (current_char == '*') {
    return {BashLexerSegment(TOK_MUL, token)};
  } else if (current_char == '/') {
    return {BashLexerSegment(TOK_DIV, token)};
  } else if (current_char == ',') {
    return {BashLexerSegment(TOK_COMMA, token)};
  } else if (current_char == ';') {
    return {BashLexerSegment(TOK_SEMI_COLON, token)};
  } else if (current_char == '\n') {
    return {BashLexerSegment(TOK_NEWLINE, token)};
  } else if (current_char == '~') {
    return {BashLexerSegment(TOK_TILDE, token)};
  } else if (current_char == '@') {
    return {BashLexerSegment(TOK_AT, token)};
  } else if (current_char == '\\') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.has_value() && next_char.value() == '\n') {
      current_char = read_char(source, cursor, token);
      return {BashLexerSegment(TOK_WHITESPACE, token)};
    }
    return {BashLexerSegment(TOK_BACKSLASH, token)};
  }

  std::println(stderr, "Error: Unknown token: {}", token);
  return {BashLexerSegment(TOK_UNK, token)};
}

std::vector<BashLexerSegment> paren_map_fusing(
    std::vector<BashLexerSegment> inputs, ParenMap paren_map) {
  std::set<size_t> fuse_map;
  std::set<size_t> func_map;

  for (auto index = paren_map.relevant_indices.begin();
       index != paren_map.relevant_indices.end(); index++) {
    if (index + 1 == paren_map.relevant_indices.end()) continue;
    if (!std::get<2>(*index) || !std::get<2>(*(index + 1))) continue;

    // levels are one appart
    if (std::get<1>(*(index + 1)) != std::get<1>(*index) + 1) continue;

    auto my_closer = paren_map.close_map[std::get<0>(*index)];

    auto other_closer = paren_map.close_map[std::get<0>(*(index + 1))];

    if (my_closer != other_closer + 1) continue;

    fuse_map.insert(std::get<0>(*index));
    fuse_map.insert(other_closer);
  }

  for (auto index = paren_map.relevant_indices.begin();
       index != paren_map.relevant_indices.end(); index++) {
    auto start_index = std::get<0>(*index);
    if (!paren_map.close_map.contains(start_index)) continue;
    if (paren_map.close_map[start_index] != start_index + 1) continue;
    func_map.insert(start_index);
  }

  std::vector<BashLexerSegment> fused_ret;
  for (size_t index = 0; index < inputs.size(); index++) {
    if (fuse_map.contains(index)) {
      if (inputs[index].token == TOK_OPEN_PAREN)
        fused_ret.emplace_back(TOK_OPEN_PAREN_PAREN, "((");
      else if (inputs[index].token == TOK_CLOSE_PAREN)
        fused_ret.emplace_back(TOK_CLOSE_PAREN_PAREN, "))");
      else if (inputs[index].token == TOK_INJECT_STR) {
        fused_ret.emplace_back(TOK_INJECT_MATH, "$((");
      }
      index++;
    } else {
      fused_ret.push_back(inputs[index]);
    }
  }
  return fused_ret;
}

std::string BashLexerSegment::get_token_name() {
#define TOKEN(x) \
  case x:        \
    return #x;
  switch (token) {
#include "lexer.inc"
  }
#undef TOKEN
  std::unreachable();
}

bool BashLexerSegment::get_operator_multiop() {
  switch (token) {
    case TOK_DEC:
      [[fallthrough]];
    case TOK_INC:
      return false;
    default:
      return true;
  }
  std::unreachable();
}
// we're stealing from a better language for this
// https://en.cppreference.com/w/c/language/operator_precedence.html
int16_t BashLexerSegment::get_token_precidence() {
  switch (token) {
    case TOK_ADD:
      [[fallthrough]];
    case TOK_SUB:
      return 40;

    case TOK_MUL:
      [[fallthrough]];
    case TOK_DIV:
      return 30;

    case TOK_MOD:
      return 20;

    case TOK_LESS_EQ:
      [[fallthrough]];
    case TOK_LESS:
      [[fallthrough]];
    case TOK_GREATER_EQ:
      [[fallthrough]];
    case TOK_GREATER:
      return 15;

    case TOK_NOT_EQ:
      [[fallthrough]];
    case TOK_EQ_EQ:
      return 10;

    case TOK_PLUS_EQ:
      [[fallthrough]];
    case TOK_MINUS_EQ:
      [[fallthrough]];
    case TOK_EQ:
      return 5;

    case TOK_DEC:
      return 60;

    case TOK_INC:
      return 60;

    default:
      return -1;
  }
  std::unreachable();
}

MathOp BashLexerSegment::get_math_op() {
  switch (token) {
    case TOK_ADD:
      return OP_ADD;
    case TOK_SUB:
      return OP_SUB;

    case TOK_MUL:
      return OP_MUL;
    case TOK_DIV:
      return OP_DIV;

    case TOK_LESS:
      return OP_LT;
    case TOK_LESS_EQ:
      return OP_LE;

    case TOK_GREATER:
      return OP_GT;
    case TOK_GREATER_EQ:
      return OP_GE;

    case TOK_MOD:
      return OP_MOD;
    case TOK_EQ_EQ:
      return OP_EQ_EQ;
    case TOK_NOT_EQ:
      return OP_NOT_EQ;

    case TOK_EQ:
      return OP_EQ;
    case TOK_PLUS_EQ:
      return OP_PLUS_EQ;
    case TOK_MINUS_EQ:
      return OP_MINUS_EQ;

    case TOK_INC:
      return OP_INC;
    case TOK_DEC:
      return OP_DEC;

    default:
      return OP_UNK;
  }
  std::unreachable();
}

std::string math_op_to_string(MathOp op) {
#define OP(x) \
  case x:     \
    return #x;
  switch (op) {
#include "mathop.inc"
  }
#undef TOKEN
  std::unreachable();
}
