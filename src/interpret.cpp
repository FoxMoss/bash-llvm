#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <print>
#include <string>

#include "ast/ast.h"
#include "jit.h"
#include "lexer.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/SHA256.h"
#include "main.h"

void bash_interpret(std::string file_name, bool debug,
                    SandboxingOptions sandboxing) {
  std::optional<CodegenState> state;
  std::expected<std::unique_ptr<BashJIT>, std::string> jit;

  auto source_file = File::open(file_name);
  if (!source_file.has_value()) {
    std::println(stderr, "error: {} does not exist.", file_name);
    return;
  }
  std::string file_contents = source_file->contents();

  std::filesystem::path interp_file(std::format("{}.llsh", file_name));
  if (!sandboxing.dont_cache && std::filesystem::exists(interp_file)) {
    std::ifstream bc_file(interp_file);

    std::string magic_header(strlen(CACHE_MAGIC), 0);
    bc_file.read(magic_header.data(), (long)strlen(CACHE_MAGIC));
    if (magic_header != CACHE_MAGIC) {
      goto jit_from_source;
    }

    llvm::SHA256 file_hash_calculator;
    file_hash_calculator.update(file_contents);
    auto truth_file_hash = file_hash_calculator.final();

    std::array<uint8_t, 32> declared_file_hash;
    bc_file.read((char*)declared_file_hash.data(), 32);
    if (declared_file_hash != truth_file_hash) {
      goto jit_from_source;
    }

    std::stringstream bitcode_data;
    bitcode_data << bc_file.rdbuf();

    std::unique_ptr<llvm::LLVMContext> context =
        std::make_unique<llvm::LLVMContext>();

    auto module = llvm::parseBitcodeFile(
        llvm::MemoryBufferRef(bitcode_data.str(), "bash"), *context);

    if (!module) {
      if (debug) {
        std::println(stderr, "error: module construction failed, {}",
                     llvm::toString(module.takeError()));
      }
      goto jit_from_source;
    }

    state = CodegenState(std::move(context), std::move(module.get()), true);
    state->sandboxing = sandboxing;

    jit = BashJIT::create(sandboxing);
    if (!jit.has_value()) {
      std::println(stderr, "jit: {}", jit.error());
    }

    state->module.get()->setDataLayout(jit->get()->data_layout);
  } else {
  jit_from_source:

    size_t cursor = 0;

    std::optional<std::vector<BashLexerSegment>> last_token;
    std::vector<BashLexerSegment> lexer_segments;
    ParenMap paren_map;

    do {
      paren_map.index_counter = lexer_segments.size();
      last_token = BashLexerSegment::munch_token(
          file_contents, cursor,
          last_token.has_value() ? last_token->back().token : TOK_UNK,
          paren_map);

      // must have value so we don't need to check
      lexer_segments.insert(lexer_segments.end(), last_token.value().begin(),
                            last_token.value().end());
    } while (last_token->back().token != TOK_EOF);

    lexer_segments = paren_map_fusing(lexer_segments, paren_map);

    if (debug) {
      for (auto token : lexer_segments) {
        std::print("[{}] {}\n", token.str, token.get_token_name());
      }
    }

    size_t ast_cursor = 0;
    auto base = parse_compound_expression(lexer_segments, ast_cursor, true);
    if (!base.has_value()) {
      std::println(stderr, "Syntax error.");
      return;
    }

    if (debug) {
      base.value()->print_name(0);
    }

    state = CodegenState(base.value()->get_functions_defined(), true);
    state->sandboxing = sandboxing;

    state->generate_entry();

    auto value = base.value()->codegen(state.value());
    if (!value.has_value()) {
      std::print(stderr, "error: {}\n", value.error());
      return;
    }

    state->generate_exit(false);

    jit = BashJIT::create(sandboxing);
    if (!jit.has_value()) {
      std::println(stderr, "jit: {}", jit.error());
    }

    state->module.get()->setDataLayout(jit->get()->data_layout);

    if (!sandboxing.dont_cache) {
      llvm::SHA256 file_hash_calculator;
      file_hash_calculator.update(file_contents);
      auto file_hash = file_hash_calculator.final();

      std::error_code error_code;
      llvm::raw_fd_ostream bc_out_file(interp_file.string(), error_code);

      if (error_code) {
        std::println(stderr, "error: {}", error_code.message());
        return;
      }

      bc_out_file << "LLSH-BC-FMT-0001";  // magic
      bc_out_file.write((const char*)file_hash.data(),
                        file_hash.size());  // hash

      llvm::WriteBitcodeToFile(*state->module, bc_out_file);
    }
  }

  auto resource_tracker = jit->get()->main_jit_dylib.createResourceTracker();
  auto thread_safe_module = llvm::orc::ThreadSafeModule(
      std::move(state->module), std::move(state->context));
  auto added_module =
      jit->get()->add_module(std::move(thread_safe_module), resource_tracker);
  if (!added_module.has_value()) {
    std::print(stderr, "jit: failed to add module");
    return;
  }

  auto expr_symbol = jit->get()->lookup("main");
  if (!expr_symbol.has_value()) {
    std::println(stderr, "jit: failed to find expr");
    return;
  }

  auto main_func = expr_symbol->toPtr<void (*)()>();
  main_func();

  auto err = resource_tracker->remove();

  if (err) {
    std::println(stderr, "jit: couldn't free resource_tracker.");
  }
}
