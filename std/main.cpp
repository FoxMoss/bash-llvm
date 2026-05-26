#include "main.h"

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
#include <map>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

#define REPORT_ISSUE(str) fprintf(stderr, "llsh: %s\n", str);
#define USE_VAR_MEM()                                        \
  if (var_mem == 0) REPORT_ISSUE("Variable Memory is null"); \
  auto var_mem_usable = (VariableMemory*)var_mem;

std::vector<std::string> colapse_enviroment(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return {};
  }
  std::vector<std::string> ret;

  auto var_mem_usable = (VariableMemory*)var_mem;

  for (auto pair : var_mem_usable->memory) {
    ret.push_back(std::format("{}={}", pair.first, pair.second));
  }

  return ret;
}

extern "C" {
void bash_cd(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }

  if (argc != 1) {
    std::println(stderr, "llsh: cd: too many arguments");
    return;
  }
  const std::string pwd_key = "PWD";
  auto pwd_path =
      std::filesystem::absolute(std::filesystem::current_path() / argv[0]);
  store_variable_memory(var_mem, pwd_key.c_str(), pwd_key.size(),
                        pwd_path.string().c_str(), pwd_path.string().size());

  std::filesystem::current_path(pwd_path);
}

void bash_echo(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  if (var_mem_usable->output_stack.size() == 0) return;

  switch (
      var_mem_usable->output_stack[var_mem_usable->stack_iterator].location) {
    case OutputFactor::OUTPUT_STDOUT:
      for (uint64_t i = 0; i < argc; i++) {
        std::print("{}", argv[i]);
        if (i != argc - 1) {
          std::print(" ");
        }
      }
      std::println("");
      break;

    case OutputFactor::OUTPUT_STR:
      if (!var_mem_usable->output_stack[var_mem_usable->stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->stack_iterator].storage =
            "";
      }

      if (var_mem_usable->output_stack[var_mem_usable->stack_iterator]
              .storage.has_value()) {
        for (uint64_t i = 0; i < argc; i++) {
          var_mem_usable->output_stack[var_mem_usable->stack_iterator]
              .storage.value() += argv[i];
          if (i != argc - 1) {
            var_mem_usable->output_stack[var_mem_usable->stack_iterator]
                .storage.value() += " ";
          }
        }
        var_mem_usable->output_stack[var_mem_usable->stack_iterator]
            .storage.value() += "\n";
      }
      break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
}

void bash_printf(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  if (argc == 0) {
    return;
  }

  std::string out_string = "";
  std::string format_string = argv[0];

  bool percenting = false;
  uint64_t argument_iter = 1;
  for (char c : format_string) {
    if (percenting) {
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

      out_string.push_back(c);
    }
  }

  if (var_mem_usable->output_stack.size() == 0) return;

  switch (
      var_mem_usable->output_stack[var_mem_usable->stack_iterator].location) {
    case OutputFactor::OUTPUT_STDOUT:
      std::print("{}", out_string);
      break;

    case OutputFactor::OUTPUT_STR:
      if (!var_mem_usable->output_stack[var_mem_usable->stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->stack_iterator].storage =
            "";
      }

      var_mem_usable->output_stack[var_mem_usable->stack_iterator]
          .storage.value() += out_string;
      break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
}
void external_program(void* var_mem, char* path, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }

  auto var_mem_usable = (VariableMemory*)var_mem;

  std::string path_key = "PATH";
  auto path_var =
      get_variable_memory(var_mem, path_key.c_str(), path_key.size());
  auto path_var_len = strlen(path_var);

  std::vector<std::filesystem::path> paths;
  std::string glob;
  for (size_t i = 0; i < path_var_len; i++) {
    if (path_var[i] == ':' && !glob.empty()) {
      auto glob_path = std::filesystem::path(glob);
      glob.clear();
      if (!std::filesystem::is_directory(glob_path)) {
        continue;
      }

      paths.emplace_back(glob_path);
      continue;
    }
    glob.push_back(path_var[i]);
  }
  if (glob.empty()) {
    paths.emplace_back(glob);
  }

  std::optional<std::string> extended_path;
  for (auto dir : paths) {
    std::filesystem::directory_iterator dir_iter(dir);
    for (auto dir_file : dir_iter) {
      if (!dir_file.is_regular_file()) continue;
      if (dir_file.path().filename() != path) continue;
      extended_path = dir_file.path();
      goto found_path;
    }
  }
found_path:

  if (!extended_path.has_value()) {
    std::println(stderr, "llsh: {}: command not found", path);
    return;
  }

  switch (
      var_mem_usable->output_stack[var_mem_usable->stack_iterator].location) {
    case OutputFactor::OUTPUT_STDOUT: {
      pid_t output_id = fork();
      if (output_id == 0) {
        auto environ_data = colapse_enviroment(var_mem);
        std::vector<char*> environ_cstr;
        for (auto var : environ_data) {
          environ_cstr.push_back(strdup((char*)var.c_str()));
        }
        environ_cstr.push_back(nullptr);

        std::vector<char*> argv_vector;
        argv_vector.push_back((char*)extended_path->c_str());
        argv_vector.insert(argv_vector.end(), argv, argv + argc);
        argv_vector.push_back(nullptr);
        execve(extended_path->c_str(), argv_vector.data(), environ_cstr.data());

        for (auto var : environ_cstr) {
          free(var);
        }

        return;
      }
      int status;
      waitpid(output_id, &status, 0);
    } break;

    case OutputFactor::OUTPUT_STR: {
      std::array<int, 2> link;
      pipe(link.data());

      if (fork() == 0) {
        auto environ_data = colapse_enviroment(var_mem);
        std::vector<char*> environ_cstr;
        for (auto var : environ_data) {
          environ_cstr.push_back(strdup((char*)var.c_str()));
        }
        environ_cstr.push_back(nullptr);

        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);

        std::vector<char*> argv_vector;
        argv_vector.push_back((char*)extended_path->c_str());
        argv_vector.insert(argv_vector.end(), argv, argv + argc);
        argv_vector.push_back(nullptr);
        execve(extended_path->c_str(), argv_vector.data(), environ_cstr.data());

        for (auto var : environ_cstr) {
          free(var);
        }
        return;
      }
      close(link[1]);
      std::string output;
      std::array<char, 4096> outbuffer;

      ssize_t size = 0;

      while ((size = read(link[0], outbuffer.data(), outbuffer.size())) != 0) {
        output.insert(output.end(), outbuffer.begin(),
                      outbuffer.begin() + size);
      }

      if (!var_mem_usable->output_stack[var_mem_usable->stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->stack_iterator].storage =
            "";
      }

      var_mem_usable->output_stack[var_mem_usable->stack_iterator]
          .storage.value() += output;
    } break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
}

float str_to_float(char* str) {
  auto val = std::strtof(str, nullptr);
  return val;
}
size_t str_to_len(char* str) { return std::strlen(str); }
size_t int_len(int64_t i) {
  size_t len = std::floor(std::max(std::log10(std::abs((double)i)) + 1, 1.0));
  if (i < 0) {
    return len + 1;
  }
  return len;
}

void int_to_str(int64_t i, char* buf, size_t buf_len) {
  snprintf(buf, buf_len, "%ld", i);
}

void* create_variable_memory(bool sandboxed) {
  static void* var_mem_cache = nullptr;
  if (var_mem_cache == nullptr) {
    var_mem_cache = new VariableMemory;

    if (!sandboxed) {
      size_t environ_cursor = 0;
      while (environ[environ_cursor] != nullptr) {
        std::string line = environ[environ_cursor];
        auto first_eq = line.find_first_of('=');
        auto key = line.substr(0, first_eq);
        auto val = line.substr(first_eq + 1, line.size() - first_eq);
        store_variable_memory(var_mem_cache, key.c_str(), key.size(),
                              val.c_str(), val.size());

        environ_cursor++;
      }
    }
  }

  return var_mem_cache;
}

void store_variable_memory(void* var_mem, const char* key_str, size_t key_len,
                           const char* val_str, size_t val_len) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  var_mem_usable->memory[std::string(key_str, key_len)] =
      std::string(val_str, val_len);
}

