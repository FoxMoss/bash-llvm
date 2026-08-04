
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <ostream>
#include <print>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "main.h"

extern "C" {

// cases to handle
// shopt -s nocasematch
// shopt -s eval_unsafe_arith &>/dev/null
// shopt -u nullglob

int bash_shopt(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return 1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  bool set = false;
  bool unset = false;
  std::optional<std::string> option;
  for (size_t i = 0; i < argc; i++) {
    auto arg = std::string(argv[i]);

    // https://www.gnu.org/software/bash/manual/html_node/The-Shopt-Builtin.html
    // it is more complicated then this...
    // TODO: finish impl
    if (arg.starts_with("-")) {
      if (arg == "-s") {
        set = true;
      } else if (arg == "-u") {
        unset = true;
      }
    } else {
      option = arg;
    }
  }

  if (set && unset) {
    std::println(stderr, "llsh: shopt: cannot both set and unset");

    return 1;
  }
  if (!option.has_value()) {
    std::println(stderr, "llsh: shopt: option not specified");

    return 1;
  }

  if (set) {
    var_mem_usable->shell_options[option.value()] = true;
  } else if (unset) {
    var_mem_usable->shell_options[option.value()] = false;
  } else {
    /*          	 */
    /*             */
    std::println("{:20}\t{}", option.value(),
                 var_mem_usable->shell_options[option.value()] ? "on" : "off");
  }

  return 1;
}

int bash_cd(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return 1;
  }

  if (argc != 1) {
    std::println(stderr, "llsh: cd: too many arguments");
    return 1;
  }

  std::string path(argv[0]);
  const std::string pwd_key = "PWD";
  const std::string home_key = "HOME";

  auto home_path = std::string(
      get_variable_memory(var_mem, home_key.c_str(), home_key.size()));
  if (home_path.empty()) {
    home_path = "/";
  }

  if (path.starts_with("~")) {
    path = path.substr(1, -1);
    path = home_path;
  }

  auto pwd_path =
      std::filesystem::absolute(std::filesystem::current_path() / path);

  if (!std::filesystem::exists(pwd_path)) {
    std::println(stderr, "llsh: cd: {} does not exist", pwd_path.string());
    return 1;
  }

  store_variable_memory(var_mem, pwd_key.c_str(), pwd_key.size(),
                        pwd_path.string().c_str(), pwd_path.string().size());

  std::filesystem::current_path(pwd_path);

  return 0;
}

int bash_echo(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return 1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  if (var_mem_usable->output_stack.size() == 0) return 1;

  std::string output;

  for (uint64_t i = 0; i < argc; i++) {
    output += std::string(argv[i]);
    if (i != argc - 1 && i != argc - 1) {
      output += " ";
    }
  }
  output += "\n";

  size_t bonus_newlines = 0;

  for (char& iter : std::views::reverse(output)) {
    if (iter != '\n') {
      break;
    }
    bonus_newlines++;
  }

  output = output.substr(0, output.size() - (bonus_newlines - 1));

  switch (var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
              .location) {
    case OutputFactor::OUTPUT_STDOUT:
      std::printf("%s", output.c_str());
      break;

    case OutputFactor::OUTPUT_STR:
      if (!var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
            .storage = "";
      }

      if (var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
              .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
            .storage.value() += output;
      }
      break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
  return 0;
}

int bash_printf(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return 1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  if (argc == 0) {
    return 1;
  }

  std::string out_string = "";
  std::string format_string = argv[0];

  bool percenting = false;
  bool escaping = false;
  uint64_t argument_iter = 1;
  for (char c : format_string) {
    if (escaping) {
      if (c == 'n') {
        out_string.push_back('\n');
        escaping = false;
      } else {
        out_string.push_back(c);
        escaping = false;
      }
    } else if (percenting) {
      if (c == '%') {
        out_string.push_back('%');
      } else if (c == 'd') {
        if (argument_iter < argc) {
          out_string.append(argv[argument_iter]);
        } else {
          out_string.append("0");
        }
        argument_iter++;
      } else if (c == 's') {
        if (argument_iter < argc) {
          out_string.append(argv[argument_iter]);
        } else {
          out_string.append("");
        }
        argument_iter++;
      }

      percenting = false;
    } else {
      if (c == '%') {
        percenting = true;
        continue;
      }
      if (c == '\\') {
        escaping = true;
        continue;
      }
      out_string.push_back(c);
    }
  }

  if (var_mem_usable->output_stack.size() == 0) return 1;

  switch (var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
              .location) {
    case OutputFactor::OUTPUT_STDOUT:
      std::print("{}", out_string);
      break;

    case OutputFactor::OUTPUT_STR:
      if (!var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
            .storage = "";
      }

      var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
          .storage.value() += out_string;
      break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
  return 0;
}
}
