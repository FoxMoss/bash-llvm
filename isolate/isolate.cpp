#include <sched.h>
int main(int argc, char* argv[]) {
  unshare(CLONE_FILES | CLONE_FS);
  return 0;
}
