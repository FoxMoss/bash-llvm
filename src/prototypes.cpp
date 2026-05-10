
#include "codegen.h"

void CodegenState::generate_standard_library() {
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
                           "int_to_str", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
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
                           "get_variable_memory", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
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
                           "store_variable_memory", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }
  {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*this->context, 0),
    };

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "free_variable_memory", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }

  {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt64Ty(*context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "store_args_variable_memory", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }

  {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*this->context, 0),
    };

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::PointerType::get(*this->context, 0), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "pop_output_stack", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }

  {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt16Ty(*this->context),
    };

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "push_output_stack", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }

  if (!is_sandboxed) {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt64Ty(*context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "external_program", module.get());
  }
}
