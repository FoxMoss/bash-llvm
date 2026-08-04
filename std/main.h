#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <string>

struct OutputFactor {
  enum OutputLocation {
    OUTPUT_STDOUT = 0,
    OUTPUT_STR = 1,
    OUTPUT_UNK = 2  // UNK must always be the largest and there can no be no
                    // gaps otherwise checking fails
  } location;

  std::optional<std::string> storage = {};

  std::optional<int> private_stdin = {};
};

struct FunctionFactor {
  std::map<std::string, std::string> positional_arguments;
};

struct VariableMemory {
  std::map<std::string, std::string> memory;
  std::map<uint64_t, OutputFactor> output_stack;
  uint64_t output_stack_iterator;
  std::map<uint64_t, FunctionFactor> function_stack;
  uint64_t function_stack_iterator;

  std::map<std::string, bool> shell_options;
};

struct SandboxingOptions {
  bool block_external_programs = false;
  bool run_in_root_sanbox = false;
  bool dont_cache = false;

  bool any_features() { return run_in_root_sanbox | block_external_programs; }
};

extern "C" {
int bash_shopt(void* var_mem, uint64_t argc, char** argv);
int bash_echo(void* var_mem, uint64_t argc, char** argv);
int bash_printf(void* var_mem, uint64_t argc, char** argv);
int bash_cd(void* var_mem, uint64_t argc, char** argv);

int external_program(void* var_mem, char* path, uint64_t argc, char** argv);

float str_to_float(char* str);
size_t str_to_len(char* str);
size_t int_len(int64_t i);
void int_to_str(int64_t i, char* buf, size_t buf_len);
bool strequals(void* var_mem, const char* a, const char* b, bool for_case);

void* create_variable_memory(void* sandboxed);

void store_variable_memory(void* var_mem, const char* key_str, size_t key_len,
                           const char* val_str, size_t val_len);

void store_args_variable_memory(void* var_mem, uint64_t argc, char** argv);

const char* get_variable_memory(void* var_mem, const char* key_str,
                                size_t key_len);

void free_variable_memory(void* var_mem);

void push_output_stack(void* var_mem, uint16_t output_type);

// the return value stays in memory till the next frame on its level overwrites
// it
const char* pop_output_stack(void* var_mem);

void push_function_stack(void* var_mem);
void pop_function_stack(void* var_mem);

int fork_process_and_capture_stdin(void* var_mem);
int fork_process_and_capture_stdout(void* var_mem);
int fork_process(void* var_mem);
int exit_helper(int status);
int wait_two_pid(void* var_mem, int pid1, int pid2);
int write_to_location(void* var_mem, const char* data, size_t data_len,
                      const char* file);
uint64_t count_argv(void* var_mem, char** argv);
char** expand_argv(void* var_mem, uint64_t argc, char** argv);

float fmodf(float x, float y);
}

