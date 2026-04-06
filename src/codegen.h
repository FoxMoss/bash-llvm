#pragma once

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <map>
#include <memory>

struct CodegenState {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  std::unique_ptr<llvm::Module> module;
  std::map<std::string, llvm::Value*> named_values;
  llvm::Function* entry;

  void generate_standard_library() {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::Type::getInt64Ty(*context),
        llvm::ArrayType::get(llvm::PointerType::get(*this->context, 0), 1)};

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "echo", module.get());
  }

  CodegenState() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("bash", *context);

    // create a new builder for the module.
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    generate_standard_library();

    llvm::FunctionType* entry_type =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context), false);

    entry = llvm::Function::Create(entry_type, llvm::Function::ExternalLinkage,
                                   "entry", module.get());

    llvm::BasicBlock* entry_block =
        llvm::BasicBlock::Create(*context, "entry", entry);
    builder->SetInsertPoint(entry_block);
  }
};

