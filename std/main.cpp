#include "main.h"

#include <glob.h>  // this should be good on mac and linux
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
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
  if (var_mem == 0) REPORT_ISSUE("variable Memory is null"); \
  auto var_mem_usable = (VariableMemory*)var_mem;

std::vector<std::string> colapse_enviroment(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return {};
  }
  std::vector<std::string> ret;

  auto var_mem_usable = (VariableMemory*)var_mem;

  for (auto pair : var_mem_usable->memory) {
    ret.push_back(std::format("{}={}", pair.first, pair.second));
  }

  return ret;
}

std::vector<std::string> expand_program_argument(void* var_mem, char* arg) {
  glob_t glob_buf;

  glob(arg, GLOB_TILDE, nullptr, &glob_buf);

  std::vector<std::string> ret;

  for (size_t i = 0; i < glob_buf.gl_pathc; i++) {
    ret.emplace_back(glob_buf.gl_pathv[i]);
  }

  globfree(&glob_buf);

  if (ret.size() == 0) {
    ret.emplace_back(arg);
  }

  return ret;
}

extern "C" {

bool get_shell_opt(void* var_mem, const char* arg) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return false;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  return var_mem_usable->shell_options[arg];
}

int external_program(void* var_mem, char* path, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return 1;
  }

  auto var_mem_usable = (VariableMemory*)var_mem;

  std::optional<std::string> extended_path;
  std::string executable(path);

  if (!executable.starts_with(".")) {
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
    paths.emplace_back("/");
    paths.emplace_back(std::filesystem::current_path());

    for (auto dir : paths) {
      std::filesystem::path dir_file = dir / path;
      if (!std::filesystem::exists(dir_file)) continue;
      if (!std::filesystem::is_regular_file(dir_file)) continue;
      extended_path = dir_file;
      goto found_path;
    }
  } else {
    std::filesystem::path local_path = std::filesystem::current_path() / path;
    if (!std::filesystem::exists(local_path)) goto found_path;
    if (!std::filesystem::is_regular_file(local_path)) goto found_path;

    extended_path = local_path;
  }
found_path:

  if (!extended_path.has_value()) {
    std::println(stderr, "llsh: {}: command not found", path);
    return 1;
  }

  switch (var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
              .location) {
    case OutputFactor::OUTPUT_STDOUT: {
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

      exit(0);
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
        return 1;
      }
      close(link[1]);
      std::string output;
      std::array<char, 4096> outbuffer;

      ssize_t size = 0;

      while ((size = read(link[0], outbuffer.data(), outbuffer.size())) != 0) {
        output.insert(output.end(), outbuffer.begin(),
                      outbuffer.begin() + size);
      }

      if (!var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
            .storage = "";
      }

      var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
          .storage.value() += output;
    } break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }

  return 0;
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

bool strequals(void* var_mem, const char* a, const char* b, bool for_case) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return false;
  }

  bool case_insensitive = true;

  if (for_case) {
    case_insensitive = get_shell_opt(var_mem, "nocasematch");
  }

  auto a_len = strlen(a);
  auto b_len = strlen(b);

  size_t a_cursor = 0;
  size_t b_cursor = 0;

  while (a_cursor < a_len && b_cursor < b_len) {
    auto b_val = b[b_cursor];
    if (case_insensitive && b_val >= 'A' && b_val <= 'Z') {
      b_val += 'a' - 'A';  // everything to lowercase
    }
    char b_peek = 0;
    if (b_cursor + 1 < b_len) {
      b_peek = b[b_cursor + 1];
    }
    auto a_val = a[a_cursor];
    if (case_insensitive && a_val >= 'A' && a_val <= 'Z') {
      a_val += 'a' - 'A';
    }

    if (a_val == b_val) {
      a_cursor++;
      b_cursor++;
      continue;
    } else if (b_val == '*') {
      a_cursor++;
      if (b_peek == a_val) {
        b_cursor += 2;  // skip the * and the char
      }
      continue;
    }

    return false;
  }

  if (a_cursor == a_len &&
      (b_cursor == b_len || (b[b_cursor] == '*' && b_cursor == b_len - 1))) {
    return true;
  }
  return false;
}

