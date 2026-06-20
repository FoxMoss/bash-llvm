#pragma once

#include <cassert>
#include <cstdio>
#include <optional>
#include <vector>

#include "ast.h"
#include "lexer.h"

std::optional<BashLexerSegment> get_current_segment(
    const std::vector<BashLexerSegment>& lexer_segments, const size_t& cursor);
std::optional<BashLexerSegment> get_next_segment(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<BashLexerSegment> peek_segment(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
void skip_whitespace_and_newline(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
void skip_whitespace(const std::vector<BashLexerSegment>& lexer_segments,
                     size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_identifier(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_value(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_numeric(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_identifier_or_numeric(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_identifier_or_value(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);

#define RETURN_WITH_WARNING()                                                 \
  auto loc = std::source_location::current();                                 \
  std::print(stderr, "error: {}:{}:{}\nin {}\n", loc.file_name(), loc.line(), \
             loc.column(), loc.function_name());                              \
  return {};

#define UNEXPECTED_RETURN_WITH_WARNING()                          \
  auto loc = std::source_location::current();                     \
  return std::unexpected(std::format("error: {}:{}:{}\nin {}\n",  \
                                     loc.file_name(), loc.line(), \
                                     loc.column(), loc.function_name()));

#define RETURN_WITH_MSG(msg)                                          \
  auto loc = std::source_location::current();                         \
  std::print(stderr, "error: {}:{}:{}\nin {}\n{}\n", loc.file_name(), \
             loc.line(), loc.column(), loc.function_name(), msg);     \
  return {};

#define UNEXPECTED_RETURN_WITH_MSG(msg)                              \
  auto loc = std::source_location::current();                        \
  return std::unexpected(std::format("error: {}:{}:{}\nin {}\n{}\n", \
                                     loc.file_name(), loc.line(),    \
                                     loc.column(), loc.function_name(), msg));

