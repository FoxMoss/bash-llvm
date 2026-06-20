#pragma once
#include <optional>
#include <string>

#include "../std/main.h"

enum OptimizationFlag {
  OPT_O0 = 0,
  OPT_O1 = 1,
  OPT_O2 = 2,
  OPT_O3 = 3,
  OPT_Oz = 4,
  OPT_Os = 5
};

[[nodiscard]] bool compile_bash(std::string filename_in,
                                std::string filename_out,
                                OptimizationFlag opt_flag, bool debug_lexer,
                                bool debug_ast, SandboxingOptions sandboxing,
                                std::optional<std::string> ir_file);
void bash_repl(bool debug, SandboxingOptions sandboxing);

void bash_interpret(std::string file_name, bool debug,
                    SandboxingOptions sandboxing);

struct File {
  [[nodiscard]] const std::string& contents() const noexcept;

  static std::optional<File> open(std::string_view path);

 private:
  explicit File(std::string contents);

  std::string d_contents;
};

