#pragma once

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <cstdio>
#include <expected>
#include <format>
#include <llvm/Support/ModRef.h>
#include <map>
#include <memory>

struct CodegenState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> module;
  std::map<std::string, std::optional<llvm::Value*>> named_values;
  llvm::Function* entry;

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

  void generate_standard_library() {
    generate_prototype("echo");
    generate_prototype("printf");

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getFloatTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "str_to_float", module.get());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "str_to_len", module.get());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::Type::getInt64Ty(*context)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "int_len", module.get());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::Type::getInt64Ty(*context), llvm::PointerType::get(*context, 0),
          llvm::Type::getInt64Ty(*context)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "int_to_str", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::PointerType::get(*this->context, 0), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "create_variable_memory", module.get());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
          llvm::PointerType::get(*this->context, 0),
          llvm::Type::getInt64Ty(*this->context)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::PointerType::get(*this->context, 0), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "get_variable_memory", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
          llvm::PointerType::get(*this->context, 0),
          llvm::Type::getInt64Ty(*this->context),
          llvm::PointerType::get(*this->context, 0),
          llvm::Type::getInt64Ty(*this->context)

      };

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "store_variable_memory", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }
    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
      };

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "free_variable_memory", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
          llvm::Type::getInt64Ty(*context),
          llvm::PointerType::get(*this->context, 0)};

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "store_args_variable_memory", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
      };

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::PointerType::get(*this->context, 0), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "pop_output_stack", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }

    {
      std::vector<llvm::Type*> bash_func_args = {
          llvm::PointerType::get(*this->context, 0),
          llvm::Type::getInt16Ty(*this->context),
      };

      llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context), bash_func_args, false);

      llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                             "push_output_stack", module.get())->setMemoryEffects(llvm::MemoryEffects::unknown());
    }
  }

  void generate_variable_memory() {
    llvm::Function* program_called =
        module->getFunction("create_variable_memory");
    if (!program_called) {
      fprintf(stderr, "Issue finding create_variable_memory\n");
      return;
    }

    // If argument mismatch error.
    if (program_called->arg_size() != 0) {
      fprintf(stderr, "create_variable_memory is illdefined\n");
      return;
    }

    auto var_mem = builder->CreateCall(program_called, {});

    named_values["variable_memory"] = var_mem;
  }
  void free_variable_memory(llvm::Value* var_mem) {
    llvm::Function* program_called =
        module->getFunction("free_variable_memory");
    if (!program_called) {
      fprintf(stderr, "Issue finding free_variable_memory\n");
      return;
    }

    // If argument mismatch error.
    if (program_called->arg_size() != 1) {
      fprintf(stderr, "free_variable_memory is illdefined\n");
      return;
    }

    builder->CreateCall(program_called, {var_mem});
  }

  CodegenState(std::vector<std::string> function_protypes) {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("bash", *context);

    // create a new builder for the module.
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    generate_standard_library();

    llvm::FunctionType* entry_type =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context), false);

    entry = llvm::Function::Create(entry_type, llvm::Function::ExternalLinkage,
                                   "main", module.get());

    llvm::BasicBlock* entry_block =
        llvm::BasicBlock::Create(*context, "entry", entry);
    builder->SetInsertPoint(entry_block);

    generate_variable_memory();

    for (auto prototype : function_protypes) {
      generate_prototype(prototype);
    }
  }
};

std::expected<llvm::Value*, std::string> runtime_strlen(CodegenState& state,
                                                        llvm::Value* val);
