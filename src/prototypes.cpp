
#include "codegen.h"
#include "jit.h"
#include "llvm/IR/Constants.h"

void CodegenState::generate_standard_library() {
#define PROG_SYMBOL(symb) generate_prototype(#symb);
#include "programfuncs.inc"
#undef PROG_SYMBOL

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
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0),
        llvm::IntegerType::getInt1Ty(*context)};

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getInt1Ty(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "strequals", module.get());
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
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*context, 0)};

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
    };

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "pop_function_stack", module.get())
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
  {
    std::vector<llvm::Type*> bash_func_args = {
        llvm::PointerType::get(*this->context, 0),
    };

    llvm::FunctionType* bash_func_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), bash_func_args, false);

    llvm::Function::Create(bash_func_type, llvm::Function::ExternalLinkage,
                           "push_function_stack", module.get())
        ->setMemoryEffects(llvm::MemoryEffects::unknown());
  }

  if (!sandboxing.block_external_programs) {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt64Ty(*context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "external_program", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "fork_process_and_capture_stdout", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "fork_process_and_capture_stdin", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "fork_process", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {llvm::Type::getInt32Ty(*context)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "exit_helper", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {llvm::Type::getInt32Ty(*context),
                                          llvm::Type::getInt32Ty(*context),
                                          llvm::Type::getInt32Ty(*context)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "wait_two_pid", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::Type::getInt64Ty(*context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::PointerType::get(*this->context, 0), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "expand_argv", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(*context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "count_argv", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::PointerType::get(*this->context, 0),
        llvm::PointerType::get(*this->context, 0),
        llvm::IntegerType::getInt64Ty(*this->context),
        llvm::PointerType::get(*this->context, 0)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::IntegerType::getInt32Ty(*this->context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                           "write_to_location", module.get());
  }
  {
    std::vector<llvm::Type*> func_args = {
        llvm::Type::getFloatTy(*this->context),
        llvm::Type::getFloatTy(*this->context)};

    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getFloatTy(*this->context), func_args, false);

    llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "fmodf",
                           module.get());
  }
}
