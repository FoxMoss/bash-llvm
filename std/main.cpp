
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
void echo(uint64_t argc, char** argv) {
  for (uint64_t i = 0; i < argc; i++) {
    printf("%s", argv[i]);
  }
  printf("\n");
}
float str_to_float(char* str) { return std::strtof(str, NULL); }
}
