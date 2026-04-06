
#include <cstdint>
#include <cstdio>

extern "C" {
void echo(uint64_t argc, char** argv) {
  for (uint64_t i = 0; i < argc; i++) {
    printf("%s", argv[i]);
  }
  printf("\n");
}
}
