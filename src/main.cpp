#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ast.h"
#include "codegen.h"
#include "lexer.h"

struct File {
  const std::string& contents() const noexcept;

  static std::optional<File> open(std::string_view path);

 private:
  explicit File(std::string contents);

  std::string d_contents;
};
File::File(std::string contents) : d_contents(std::move(contents)) {}

const std::string& File::contents() const noexcept { return d_contents; }

std::optional<File> File::open(std::string_view file_name) {
  std::filesystem::path path{file_name};
  std::ifstream file{path};
  if (!file) return std::nullopt;

  std::stringstream buffer;
  buffer << file.rdbuf();
  return File{buffer.str()};
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::print("USAGE: {} [filename]\n", argv[0]);
    return 1;
  }
  bool print_lexed = true;
  bool print_ast = true;

  auto source_file = File::open(argv[1]);
  size_t cursor = 0;

  std::optional<std::vector<BashLexerSegment>> last_token;
  std::vector<BashLexerSegment> lexer_segments;
  ParenMap paren_map;

  do {
    last_token = BashLexerSegment::munch_token(
        source_file->contents(), cursor,
        last_token.has_value() ? last_token->back().token : TOK_UNK, paren_map);

    // must have value so we don't need to check
    lexer_segments.insert(lexer_segments.end(), last_token.value().begin(),
                          last_token.value().end());
  } while (last_token->back().token != TOK_EOF);

  lexer_segments = paren_map_fusing(lexer_segments, paren_map);

  if (print_lexed) {
    for (auto token : lexer_segments) {
      std::print("[{}] {}\n", token.str, token.get_token_name());
    }
  }

  size_t ast_cursor = 0;
  auto base = parse_compound_expression(lexer_segments, ast_cursor);

  if (print_ast && base.has_value()) {
    base.value()->print_name(0);
  }

  if (!base.has_value()) {
    std::print("Error while parsing\n");
    return 1;
  }

  CodegenState state;
  auto value = base.value()->codegen(state);
  if (!value.has_value()) {
    std::print("Error: {}\n", value.error());
    return 1;
  }
  state.builder->CreateRetVoid();

  std::error_code error;
  llvm::raw_fd_ostream out_file("out.ll", error);
  state.module->print(out_file, NULL);

  return 0;
}
