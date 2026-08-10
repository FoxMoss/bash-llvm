
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <print>
#include <string>

#include "ast/ast.h"
#include "lexer.h"
#include "main.h"
#include "whereami.h"

File::File(std::string contents) : d_contents(std::move(contents)) {}

const std::string& File::contents() const noexcept { return d_contents; }

std::optional<File> File::open(std::string_view file_name) {
  std::filesystem::path path{file_name};
  std::ifstream file{path};
  if (!file) return std::nullopt;

  std::stringstream buffer;
  buffer << file.rdbuf();
  return File{buffer.str()};
}

[[nodiscard]] bool compile_bash(std::string filename_in,
                                std::string filename_out,
                                std::optional<std::string> exectuable_out,
                                OptimizationFlag opt_flag, bool debug_lexer,
                                bool debug_ast, SandboxingOptions sandboxing,
                                std::optional<std::string> ir_file) {
  bool print_lexed = debug_lexer;
  bool print_ast = debug_ast;

  auto source_file = File::open(filename_in);
  size_t cursor = 0;

  std::optional<std::vector<BashLexerSegment>> last_token;
  std::vector<BashLexerSegment> lexer_segments;
  ParenMap paren_map;

  std::string file_contents = source_file->contents();
  do {
    paren_map.index_counter = lexer_segments.size();
    last_token = BashLexerSegment::munch_token(
        file_contents, cursor,
        last_token.has_value() ? last_token->back().token : TOK_UNK, paren_map);

    // must have value so we don't need to check
    lexer_segments.insert(lexer_segments.end(), last_token.value().begin(),
                          last_token.value().end());
  } while (last_token->back().token != TOK_EOF);

  lexer_segments = paren_map_fusing(lexer_segments, paren_map);

  if (print_lexed) {
    for (auto token : lexer_segments) {
      std::print(stderr, "[{}] {}\n", token.str, token.get_token_name());
    }
  }

  size_t ast_cursor = 0;
  auto base = parse_compound_expression(lexer_segments, ast_cursor, true);

  if (print_ast && base.has_value()) {
    base.value()->print_name(0);
  }

  if (!base.has_value()) {
    std::print(stderr, "error while parsing\n");
    return false;
  }

  auto function_prototypes = base->get()->get_functions_defined();
  CodegenState state(function_prototypes, false);
  state.sandboxing = sandboxing;

  auto target_triple = llvm::Triple(sys::getDefaultTargetTriple());

  std::string error;
  auto target = TargetRegistry::lookupTarget(target_triple, error);
  if (!target) {
    std::println(stderr, "error could not get target triple");
    return false;
  }

  TargetOptions opt;
  TargetMachine* target_machine = target->createTargetMachine(
      target_triple, "generic", "", opt, Reloc::PIC_);

  state.module->setDataLayout(target_machine->createDataLayout());
  state.module->setTargetTriple(target_triple);

  // entry function
  {
    state.generate_entry();

    auto value = base.value()->codegen(state);
    if (!value.has_value()) {
      std::print(stderr, "error: {}\n", value.error());
      return false;
    }

    state.generate_exit(false);

    llvm::OptimizationLevel llvm_level;
    switch (opt_flag) {
      default:
      case OPT_O0:
        llvm_level = llvm::OptimizationLevel::O0;
        break;
      case OPT_O1:
        llvm_level = llvm::OptimizationLevel::O1;
        break;
      case OPT_O2:
        llvm_level = llvm::OptimizationLevel::O2;
        break;
      case OPT_O3:
        llvm_level = llvm::OptimizationLevel::O3;
        break;
      case OPT_Oz:
        llvm_level = llvm::OptimizationLevel::Oz;
        break;
      case OPT_Os:
        llvm_level = llvm::OptimizationLevel::Os;
        break;
    }

    llvm::PassBuilder pb(target_machine);
    pb.registerModuleAnalyses(*state.mam);
    pb.registerFunctionAnalyses(*state.fam);
    pb.crossRegisterProxies(*state.lam, *state.fam, *state.cgam, *state.mam);

    auto mpm = pb.buildPerModuleDefaultPipeline(llvm_level);
    state.mpm.run(*state.module, *state.mam);
  }

  std::error_code error_code;
  llvm::raw_fd_ostream out_file(filename_out, error_code);

  if (error_code) {
    std::println(stderr, "error opening output file");
    return false;
  }

  llvm::legacy::PassManager object_passes;
  if (target_machine->addPassesToEmitFile(object_passes, out_file, nullptr,
                                          llvm::CodeGenFileType::ObjectFile)) {
    std::println(stderr, "target machine can't output object file");
    return false;
  }

  object_passes.run(*state.module);
  out_file.flush();

  delete target_machine;

  if (ir_file.has_value()) {
    llvm::raw_fd_ostream ir_out_file(ir_file.value(), error_code);
    state.module->print(ir_out_file, nullptr);
  }

  if (exectuable_out.has_value()) {
    std::array<char, 1024> module_path = {};
    int length = wai_getModulePath(module_path.data(), 1023, nullptr);

    auto module_path_parent =
        std::filesystem::path(std::string(module_path.data(), length))
            .parent_path()
            .string();

    auto library_path = std::format("{}/libstdllsh.so", module_path_parent);

    auto clang_path = llvm::sys::findProgramByName("clang++");
    if (!clang_path) {
      std::println(stderr, "cannot find clang++ to make executable");
      return false;
    }
    std::vector<llvm::StringRef> args;
    args.emplace_back(clang_path.get());
    args.emplace_back("-rpath");
    args.emplace_back(module_path_parent);
    args.emplace_back(library_path);
    args.emplace_back(filename_out);
    args.emplace_back("-o");
    args.emplace_back(exectuable_out.value());

    llvm::sys::ExecuteAndWait(clang_path.get(),
                              llvm::ArrayRef(args.data(), args.size()));

    std::filesystem::remove(filename_out);
  }

  return true;
}
