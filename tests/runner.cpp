#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <print>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::println(stderr, "USAGE: {} [test_directory] [impl]", argv[0]);
    return 1;
  }

  std::filesystem::path test_directory(argv[1]);
  std::filesystem::path impl_progam(argv[2]);

  if (!std::filesystem::is_directory(test_directory)) {
    std::println(stderr, "Error {} is not a directory",
                 test_directory.string());
    return 1;
  }

  if (!std::filesystem::is_regular_file("/usr/bin/bash")) {
    std::println(stderr, "Error /usr/bin/bash does not exist.");
    return 1;
  }

  size_t total_tests = 0;
  size_t successful_tests = 0;

  std::filesystem::directory_iterator children(test_directory);
  for (auto file : children) {
    if (file.path().extension() != ".sh") {
      continue;
    }

    std::println("\e[0;34m....\e[0;37m {}", file.path().string());

    std::array<int, 2> bash_link;
    pipe(bash_link.data());

    if (fork() == 0) {
      dup2(bash_link[1], STDOUT_FILENO);
      close(bash_link[0]);
      close(bash_link[1]);
      close(STDERR_FILENO);
      execl("/usr/bin/bash", "bash",
            std::filesystem::absolute(file.path()).c_str(), nullptr);
      return 0;
    }
    close(bash_link[1]);
    std::array<char, 4096> outbuffer;
    std::string bash_output;

    ssize_t size = 0;
    while ((size = read(bash_link[0], outbuffer.data(), outbuffer.size())) !=
           0) {
      bash_output.insert(bash_output.end(), outbuffer.begin(),
                         outbuffer.begin() + size);
    }

    std::array<int, 2> impl_link;
    pipe(impl_link.data());

    if (fork() == 0) {
      dup2(impl_link[1], STDOUT_FILENO);
      close(impl_link[0]);
      close(impl_link[1]);
      close(STDERR_FILENO);
      execl(std::filesystem::absolute(impl_progam).c_str(),
            impl_progam.filename().c_str(),
            std::filesystem::absolute(file.path()).c_str(), nullptr);
      return 0;
    }
    close(impl_link[1]);
    std::string impl_output;

    while ((size = read(impl_link[0], outbuffer.data(), outbuffer.size())) !=
           0) {
      impl_output.insert(impl_output.end(), outbuffer.begin(),
                         outbuffer.begin() + size);
    }

    bool success = impl_output == bash_output;
    total_tests++;
    successful_tests += success;
    std::println("\e[1A{} {}",
                 success ? "\e[0;32mGOOD\e[0;37m" : "\e[0;31mNOPE\e[0;37m",
                 file.path().string());
  }

  std::println("Coverage {}/{} or {:.2f}%", successful_tests, total_tests,
               (float)successful_tests / (float)total_tests * 100);

  return 0;
}
