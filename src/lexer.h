#pragma once

#include <llvm/IR/Value.h>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define TOKEN(x) x,
enum BashLexerToken {
#include "lexer.inc"
};
#undef TOKEN

#define OP(x) x,
enum MathOp {
#include "mathop.inc"
};
#undef OP

std::string math_op_to_string(MathOp op);

struct ParenMap {
  size_t index_counter = 0;  // this always incs
  ssize_t level_counter = 0;

  // level_map[level] = {{index, open_or_close_paren}}
  std::map<size_t, std::optional<std::pair<size_t, bool>>> level_map;
  std::unordered_map<size_t, size_t> close_map;
  // where the revelant indicies is and on what level
  std::vector<std::tuple<size_t, size_t, bool>> relevant_indices;
};

class BashLexerSegment {
 public:
  BashLexerSegment(BashLexerToken token, std::string str)
      : token(token), str(str) {}
  ~BashLexerSegment() {}

  BashLexerToken token;
  std::string str;

  std::string get_token_name();
  int16_t get_token_precidence();
  MathOp get_math_op();

  static BashLexerSegment munch_token(const std::string& source, size_t& cursor,
                                      BashLexerToken prev_token,
                                      ParenMap& paren_map);

 private:
};

std::vector<BashLexerSegment> paren_map_fusing(
    std::vector<BashLexerSegment> inputs, ParenMap paren_map);

