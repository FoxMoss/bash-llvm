#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void echo(uint64_t argc, char** argv) {
  for (uint64_t i = 0; i < argc; i++) {
    printf("%s", argv[i]);
    if (i != argc - 1) {
      printf(" ");
    }
  }
  printf("\n");
}
float str_to_float(char* str) { return std::strtof(str, NULL); }
size_t str_to_len(char* str) { return std::strlen(str); }
size_t int_log(uint64_t i) {
  return std::floor(std::max(std::log10((double)i) + 1, 1.0));
}
}
