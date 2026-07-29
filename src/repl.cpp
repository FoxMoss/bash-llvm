#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-bash.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>

#include "../std/main.h"
#include "ast/ast.h"
#include "codegen.h"
#include "isocline.h"
#include "jit.h"
#include "lexer.h"
#include "main.h"
#include "treesitter.h"

const uint8_t image_data[] = {
#embed "../repllogo.png"
};

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

static void highlighter(ic_highlight_env_t* henv, const char* input, void* arg);

const std::string shorten_path(const std::string home, const std::string path) {
  if (path.starts_with(home)) {
    return std::format("~{}", path.substr(home.size(), -1));
  }
  return path;
}

// https://github.com/zhicheng/base64/blob/master/base64.c
#define BASE64_PAD '='
static const char base64en[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};
unsigned int base64_encode(const unsigned char* in, unsigned int inlen,
                           char* out) {
  int s;
  unsigned int i;
  unsigned int j;
  unsigned char c;
  unsigned char l;

  s = 0;
  l = 0;
  for (i = j = 0; i < inlen; i++) {
    c = in[i];

    switch (s) {
      case 0:
        s = 1;
        out[j++] = base64en[(c >> 2) & 0x3F];
        break;
      case 1:
        s = 2;
        out[j++] = base64en[((l & 0x3) << 4) | ((c >> 4) & 0xF)];
        break;
      case 2:
        s = 0;
        out[j++] = base64en[((l & 0xF) << 2) | ((c >> 6) & 0x3)];
        out[j++] = base64en[c & 0x3F];
        break;
    }
    l = c;
  }

  switch (s) {
    case 1:
      out[j++] = base64en[(l & 0x3) << 4];
      out[j++] = BASE64_PAD;
      out[j++] = BASE64_PAD;
      break;
    case 2:
      out[j++] = base64en[(l & 0xF) << 2];
      out[j++] = BASE64_PAD;
      break;
  }

  out[j] = 0;

  return j;
}
#define BASE64_ENCODE_OUT_SIZE(s) ((unsigned int)((((s) + 2) / 3) * 4 + 1))

std::string get_shell_prompt(std::string home_path, std::string path,
                             bool nice_shell) {
  std::string promt_str = "";

  if (nice_shell) {
    const std::string prompt_bg = "69;71;80";
    const std::string prompt_fg = "205;214;244";
    promt_str = std::format(
        "\e[48;2;{0}m\e[38;2;{1}m {2} "
        "\e[49m\e[38;2;{0}m\e[49m\e[39m",
        prompt_bg, prompt_fg, shorten_path(home_path, path));
  } else {
    promt_str = std::format("{}$", shorten_path(home_path, path));
  }
  return promt_str;
}

void bash_repl(bool debug, SandboxingOptions sandboxing, bool nice_shell) {
  if (nice_shell) {
    std::array<uint8_t, BASE64_ENCODE_OUT_SIZE(sizeof(image_data))>
        base64_encoded;

    base64_encode(image_data, sizeof(image_data), (char*)base64_encoded.data());

    for (size_t i = 0; i < base64_encoded.size(); i += 4096) {
      auto write_size = std::min(base64_encoded.size() - i, (size_t)4096);
      std::print("\e_G{}m={};", i == 0 ? "a=T,f=100," : "",
                 write_size < 4096 ? 0 : 1);
      fwrite(base64_encoded.data() + i, 1, write_size, stdout);
      std::print("\e\\");
    }
    std::print("\n");
  }

  ic_style_def("kbd", "gray underline");
  ic_style_def("ic-prompt", catppuccin_mocha_theme["lavender"].c_str());
  ic_set_prompt_marker(" ", "> ");

  std::string home_path = "";
  if (getenv("HOME") != nullptr) {
    home_path = getenv("HOME");
  }

  std::filesystem::path history_path(".cache/llsh/history");
  history_path = home_path / history_path;
  std::filesystem::create_directories(history_path.parent_path());
  ic_set_history(std::filesystem::absolute(history_path).c_str(),
                 -1 /* default entries (= 200) */);
  ic_set_default_completer(&completer, nullptr);

  auto ts_state = alloc_ts();
  ic_set_default_highlighter(highlighter, ts_state);

  ic_enable_auto_tab(true);

  auto state = CodegenState({}, true);
  state.sandboxing = sandboxing;

  auto jit = BashJIT::create(sandboxing);
  state.module.get()->setDataLayout(jit->get()->data_layout);

  if (!jit.has_value()) {
    std::println(stderr, "jit: {}", jit.error());
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

  auto* var_mem = (VariableMemory*)create_variable_memory((void*)&sandboxing);

  std::string pwd_key = "PWD";
  std::string path = std::filesystem::current_path();

  char* input;
  while ((input = ic_readline(
              get_shell_prompt(home_path, path, nice_shell).c_str())) !=
         nullptr) {
    ic_term_writef("\e]0;%s\a", input);
    ic_term_flush();

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

    if (!base.has_value()) {
      free(input);

      // kill it otherwise the jit gets very confused
      state.module.reset();
      state.init_llvm();
      state.module->setDataLayout(jit->get()->data_layout);
      continue;
    }

    if (debug) {
      base.value()->print_name(0);
    }

    state.generate_entry();

    auto value = base.value()->codegen(state);
    if (!value.has_value()) {
      std::print(stderr, "error: {}\n", value.error());

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
      std::println(stderr, "jit: failed to make module");
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

    state.init_llvm();
    state.module->setDataLayout(jit->get()->data_layout);

    path = std::filesystem::current_path();
    free(input);

    ic_term_writef("\e]0;%s\a", shorten_path(home_path, path).c_str());
    ic_term_flush();
  }

  free_ts(ts_state);
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
