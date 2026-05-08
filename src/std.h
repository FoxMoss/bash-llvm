#pragma once
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

  std::map<std::string, std::string> positional_arguments;

  std::optional<std::string> storage;
};

struct VariableMemory {
  std::map<std::string, std::string> memory;
  std::map<uint64_t, OutputFactor> output_stack;
  uint64_t stack_iterator;
};

extern "C" {
void bash_echo(void* var_mem, uint64_t argc, char** argv);

void bash_printf(void* var_mem, uint64_t argc, char** argv);

float str_to_float(char* str);
size_t str_to_len(char* str);
size_t int_len(int64_t i);

void int_to_str(int64_t i, char* buf, size_t buf_len);

void* create_variable_memory();

void store_variable_memory(void* var_mem, char* key_str, size_t key_len,
                           char* val_str, size_t val_len);

void store_args_variable_memory(void* var_mem, uint64_t argc, char** argv);

const char* get_variable_memory(void* var_mem, char* key_str, size_t key_len);

void free_variable_memory(void* var_mem);

void push_output_stack(void* var_mem, uint16_t output_type);

// the return value stays in memory till the next frame on its level overwrites
// it
const char* pop_output_stack(void* var_mem);
}