void* create_variable_memory(void* sandboxing_raw) {
  SandboxingOptions sandboxing;
  if (sandboxing_raw != nullptr) {
    sandboxing = *(SandboxingOptions*)sandboxing_raw;
  }

  static void* var_mem_cache = nullptr;
  if (var_mem_cache == nullptr) {
    var_mem_cache = new VariableMemory;

    if (!sandboxing.block_external_programs) {
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
    std::println(stderr, "llsh: variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  var_mem_usable->memory[std::string(key_str, key_len)] =
      std::string(val_str, val_len);
}

void store_args_variable_memory(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  for (uint64_t i = 0; i < argc; i++) {
    var_mem_usable->function_stack[var_mem_usable->function_stack_iterator]
        .positional_arguments[std::to_string(i + 1)] = argv[i];
  }
}

const char* get_variable_memory(void* var_mem, const char* key_str,
                                size_t key_len) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return "";
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  std::string key(key_str, key_len);
  if (var_mem_usable->function_stack[var_mem_usable->function_stack_iterator]
          .positional_arguments.contains(key)) {
    return var_mem_usable
        ->function_stack[var_mem_usable->function_stack_iterator]
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
    std::println(stderr, "llsh: variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  if (output_type >= OutputFactor::OUTPUT_UNK) {
    return;
  }

  var_mem_usable->output_stack_iterator++;

  var_mem_usable->output_stack[var_mem_usable->output_stack_iterator].location =
      (OutputFactor::OutputLocation)
          output_type;  // we can safely convert since its loc < UNK
  switch ((OutputFactor::OutputLocation)output_type) {
    case OutputFactor::OUTPUT_STDOUT:
      break;
    case OutputFactor::OUTPUT_STR:
      var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
          .storage = "";
      break;
    case OutputFactor::OUTPUT_UNK:
      std::unreachable();
  }
}

void push_function_stack(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  var_mem_usable->function_stack_iterator++;
  var_mem_usable->function_stack[var_mem_usable->function_stack_iterator] = {};
}
// the return value stays in memory till the next frame on its level overwrites
// it
const char* pop_output_stack(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return "";
  }

  auto var_mem_usable = (VariableMemory*)var_mem;
  if (var_mem_usable->output_stack_iterator == 0) {
    return "";
  }

  var_mem_usable->output_stack_iterator--;

  if (var_mem_usable->output_stack[var_mem_usable->output_stack_iterator + 1]
              .location == OutputFactor::OUTPUT_STR &&
      var_mem_usable->output_stack[var_mem_usable->output_stack_iterator + 1]
          .storage.has_value()) {
    return var_mem_usable
        ->output_stack[var_mem_usable->output_stack_iterator + 1]
        .storage.value()
        .c_str();
  }

  return "";
}
void pop_function_stack(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return;
  }

  auto var_mem_usable = (VariableMemory*)var_mem;
  if (var_mem_usable->function_stack_iterator == 0) {
    return;
  }

  var_mem_usable->function_stack_iterator--;

  return;
}

int fork_process_and_capture_stdin(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return -1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  var_mem_usable->output_stack_iterator++;

  var_mem_usable->output_stack[var_mem_usable->output_stack_iterator].location =
      OutputFactor::OUTPUT_STDOUT;

  std::array<int, 2> impl_link;
  if (pipe(impl_link.data()) == -1) {
    auto p = errno;
    std::println(stderr, "error: {} {}", p, strerror(p));
  }

  var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
      .private_stdin = impl_link[1];

  auto forked_id = fork();

  if (forked_id == 0) {
    int dup_ret = dup2(impl_link[0], STDIN_FILENO);  // read end
    if (dup_ret == -1) {
      std::println(stderr, "error: {}", strerror(errno));
    }

    close(impl_link[0]);
    close(impl_link[1]);
  } else {
    close(impl_link[0]);
  }

  return forked_id;
}

int fork_process_and_capture_stdout(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return -1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  auto lower_stdin =
      var_mem_usable->output_stack[var_mem_usable->output_stack_iterator]
          .private_stdin;

  var_mem_usable->output_stack_iterator++;

  var_mem_usable->output_stack[var_mem_usable->output_stack_iterator].location =
      OutputFactor::OUTPUT_STDOUT;

  auto forked_id = fork();

  if (forked_id == 0) {
    if (lower_stdin.has_value()) {
      int dup_ret = dup2(lower_stdin.value(), STDOUT_FILENO);
      if (dup_ret == -1) {
        std::println(stderr, "error: {}", strerror(errno));
      }
      close(lower_stdin.value());
    }

  } else {
    if (lower_stdin.has_value()) {
      close(lower_stdin.value());
    }
  }

  return forked_id;
}
int fork_process(void* var_mem) {
  if (var_mem == nullptr) {
    std::println(stderr, "llsh: variable memory is null");
    return -1;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  return fork();
}
int exit_helper(int status) { exit(status); }
int wait_two_pid(void* var_mem, int pid1, int pid2) {
  if (pid2 == 0) {
    int stat_loc1 = 0;

    waitpid(pid1, &stat_loc1, 0);
    return WIFEXITED(stat_loc1);
  }

  int stat_loc1 = 0;
  waitpid(pid1, &stat_loc1, 0);

  int stat_loc2 = 0;
  waitpid(pid2, &stat_loc2, 0);

  if (WIFEXITED(stat_loc1) != 0) return WIFEXITED(stat_loc1);
  if (WIFEXITED(stat_loc2) != 0) return WIFEXITED(stat_loc2);

  return 0;
}

int count_argv(void* var_mem, char** argv) {
  int count = 0;
  while (*argv != nullptr) {
    argv++;
    count++;
  }
  return count;
}

char** expand_argv(void* var_mem, int argc, char** argv) {
  std::vector<std::string> ret;

  for (uint32_t i = 0; i < argc; i++) {
    if (argv[i] != nullptr) {
      ret.emplace_back(argv[i]);
    } else {
      i++;
      auto paths = expand_program_argument(var_mem, argv[i]);
      ret.insert(ret.end(), paths.begin(), paths.end());
    }
  }

  char** ret_argv = (char**)malloc(sizeof(char*) * (ret.size() + 1));
  size_t i = 0;
  for (auto str : ret) {
    ret_argv[i] = strdup(str.c_str());
    i++;
  }

  ret_argv[ret.size()] = nullptr;

  return ret_argv;
}

int write_to_location(void* var_mem, const char* data, size_t data_len,
                      const char* file) {
  FILE* fd;

  // TODO: add append
  if (!false) {
    fd = fopen(file, "w");
  } else {
    fd = fopen(file, "a");
  }
  if (fd == nullptr) {
    return 1;
  }

  fwrite(data, 1, data_len, fd);
  fclose(fd);
  return 0;
}
}
