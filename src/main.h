#pragma once
#include <string>

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
                                OptimizationFlag opt_flag);
void bash_repl();

