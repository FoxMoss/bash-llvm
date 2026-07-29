#include <glob.h>

#include <filesystem>
#include <print>

#include "isocline.h"

void add_completions_in_dir(ic_completion_env_t* cenv, std::string prefix,
                            std::filesystem::path path,
                            std::string starts_with) {
  if (!std::filesystem::exists(path)) return;

  for (auto file_iter : std::filesystem::directory_iterator(path)) {
    std::string testing_file_name = file_iter.path().filename();
    if (!testing_file_name.starts_with(starts_with)) continue;

    if (file_iter.is_directory()) {
      testing_file_name += "/";
    }

    auto combined = std::format("{}{}", prefix, testing_file_name);

    ic_add_completion(cenv, combined.c_str());
  }
}

void complete_filename(ic_completion_env_t* cenv, const char* prefix) {
  std::string prefix_str(prefix);

  if (prefix_str.contains("*")) {
    glob_t glob_buf;

    glob(prefix_str.c_str(), GLOB_TILDE, nullptr, &glob_buf);
    std::string globbed_str = "";

    for (size_t i = 0; i < glob_buf.gl_pathc; i++) {
      globbed_str += std::format("{} ", glob_buf.gl_pathv[i]);
    }

    globfree(&glob_buf);

    ic_add_completion(cenv, globbed_str.c_str());
  } else if (prefix_str == "") {
    add_completions_in_dir(cenv, "", std::filesystem::current_path(), "");
  } else {
    std::string original = prefix_str;
    if (prefix_str.starts_with("~/")) {
      std::string home_path = "";
      if (getenv("HOME") != nullptr) {
        home_path = getenv("HOME");
      }

      prefix_str = std::format("{}{}", home_path,
                               prefix_str.substr(1, std::string::npos));
    }

    std::filesystem::path prefix_path(prefix_str);
    std::error_code ec;
    prefix_path = std::filesystem::absolute(prefix_path, ec);

    std::string file_name = prefix_path.filename();
    original = original.substr(0, original.size() - file_name.size());

    add_completions_in_dir(cenv, original, prefix_path.parent_path(),
                           file_name);
  }
}
void completer(ic_completion_env_t* cenv, const char* input) {
  ic_complete_qword_ex(cenv, input, &complete_filename,
                       &ic_char_is_filename_letter, '\\', "'\"");
}
