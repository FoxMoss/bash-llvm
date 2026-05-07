#include <cstring>
#include <filesystem>
#include <print>
#include <string>

#include "ast.h"
#include "isocline.h"
#include "lexer.h"

static void completer(ic_completion_env_t* cenv, const char* prefix);

static void highlighter(ic_highlight_env_t* henv, const char* input, void* arg);

void bash_repl() {
  ic_style_def("kbd", "gray underline");
  ic_style_def("ic-prompt", "ansi-maroon");

  ic_set_default_completer(&completer, NULL);

  ic_set_default_highlighter(highlighter, NULL);

  ic_enable_auto_tab(true);

  std::string path = std::filesystem::current_path();

  char* input;
  while ((input = ic_readline(path.c_str())) != NULL) {
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

    for (auto token : lexer_segments) {
      std::print("[{}] {}\n", token.str, token.get_token_name());
    }

    size_t ast_cursor = 0;
    auto base = parse_compound_expression(lexer_segments, ast_cursor, true);

    base.value()->print_name(0);

    free(input);
  }
  ic_println("done");
}

static void completer(ic_completion_env_t* cenv, const char* input) {
  ic_complete_filename(cenv, input, 0, ".", NULL);
}

static void highlighter(ic_highlight_env_t* henv, const char* input,
                        void* arg) {
  auto input_len = strlen(input);
  ic_highlight(henv, 0, input_len, "white");
}