void store_args_variable_memory(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  for (uint64_t i = 0; i < argc; i++) {
    var_mem_usable->output_stack[var_mem_usable->stack_iterator]
        .positional_arguments[std::to_string(i + 1)] = argv[i];
  }
}

const char* get_variable_memory(void* var_mem, const char* key_str,
                                size_t key_len) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return "";
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  std::string key(key_str, key_len);
  if (var_mem_usable->output_stack[var_mem_usable->stack_iterator]
          .positional_arguments.contains(key)) {
    return var_mem_usable->output_stack[var_mem_usable->stack_iterator]
        .positional_arguments[key]
        .c_str();
  }
  if (!var_mem_usable->memory.contains(key)) {
    return "";
  }
  return var_mem_usable->memory[std::string(key_str, key_len)].c_str();
}

void free_variable_memory(void* var_mem) { delete (VariableMemory*)var_mem; }

void push_output_stack(void* var_mem, uint16_t output_type) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  if (output_type >= OutputFactor::OUTPUT_UNK) {
    return;
  }

  var_mem_usable->stack_iterator++;

  var_mem_usable->output_stack[var_mem_usable->stack_iterator].location =
      (OutputFactor::OutputLocation)
          output_type;  // we can safely convert since its loc < UNK
  switch ((OutputFactor::OutputLocation)output_type) {
    case OutputFactor::OUTPUT_STDOUT:
      break;
    case OutputFactor::OUTPUT_STR:
      var_mem_usable->output_stack[var_mem_usable->stack_iterator].storage = "";
      break;
    case OutputFactor::OUTPUT_UNK:
      std::unreachable();
  }
}

// the return value stays in memory till the next frame on its level overwrites
// it
const char* pop_output_stack(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: Variable memory is null");
    return "";
  }

  auto var_mem_usable = (VariableMemory*)var_mem;
  if (var_mem_usable->stack_iterator == 0) {
    return "";
  }

  var_mem_usable->stack_iterator--;

  if (var_mem_usable->output_stack[var_mem_usable->stack_iterator + 1]
              .location == OutputFactor::OUTPUT_STR &&
      var_mem_usable->output_stack[var_mem_usable->stack_iterator + 1]
          .storage.has_value()) {
    return var_mem_usable->output_stack[var_mem_usable->stack_iterator + 1]
        .storage.value()
        .c_str();
  }

  return "";
}
}
