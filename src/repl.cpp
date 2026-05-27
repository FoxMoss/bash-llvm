#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-bash.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <print>
#include <string>

#include "../std/main.h"
#include "ast.h"
#include "codegen.h"
#include "isocline.h"
#include "jit.h"
#include "lexer.h"
#include "treesitter.h"

struct TSState {
  TSParser* parser;
  TSTree* tree;
  TSQuery* query;
};
void* alloc_ts() {
  auto* ret = (TSState*)malloc(sizeof(TSState));

  ret->parser = ts_parser_new();
  if (ret->parser == nullptr) {
    return nullptr;
  }

  ts_parser_set_language(ret->parser, tree_sitter_bash());

  static uint32_t error_offset;
  static TSQueryError error_type;
  ret->query =
      ts_query_new(tree_sitter_bash(), highlighter_query.c_str(),
                   highlighter_query.size(), &error_offset, &error_type);

  if (ret->query == nullptr) {
    std::println(stderr, "Tree Sitter failed at {} in query with error {}",
                 error_offset, (int)error_type);
    return nullptr;
  }

  ret->tree = nullptr;
  return ret;
}

void free_ts(void* ts_state_raw) {
  auto* ts_state = (TSState*)ts_state_raw;
  ts_query_delete(ts_state->query);
  if (ts_state->tree != nullptr) {
    ts_tree_delete(ts_state->tree);
  }
  ts_parser_delete(ts_state->parser);
  free(ts_state_raw);
}
static void completer(ic_completion_env_t* cenv, const char* prefix);

static void highlighter(ic_highlight_env_t* henv, const char* input, void* arg);

void bash_repl(bool debug, bool sandbox) {
  ic_style_def("kbd", "gray underline");
  ic_style_def("ic-prompt", catppuccin_mocha_theme["lavender"].c_str());
  ic_set_prompt_marker("$ ", "> ");

  ic_set_history(nullptr, -1 /* default entries (= 200) */);
  ic_set_default_completer(&completer, nullptr);

  auto ts_state = alloc_ts();
  ic_set_default_highlighter(highlighter, ts_state);

  ic_enable_auto_tab(true);

  auto state = CodegenState({}, true);
  state.is_sandboxed = sandbox;

  auto jit = BashJIT::create(sandbox);
  state.module.get()->setDataLayout(jit->get()->data_layout);

  if (!jit.has_value()) {
    std::println(stderr, "Couldn't make JIT: {}", jit.error());
  }

  auto resource_tracker = jit->get()->main_jit_dylib.createResourceTracker();
  auto thread_safe_module = llvm::orc::ThreadSafeModule(
      std::move(state.module), std::move(state.context));
  auto added_module =
      jit->get()->add_module(std::move(thread_safe_module), resource_tracker);
  if (!added_module.has_value()) {
    std::print(stderr, "Failed to add module");
    return;
  }
  state.init_llvm();
  state.module->setDataLayout(jit->get()->data_layout);

  auto* var_mem = (VariableMemory*)create_variable_memory(sandbox);

  std::string pwd_key = "PWD";
  std::string path = std::filesystem::current_path();

  char* input;
  while ((input = ic_readline(path.c_str())) != nullptr) {
    store_variable_memory(var_mem, pwd_key.c_str(), pwd_key.size(),
                          path.c_str(), path.size());
    size_t cursor = 0;

    std::optional<std::vector<BashLexerSegment>> last_token;
    std::vector<BashLexerSegment> lexer_segments;
    ParenMap paren_map;

    std::string file_contents(input);
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

    if (debug) {
      base.value()->print_name(0);
    }

    state.generate_entry();

    auto value = base.value()->codegen(state);
    if (!value.has_value()) {
      std::print(stderr, "Error: {}\n", value.error());

      free(input);

      // kill it otherwise the jit gets very confused
      state.module.reset();
      state.init_llvm();
      state.module->setDataLayout(jit->get()->data_layout);
      continue;
    }

    state.generate_exit(true);

    auto resource_tracker = jit->get()->main_jit_dylib.createResourceTracker();

    auto thread_safe_module = llvm::orc::ThreadSafeModule(
        std::move(state.module), std::move(state.context));
    auto added_module =
        jit->get()->add_module(std::move(thread_safe_module), resource_tracker);
    if (!added_module.has_value()) {
      std::println(stderr, "Failed to make module");
      return;
    }

    auto expr_symbol = jit->get()->lookup("main");
    if (!expr_symbol.has_value()) {
      std::println(stderr, "Failed to find expr");
      return;
    }

    auto main_func = expr_symbol->toPtr<void (*)()>();
    main_func();

    auto err = resource_tracker->remove();

    if (err) {
      std::println(stderr, "Couldn't free resource_tracker.");
    }

    state.init_llvm();
    state.module->setDataLayout(jit->get()->data_layout);

    path = std::filesystem::current_path();
    free(input);
  }

  free_ts(ts_state);
}

static void completer(ic_completion_env_t* cenv, const char* input) {
  ic_complete_filename(cenv, input, 0, ".", nullptr);
}

static void highlighter(ic_highlight_env_t* henv, const char* input,
                        void* arg) {
  auto* ts_state = (TSState*)arg;
  std::string full_str(input);

  auto old_tree = ts_parser_parse_string(ts_state->parser, nullptr,
                                         full_str.c_str(), full_str.size());

  auto query_cursor = ts_query_cursor_new();
  ts_query_cursor_exec(query_cursor, ts_state->query,
                       ts_tree_root_node(old_tree));

  TSQueryMatch match;
  while (ts_query_cursor_next_match(query_cursor, &match)) {
    for (size_t i = 0; i < match.capture_count; i++) {
      uint32_t id_length;
      const char* id_c_str = ts_query_capture_name_for_id(
          ts_state->query, match.captures[i].index, &id_length);
      std::string id_str(id_c_str,
                         std::find(id_c_str, id_c_str + id_length, '.'));

      std::optional<std::string> color;
      if (query_to_theme.contains(id_str)) {
        color = catppuccin_mocha_theme[query_to_theme[id_str]];
      }

      auto str_cursor = ts_node_start_byte(match.captures[i].node);
      auto str_len = ts_node_end_byte(match.captures[i].node) - str_cursor;
      if (color.has_value()) {
        ic_highlight(henv, str_cursor, str_len, color.value().c_str());
      }
    }
  }

  ts_query_cursor_delete(query_cursor);
  ts_tree_delete(old_tree);
}
