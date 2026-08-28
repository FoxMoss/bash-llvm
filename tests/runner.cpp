#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <set>
#include <string>

int main(int argc, char* argv[]) {
  if (argc != 3 && argc != 4 && argc != 5) {
    std::println(stderr, "USAGE: {} <bench> <precomp> [test_directory] [impl]",
                 argv[0]);
    return 1;
  }

  bool bench = strcmp(argv[1], "bench") == 0;
  if (bench && argc != 4 && argc != 5) {
    std::println(stderr, "USAGE: {} bench <precomp> [test_directory] [impl]",
                 argv[0]);
    return 1;
  }
  bool precomp = strcmp(argv[1 + bench], "precomp") == 0;
  if (precomp && argc != 4 && argc != 5) {
    std::println(stderr, "USAGE: {} <bench> precomp [test_directory] [impl]",
                 argv[0]);
    return 1;
  }

  int add = bench + precomp;

  std::filesystem::path test_directory(argv[1 + add]);
  std::filesystem::path impl_progam(argv[2 + add]);

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

  std::map<std::string, double> speeds;

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

    std::string test_prog;
    std::optional<std::string> first_arg;
    std::optional<std::string> second_arg;

    if (precomp) {
      test_prog = std::format("/tmp/{}.o", file.path().filename().string());
      first_arg = {};
      second_arg = {};

      auto comp_fd = fork();
      if (comp_fd == 0) {
        close(STDERR_FILENO);
        execl(std::filesystem::absolute(impl_progam).c_str(),
              impl_progam.filename().c_str(), "compile",
              std::filesystem::absolute(file.path()).c_str(), "--executable",
              test_prog.c_str(), nullptr);
        return 0;
      }

      int stat_loc;
      waitpid(comp_fd, &stat_loc, 0);
    } else {
      test_prog = std::filesystem::absolute(impl_progam).string();
      first_arg = impl_progam.filename();
      second_arg = std::filesystem::absolute(file.path());
    }

    auto before_time = std::chrono::high_resolution_clock::now();
    std::array<int, 2> impl_link;
    pipe(impl_link.data());

    if (fork() == 0) {
      dup2(impl_link[1], STDOUT_FILENO);
      close(impl_link[0]);
      close(impl_link[1]);
      close(STDERR_FILENO);
      execl(test_prog.c_str(),
            first_arg.has_value() ? first_arg->c_str() : nullptr,
            second_arg.has_value() ? second_arg->c_str() : nullptr, nullptr);
      return 0;
    }
    close(impl_link[1]);
    std::string impl_output;

    while ((size = read(impl_link[0], outbuffer.data(), outbuffer.size())) !=
           0) {
      impl_output.insert(impl_output.end(), outbuffer.begin(),
                         outbuffer.begin() + size);
    }
    auto after_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms_double =
        after_time - before_time;

    bool success = impl_output == bash_output;
    total_tests++;
    successful_tests += success;
    std::println(
        "\e[1A{} {} {}",
        success ? "\e[0;32mGOOD\e[0;37m" : "\e[0;31mNOPE\e[0;37m",
        file.path().string(),
        success ? "in " + std::to_string(ms_double.count()) + "ms" : "");
    if (success) {
      successful_tests_json[file.path().string()] = true;
      speeds[file.path().filename().string()] = ms_double.count();
    }
  }

  std::println("Coverage {}/{} or {:.2f}%", successful_tests, total_tests,
               (float)successful_tests / (float)total_tests * 100);

  if (!bench) {
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
  } else {
    nlohmann::json persistent;
    if (std::filesystem::exists("bench_persistent.json")) {
      std::ifstream bench_persistent("bench_persistent.json");
      persistent = nlohmann::json::parse(bench_persistent);
      bench_persistent.close();
    }

    persistent[impl_progam.filename().string()] = nlohmann::json::object();
    for (auto pair : speeds) {
      persistent[impl_progam.filename().string()][pair.first] = pair.second;
    }
    std::ofstream out_file("bench_persistent.json");
    out_file << persistent.dump();

    std::map<std::string, std::vector<double>> unique_tests;
    std::print("|   |");
    size_t implementations = 0;
    for (auto& impl : persistent.items()) {
      std::print(" {} |", impl.key());

      for (auto& test : impl.value().items()) {
        unique_tests[test.key()].push_back(test.value());
      }
      implementations++;
    }
    std::print("\n");

    for (size_t i = 0; i < implementations; i++) {
      std::print("|---");
    }
    std::print("|---|\n");

    for (auto test : unique_tests) {
      if (test.second.size() != implementations) {
        continue;
      }

      auto min =
          std::ranges::min_element(test.second.begin(), test.second.end());
      std::print("| {} |", test.first);
      for (auto val : test.second) {
        if (*min.base() == val) {
          std::print(" {}ms |", val);
        } else {
          std::print(" {}ms |", val);
        }
      }
      std::print("\n");
    }
  }

  return 0;
}
