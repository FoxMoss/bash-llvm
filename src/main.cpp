#include "main.h"

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <print>
#include <utility>

#include "CLI/CLI.hpp"
#include "belladonna.h"

File::File(std::string contents) : d_contents(std::move(contents)) {}

int main(int argc, char* argv[]) {
  try {
    CLI::App app{"LLVM Bash compiler"};

    bool debug_general = false;
    app.add_flag("--debug", debug_general, "Print debug info to console");

    SandboxingOptions sandboxing;
    app.add_flag("-e,--disable-external", sandboxing.block_external_programs,
                 "Prevent script from running non-builtin commands");

    app.add_flag(
        "-s,--root-sandbox", sandboxing.run_in_root_sanbox,
        "Run the program with root access but disable it from modifying files, "
        "instead creating a upatch.tar.gz patch file.");

    std::string interpret_file;
    app.add_option("file", interpret_file, "Source file for interpreting");

    CLI::App* compile = app.add_subcommand("compile", "AOT compile bash");

    compile->allow_non_standard_option_names();

    std::string input_file;
    compile->add_option("file", input_file, "Source file for codegen")
        ->required();

    std::string object_file = "out.o";
    compile->add_option("-o,--output", object_file, "Output file codegen")
        ->default_str("out.o");

    std::optional<std::string> ir_file;
    compile->add_option("--emit-ir", ir_file, "Output file for LLVM ir");

    OptimizationFlag opt_flag = OPT_O0;
    compile->add_flag("-O0{0},-O1{1},-O2{2},-O3{3},-Oz{4},-Os{5},", opt_flag,
                      "Optimization level");

    CLI11_PARSE(app, argc, argv);

    if (*compile) {
      if (sandboxing.any_features()) {
        std::println(stderr, "error: cannot use sandboxing precompiled");
        return 1;
      }
      if (!compile_bash(input_file, object_file, opt_flag, false, debug_general,
                        sandboxing, ir_file)) {
        std::println(stderr, "error: compile failed");
        return 1;
      }
    } else {
      if (sandboxing.run_in_root_sanbox) {
        auto sandbox = BelladonnaState::belladonna_create_sandbox();

        if (!sandbox.has_value()) {
          std::println(stderr, "{}", sandbox.error());
          return 1;
        }

        auto sandbox_pid = sandbox.value()->fork_into();
        if (sandbox_pid == 0) {
          if (interpret_file.size() == 0) {
            bash_repl(debug_general, sandboxing);
            exit(0);
          }
          bash_interpret(interpret_file, debug_general, sandboxing);
          exit(0);
        }

        int stat_loc;
        waitpid(sandbox_pid, &stat_loc, 0);

        delete sandbox.value();

        return WEXITSTATUS(stat_loc);
      } else {
        if (interpret_file.size() == 0) {
          bash_repl(debug_general, sandboxing);
          return 0;
        }
        bash_interpret(interpret_file, debug_general, sandboxing);
        return 0;
      }
    }

  } catch (std::exception& e) {
    std::println(stderr, "error: {}", e.what());
  }
  return 0;
}
