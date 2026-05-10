#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <print>
#include <string>

#include "ast.h"
#include "jit.h"
#include "lexer.h"
#include "main.h"

void bash_interpret(std::string file_name, bool debug, bool sandbox) {
  auto source_file = File::open(file_name);
  if (!source_file.has_value()) {
    std::println(stderr, "Error {} does not exist.", file_name);
  }
  std::string file_contents = source_file->contents();

  size_t cursor = 0;

  std::optional<std::vector<BashLexerSegment>> last_token;
  std::vector<BashLexerSegment> lexer_segments;
  ParenMap paren_map;

  do {
    paren_map.index_counter = lexer_segments.size();
    last_token = BashLexerSegment::munch_token(
        file_contents, cursor,
        last_token.has_value() ? last_token->back().token : TOK_UNK, paren_map);

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

  auto state = CodegenState(base.value()->get_functions_defined(), true);
  state.is_sandboxed = sandbox;

  auto jit = BashJIT::create(sandbox);
  if (!jit.has_value()) {
    std::println(stderr, "Couldn't make JIT: {}", jit.error());
  }

  state.module.get()->setDataLayout(jit->get()->data_layout);

  llvm::FunctionType* entry_type =
      llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);

  state.entry = llvm::Function::Create(
      entry_type, llvm::Function::ExternalLinkage, "main", state.module.get());

  llvm::BasicBlock* entry_block =
      llvm::BasicBlock::Create(*state.context, "entry", state.entry);
  state.builder->SetInsertPoint(entry_block);

  state.generate_variable_memory();
  if (!runtime_push_output_stack(state, 0).has_value()) {
    std::println("Error while pushing stack");
  }

  auto value = base.value()->codegen(state);
  if (!value.has_value()) {
    std::print(stderr, "Error: {}", value.error());
    return;
  }

  if (!runtime_pop_output_stack(state).has_value()) {
    std::println(stderr, "Error while popping stack");
    return;
    ;
  }

  state.builder->CreateRetVoid();

  auto resource_tracker = jit->get()->main_jit_dylib.createResourceTracker();
  auto thread_safe_module = llvm::orc::ThreadSafeModule(
      std::move(state.module), std::move(state.context));
  auto added_module =
      jit->get()->add_module(std::move(thread_safe_module), resource_tracker);
  if (!added_module.has_value()) {
    std::print(stderr, "Failed to add module");
    return;
  }

  auto expr_symbol = jit->get()->lookup("main");
  if (!expr_symbol.has_value()) {
    std::print(stderr, "Failed to find expr");
    return;
  }

  auto main_func = expr_symbol->toPtr<void (*)()>();
  main_func();

  auto err = resource_tracker->remove();

  if (err) {
    std::println(stderr, "Couldn't free resource_tracker.");
  }
}
