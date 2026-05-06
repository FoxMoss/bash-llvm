#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

#define REPORT_ISSUE(str) fprintf(stderr, "Runtime Issue: %s\n", str);
#define USE_VAR_MEM()                                        \
  if (var_mem == 0) REPORT_ISSUE("Variable Memory is null"); \
  auto var_mem_usable = (VariableMemory*)var_mem;

extern "C" {
void bash_echo(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  if (var_mem_usable->output_stack.size() == 0) return;

  switch (
      var_mem_usable->output_stack[var_mem_usable->stack_iterator].location) {
    case OutputFactor::OUTPUT_STDOUT:
      for (uint64_t i = 0; i < argc; i++) {
        printf("%s", argv[i]);
        if (i != argc - 1) {
          printf(" ");
        }
      }
      printf("\n");
      break;

    case OutputFactor::OUTPUT_STR:
      if (!var_mem_usable->output_stack[var_mem_usable->stack_iterator]
               .storage.has_value()) {
        var_mem_usable->output_stack[var_mem_usable->stack_iterator].storage =
            "";
      }

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
      break;
    case OutputFactor::OUTPUT_UNK:
      break;
  }
}

void bash_printf(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
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
      printf("%s", out_string.c_str());
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

float str_to_float(char* str) {
  auto val = std::strtof(str, NULL);
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

void* create_variable_memory() {
  VariableMemory* ptr = (VariableMemory*)malloc(sizeof(VariableMemory));
  *ptr = VariableMemory();
  return ptr;
}

void store_variable_memory(void* var_mem, char* key_str, size_t key_len,
                           char* val_str, size_t val_len) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;
  var_mem_usable->memory[std::string(key_str, key_len)] =
      std::string(val_str, val_len);
}

void store_args_variable_memory(void* var_mem, uint64_t argc, char** argv) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
    return;
  }
  auto var_mem_usable = (VariableMemory*)var_mem;

  for (uint64_t i = 0; i < argc; i++) {
    var_mem_usable->output_stack[var_mem_usable->stack_iterator]
        .positional_arguments[std::to_string(i + 1)] = argv[i];
  }
}

const char* get_variable_memory(void* var_mem, char* key_str, size_t key_len) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
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

void free_variable_memory(void* var_mem) { free(var_mem); }

void push_output_stack(void* var_mem, uint16_t output_type) {
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
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
  if (var_mem == 0) {
    fprintf(stderr, "Runtime Issue: Variable memory is null\n");
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
