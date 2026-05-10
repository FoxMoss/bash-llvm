#pragma once

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Passes/CodeGenPassBuilder.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Support/ModRef.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include <cstdio>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <print>

struct CodegenState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> module;

  std::unique_ptr<llvm::LoopAnalysisManager> lam;
  std::unique_ptr<llvm::FunctionAnalysisManager> fam;
  std::unique_ptr<llvm::CGSCCAnalysisManager> cgam;
  std::unique_ptr<llvm::ModuleAnalysisManager> mam;
  std::unique_ptr<llvm::PassInstrumentationCallbacks> pic;
  std::unique_ptr<llvm::StandardInstrumentations> si;
  llvm::ModulePassManager mpm;

  std::map<std::string, std::optional<llvm::Value*>> named_values;
  llvm::Function* entry;

  bool is_jit = false;
  bool is_sandboxed = false;

  void generate_prototype(std::string name) {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt64Ty(*context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           std::format("bash_{}", name), module.get());
  }

  void generate_standard_library();

  void generate_variable_memory() {
    llvm::Function* program_called =
        module->getFunction("create_variable_memory");
    if (!program_called) {
      std::println(stderr, "Issue finding create_variable_memory");
      return;
    }

    // If argument mismatch error.
    if (program_called->arg_size() != 0) {
      std::println(stderr, "create_variable_memory is illdefined");
      return;
    }

    auto var_mem = builder->CreateCall(program_called, {});

    named_values["variable_memory"] = var_mem;
  }
  void free_variable_memory(llvm::Value* var_mem) {
    llvm::Function* program_called =
        module->getFunction("free_variable_memory");
    if (!program_called) {
      std::println(stderr, "Issue finding free_variable_memory");
      return;
    }

    // If argument mismatch error.
    if (program_called->arg_size() != 1) {
      std::println(stderr, "free_variable_memory is illdefined");
      return;
    }

    builder->CreateCall(program_called, {var_mem});
  }

  void init_llvm() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("bash", *context);

    // create a new builder for the module.
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    lam = std::make_unique<llvm::LoopAnalysisManager>();
    fam = std::make_unique<llvm::FunctionAnalysisManager>();
    cgam = std::make_unique<llvm::CGSCCAnalysisManager>();
    mam = std::make_unique<llvm::ModuleAnalysisManager>();
    pic = std::make_unique<llvm::PassInstrumentationCallbacks>();
    si =
        std::make_unique<llvm::StandardInstrumentations>(*context,
                                                         /*DebugLogging*/ true);
    si->registerCallbacks(*pic, mam.get());

    generate_standard_library();
  }
  CodegenState(std::vector<std::string> function_protypes, bool is_jit)
      : is_jit(is_jit) {
    init_llvm();

    for (auto prototype : function_protypes) {
      generate_prototype(prototype);
    }

    if (is_jit) {
      {
        std::vector<llvm::Type*> bash_func_args = {};

        llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
            llvm::PointerType::get(*this->context, 0), bash_func_args, false);

        llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                               "jit_create_variable_memory", module.get());
      }
    }
  }
};

std::expected<llvm::Value*, std::string> runtime_strlen(CodegenState& state,
                                                        llvm::Value* val);
