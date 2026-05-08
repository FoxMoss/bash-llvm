#include "main.h"

#include <cstdio>

#include "CLI/CLI.hpp"

File::File(std::string contents) : d_contents(std::move(contents)) {}

int main(int argc, char* argv[]) {
  CLI::App app{"LLVM Bash compiler"};

  bool debug_general = false;
  app.add_flag("--debug", debug_general, "Print debug info to console");

  std::string interpret_file;
  app.add_option("file", interpret_file, "Source file for interpreting");

  CLI::App* compile = app.add_subcommand("compile", "AOT compile bash");

  compile->allow_non_standard_option_names();

  std::string input_file;
  compile->add_option("file", input_file, "Source file for codegen")
      ->required();

  std::string object_file = "out.o";
  compile->add_option("-o", object_file, "Output file codegen")
      ->default_str("out.o");

  OptimizationFlag opt_flag = OPT_O0;
  compile->add_flag("-O0{0},-O1{1},-O2{2},-O3{3},-Oz{4},-Os{5},", opt_flag,
                    "Optimization level");

  bool debug_ast = false;
  compile->add_flag("--debug-ast", debug_ast, "Print out the AST");

  CLI11_PARSE(app, argc, argv);

  if (*compile) {
    if (!compile_bash(input_file, object_file, opt_flag, false, debug_ast)) {
      fprintf(stderr, "Compile failed\n");
      return 1;
    }
  } else {
    if (interpret_file.size() == 0) {
      bash_repl(debug_general);
      return 0;
    }
    bash_interpret(interpret_file, debug_general);
    return 0;
  }

  return 0;
}
