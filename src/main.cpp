#include "main.h"

#include <cstdio>
#include <print>

#include "CLI/CLI.hpp"

File::File(std::string contents) : d_contents(std::move(contents)) {}

int main(int argc, char* argv[]) {
  try {
    CLI::App app{"LLVM Bash compiler"};

    bool debug_general = false;
    app.add_flag("--debug", debug_general, "Print debug info to console");

    bool sandbox_general = false;
    app.add_flag("--sandbox", sandbox_general,
                 "Prevent script from changing the system");

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

    CLI11_PARSE(app, argc, argv);

    if (*compile) {
      if (!compile_bash(input_file, object_file, opt_flag, false, debug_general,
                        sandbox_general)) {
        std::println(stderr, "Compile failed");
        return 1;
      }
    } else {
      if (interpret_file.size() == 0) {
        bash_repl(debug_general, sandbox_general);
        return 0;
      }
      bash_interpret(interpret_file, debug_general, sandbox_general);
      return 0;
    }

  } catch (std::exception& e) {
    std::println(stderr, "Error creating CLI arguments: {}", e.what());
  }
  return 0;
}
