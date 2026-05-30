#pragma once 

#include <cassert>
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "ast.h"
#include "lexer.h"

std::optional<std::unique_ptr<ExprAST>> parse_curly_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept;
std::optional<std::unique_ptr<ExprAST>> parse_floating_arg(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor);
std::optional<std::unique_ptr<ExprAST>> parse_floating_expression(
    const std::vector<BashLexerSegment>& lexer_segments, size_t& cursor) ;
std::optional<std::unique_ptr<ExprAST>> parse_call_expression(
    const std::vector<BashLexerSegment>& lexer_segments,
    size_t& cursor) noexcept;
