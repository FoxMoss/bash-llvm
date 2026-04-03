#include "lexer.h"

#include <cstddef>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

bool is_alpha(char c) {
  bool lowercase = ('a' <= c && c <= 'z');
  bool uppercase = ('A' <= c && c <= 'Z');

  return uppercase || lowercase;
}
bool is_alpha_numeric(char c) {
  bool lowercase = ('a' <= c && c <= 'z');
  bool uppercase = ('A' <= c && c <= 'Z');
  bool num = ('0' <= c && c <= '9');

  return uppercase || lowercase || num;
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

BashLexerSegment BashLexerSegment::munch_token(const std::string& source,
                                               size_t& cursor,
                                               BashLexerToken prev_token,
                                               ParenMap& paren_map) {
  ssize_t my_index = paren_map.index_counter;
  paren_map.index_counter++;
  std::string token = "";

  std::optional<char> current_char = read_char(source, cursor, token);
  if (!current_char.has_value()) {
    return BashLexerSegment(TOK_EOF, token);
  }

  if (is_whitespace(current_char.value())) {
    std::optional<char> next_char = peek_char(source, cursor);
    while (is_whitespace(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    return BashLexerSegment(TOK_WHITESPACE, token);
  } else if (prev_token == TOK_EQ) {
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && is_alpha_numeric(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    return BashLexerSegment(TOK_VALUE, token);
  } else if (is_alpha_numeric(current_char.value()) ||
             current_char == '$') {  // value, identifier, or keyword
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && is_alpha_numeric(next_char.value())) {
      current_char = read_char(source, cursor, token);
      next_char = peek_char(source, cursor);
    }

    if (!next_char.has_value() || next_char.value() != '=') {
      if (token == "if")
        return BashLexerSegment(TOK_IF, token);
      else if (token == "fi")
        return BashLexerSegment(TOK_FI, token);
      else if (token == "do")
        return BashLexerSegment(TOK_DO, token);
      else if (token == "in")
        return BashLexerSegment(TOK_IN, token);
      else if (token == "done")
        return BashLexerSegment(TOK_DONE, token);
      else if (token == "for")
        return BashLexerSegment(TOK_FOR, token);
      // TODO: finish the LUT
    }

    return BashLexerSegment(TOK_IDENTIFIER, token);
  } else if (current_char == '#') {
    std::optional<char> next_char = peek_char(source, cursor);
    while (next_char.has_value() && next_char.value() != '\n') {
      current_char = read_char(source, cursor, token);
    }
    return BashLexerSegment(TOK_COMMENT, token);
  } else if (current_char == '\'') {
    std::string inner_text = "";
    bool escaping = false;
    do {
      current_char = read_char(source, cursor, token);

      if (current_char == '\\' && !escaping) {
        escaping = true;
      } else if (current_char == '\'' && !escaping) {
      } else {
        inner_text.push_back(current_char.value());
        escaping = false;  // we're done escaping
      }

    } while (!(current_char == '\'' && !escaping));

    return BashLexerSegment(TOK_VALUE, inner_text);
  } else if (current_char == '=') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '=') {  // ==
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_EQ_EQ, token);
    }
    return BashLexerSegment(TOK_EQ, token);
  } else if (current_char == '<') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '=') {  // <=
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_LESS_EQ, token);
    }
    return BashLexerSegment(TOK_LESS, token);
  } else if (current_char == '>') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '=') {  // >=
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_GREATER_EQ, token);
    }
    return BashLexerSegment(TOK_GREATER, token);
  } else if (current_char == '&') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '&') {  // &&
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_AND_AND, token);
    }
    return BashLexerSegment(TOK_AND, token);
  } else if (current_char == '|') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '|') {  // ||
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_OR_OR, token);
    }
    return BashLexerSegment(TOK_OR, token);
  } else if (current_char == '.') {
    std::optional<char> next_char = peek_char(source, cursor);
    if (next_char.value() == '.') {  // ..
      current_char = read_char(source, cursor, token);
      return BashLexerSegment(TOK_RANGE, token);
    }
    printf("ERROR: Single period is unacceptable");
    return BashLexerSegment(TOK_UNK, token);
  } else if (current_char == '(') {
    paren_map.level_counter++;
    paren_map.relevant_indices.push_back(
        {my_index, paren_map.level_counter, true});
    paren_map.level_map[paren_map.level_counter] = {my_index, true};
    return BashLexerSegment(TOK_OPEN_PAREN, token);
  } else if (current_char == ')') {
    paren_map.relevant_indices.push_back(
        {my_index, paren_map.level_counter, false});
    paren_map.close_map[paren_map.level_map[paren_map.level_counter]->first] =
        my_index;
    paren_map.level_map[paren_map.level_counter] = {};
    paren_map.level_counter--;

    return BashLexerSegment(TOK_CLOSE_PAREN, token);
  } else if (current_char == '[') {
    return BashLexerSegment(TOK_OPEN_SQUARE, token);
  } else if (current_char == ']') {
    return BashLexerSegment(TOK_CLOSE_SQUARE, token);
  } else if (current_char == '{') {
    return BashLexerSegment(TOK_OPEN_CURLY, token);
  } else if (current_char == '}') {
    return BashLexerSegment(TOK_CLOSE_CURLY, token);
  } else if (current_char == '%') {
    return BashLexerSegment(TOK_MOD, token);
  } else if (current_char == ',') {
    return BashLexerSegment(TOK_COMMA, token);
  } else if (current_char == ';') {
    return BashLexerSegment(TOK_SEMI_COLON, token);
  } else if (current_char == '\n') {
    return BashLexerSegment(TOK_NEWLINE, token);
  }

  printf("ERROR: Unknown token: %s\n", token.c_str());
  return BashLexerSegment(TOK_UNK, token);
}

std::vector<BashLexerSegment> paren_map_fusing(
    std::vector<BashLexerSegment> inputs, ParenMap paren_map) {
  std::set<size_t> fuse_map;
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

  std::vector<BashLexerSegment> fused_ret;
  for (size_t index = 0; index < inputs.size(); index++) {
    if (fuse_map.contains(index)) {
      if (inputs[index].token == TOK_OPEN_PAREN)
        fused_ret.push_back(BashLexerSegment(TOK_OPEN_PAREN_PAREN, "(("));
      else if (inputs[index].token == TOK_CLOSE_PAREN)
        fused_ret.push_back(BashLexerSegment(TOK_CLOSE_PAREN_PAREN, "))"));
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
