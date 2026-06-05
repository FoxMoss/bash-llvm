#include <sched.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <print>
int main(int argc, char* argv[]) {
  if (unshare(CLONE_NEWNS) == -1) {
    std::println(stderr, "error: {}", strerror(errno));
    return 1;
  }
  execvp(argv[1], &argv[1]);
  return 0;
}
