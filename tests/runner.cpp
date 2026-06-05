#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
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

  if (!std::filesystem::is_regular_file("/bin/bash")) {
    std::println(stderr, "Error /bin/bash does not exist.");
    return 1;
  }

  nlohmann::json successful_tests_json = nlohmann::json::object();
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
    if (success) {
      successful_tests_json[file.path().string()] = true;
    }
  }

  std::println("Coverage {}/{} or {:.2f}%", successful_tests, total_tests,
               (float)successful_tests / (float)total_tests * 100);

  if (std::filesystem::exists("test_persistent.json")) {
    std::ifstream test_persistent("test_persistent.json");
    nlohmann::json last = nlohmann::json::parse(test_persistent);
    test_persistent.close();
    if (!last.is_object()) {
      std::println(stderr, "error: test_persistent.json is not an object");
      return 1;
    }

    std::println("\nRegressions:");

    size_t regressed = 0;
    for (nlohmann::json::iterator it = last.begin(); it != last.end(); ++it) {
      if (!successful_tests_json.contains(it.key())) {
        std::println("\e[0;31mREGRESSED\e[0;37m {}", it.key());
        regressed++;
      }
    }
    if (regressed == 0) {
      std::println("\e[0;32mNo regressions :)\e[0;37m");
    }

    for (nlohmann::json::iterator it = successful_tests_json.begin();
         it != successful_tests_json.end(); ++it) {
      if (!last.contains(it.key())) {
        last[it.key()] = true;
      }
    }

    std::ofstream out_file("test_persistent.json");
    out_file << last.dump();
  } else {
    std::ofstream out_file("test_persistent.json");
    out_file << successful_tests_json.dump();
  }

  return 0;
}
