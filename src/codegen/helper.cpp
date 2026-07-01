
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "codegen.h"

std::expected<llvm::Value*, std::string> get_variable_memory(
    CodegenState& state, std::string name) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("get_variable_memory");
  if (!program_called) return std::unexpected("Variable getter does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Helper get_variable_memory is illdefined");

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      state.builder->CreateGlobalString(name),
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                             name.size())};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> runtime_strlen(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isPointerTy()) {
    llvm::Function* program_called = state.module->getFunction("str_to_len");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program str_to_len is illdefined");

    std::vector<llvm::Value*> arg_values = {val};

    return state.builder->CreateCall(program_called, arg_values);

  } else {
    return std::unexpected("Could not get strlen");
  }
}

std::expected<llvm::Value*, std::string> cast_to_int(CodegenState& state,
                                                     llvm::Value* val) {
  if (val->getType()->isIntegerTy()) {
    return val;
  } else if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFPToSI(val,
                                       llvm::Type::getInt64Ty(*state.context));
  }
  return std::unexpected("Could not reduce to int");
}

std::expected<llvm::Value*, std::string> cast_to_str(CodegenState& state,
                                                     llvm::Value* val) {
  if (val->getType()->isPointerTy()) {
    return val;
  } else if (val->getType()->isFloatingPointTy() ||
             val->getType()->isIntegerTy()) {
    auto int_val = cast_to_int(state, val);
    UNWRAP_EXPECTED(int_val)

    llvm::Function* program_called = state.module->getFunction("int_len");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program int_len is illdefined");

    std::vector<llvm::Value*> arg_values = {int_val.value()};

    auto int_len = state.builder->CreateCall(program_called, arg_values);

    auto int_len_padded = state.builder->CreateAdd(
        int_len,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    auto stack_str = state.builder->CreateAlloca(
        llvm::Type::getInt8Ty(*state.context), int_len_padded);

    program_called = state.module->getFunction("int_to_str");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 3)
      return std::unexpected("Program int_to_str is illdefined");

    arg_values = {int_val.value(), stack_str, int_len_padded};

    state.builder->CreateCall(program_called, arg_values);
    return stack_str;
  }
  return std::unexpected("Type not support for str conversion");
}

std::expected<llvm::Value*, std::string> cast_to_float(CodegenState& state,
                                                       llvm::Value* val) {
  if (val->getType()->isIntegerTy()) {
    return state.builder->CreateSIToFP(val,
                                       llvm::Type::getFloatTy(*state.context));
  } else if (val->getType()->isFloatTy()) {
    return val;
  } else if (val->getType()->isDoubleTy()) {
    return state.builder->CreateFPCast(val,
                                       llvm::Type::getFloatTy(*state.context));
  } else if (val->getType()->isPointerTy()) {
    llvm::Function* program_called = state.module->getFunction("str_to_float");
    if (!program_called) return std::unexpected("Unknown function referenced");

    // If argument mismatch error.
    if (program_called->arg_size() != 1)
      return std::unexpected("Program str_to_float is illdefined");

    std::vector<llvm::Value*> arg_values = {val};

    return state.builder->CreateCall(program_called, arg_values);

    // hand written str to float parsing
    // dragons be ware
#if 0
    llvm::Function* parent_func = state.builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* header = state.builder->GetInsertBlock();

    auto length_search =
        llvm::BasicBlock::Create(*state.context, "length_search", parent_func);
    auto increment = llvm::BasicBlock::Create(*state.context, "increment");
    auto merge = llvm::BasicBlock::Create(*state.context, "merge");

    state.builder->CreateBr(length_search);

    state.builder->SetInsertPoint(length_search);

    llvm::PHINode* length = state.builder->CreatePHI(
        llvm::Type::getInt64Ty(*state.context), 2, "length");
    length->addIncoming(
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0),
        header);

    auto search_ptr = state.builder->CreateIntToPtr(
        state.builder->CreateAdd(
            state.builder->CreatePtrToInt(
                val, llvm::Type::getInt64Ty(*state.context)),
            length),
        llvm::PointerType::get(*state.context, 0));

    auto searched_val = state.builder->CreateLoad(
        llvm::Type::getInt8Ty(*state.context), search_ptr);

    auto was_null = state.builder->CreateICmpEQ(
        searched_val,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), 0));

    auto next_length_test = state.builder->CreateAdd(
        length,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    length->addIncoming(next_length_test, length_search);

    state.builder->CreateCondBr(was_null, increment, length_search);

    length_search = state.builder->GetInsertBlock();

    parent_func->insert(parent_func->end(), increment);
    state.builder->SetInsertPoint(increment);

    llvm::PHINode* index = state.builder->CreatePHI(
        llvm::Type::getInt64Ty(*state.context), 2, "index");
    index->addIncoming(
        // 2 just to offset the fact that we'll be 2 off from the lowest digit
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0),
        length_search);

    llvm::PHINode* return_value = state.builder->CreatePHI(
        llvm::Type::getFloatTy(*state.context), 2, "interpreted_value");
    return_value->addIncoming(
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 0),
        length_search);

    llvm::PHINode* power_to = state.builder->CreatePHI(
        llvm::Type::getFloatTy(*state.context), 2, "power_to");
    power_to->addIncoming(
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 1),
        length_search);

    auto found_char = state.builder->CreateLoad(
        llvm::Type::getInt8Ty(*state.context),
        state.builder->CreateIntToPtr(
            state.builder->CreateSub(

                state.builder->CreateSub(
                    state.builder->CreateAdd(
                        state.builder->CreatePtrToInt(
                            val, llvm::Type::getInt64Ty(*state.context)),
                        length),
                    index),
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                                       1)),
            llvm::PointerType::get(*state.context, 0)));

    auto char_int_val = state.builder->CreateUIToFP(
        state.builder->CreateSub(
            found_char,
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), '0')),
        llvm::Type::getFloatTy(*state.context));

    auto new_return = state.builder->CreateFAdd(
        state.builder->CreateFMul(char_int_val, power_to), return_value);
    return_value->addIncoming(new_return, increment);

    auto new_power = state.builder->CreateFMul(
        power_to,
        llvm::ConstantFP::get(llvm::Type::getFloatTy(*state.context), 10));

    power_to->addIncoming(new_power, increment);

    auto reached_end = state.builder->CreateICmpEQ(index, length);

    auto new_index = state.builder->CreateAdd(
        index,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 1));

    index->addIncoming(new_index, increment);

    state.builder->CreateCondBr(reached_end, merge, increment);

    increment = state.builder->GetInsertBlock();

    parent_func->insert(parent_func->end(), merge);
    state.builder->SetInsertPoint(merge);

    return new_return;
#endif
  } else {
    return std::unexpected("Could not reduce to float");
  }
}

std::expected<llvm::Value*, std::string> store_variable_memory(
    CodegenState& state, std::string name, llvm::Value* value) {
  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  llvm::Function* program_called =
      state.module->getFunction("store_variable_memory");
  if (!program_called) return std::unexpected("Variable getter does not exist");

  // If argument mismatch error.
  if (program_called->arg_size() != 5)
    return std::unexpected("Helper store_variable_memory is illdefined");

  auto str_length = runtime_strlen(state, value);
  UNWRAP_EXPECTED(str_length)

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(),
      state.builder->CreateGlobalString(name),
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context),
                             name.size()),
      value, str_length.value()};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> runtime_store_args_variable_memory(
    CodegenState& state, llvm::Value* argc, llvm::Value* argv) {
  llvm::Function* program_called =
      state.module->getFunction("store_args_variable_memory");
  if (!program_called)
    return std::unexpected("store_args_variable_memory not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Program store_args_variable_memory is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(), argc, argv};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val) {
  if (val->getType()->isFloatingPointTy()) {
    return state.builder->CreateFCmpOGT(
        val, llvm::ConstantFP::get(*state.context, llvm::APFloat(0.0)));
  } else if (val->getType()->isIntegerTy(1)) {
    return val;
  } else if (val->getType()->isIntegerTy()) {
    return state.builder->CreateICmpSGT(
        val, llvm::ConstantInt::get(val->getType(), 0));
  } else if (val->getType()->isVoidTy()) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*state.context), 1);
  } else if (val->getType()->isPointerTy()) {
    auto len = runtime_strlen(state, val);
    UNWRAP_EXPECTED(len)

    return state.builder->CreateICmpSGT(
        len.value(), llvm::ConstantInt::get(len.value()->getType(), 0));
  }

  return std::unexpected("Couldn't reduce to bool");
}

std::expected<llvm::Value*, std::string> runtime_strequals(CodegenState& state,
                                                           llvm::Value* a,
                                                           llvm::Value* b,
                                                           bool for_case) {
  llvm::Function* program_called = state.module->getFunction("strequals");
  if (!program_called) return std::unexpected("strequals not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 4)
    return std::unexpected("Program strequals is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(), a, b,
      llvm::ConstantInt::getBool(*state.context, for_case)};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string>
runtime_fork_process_and_capture_stdout(CodegenState& state) {
  llvm::Function* program_called =
      state.module->getFunction("fork_process_and_capture_stdout");
  if (!program_called)
    return std::unexpected("fork_process_and_capture_stdout not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected(
        "Func fork_process_and_capture_stdout is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value()};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> runtime_fork_process_and_capture_stdin(
    CodegenState& state) {
  llvm::Function* program_called =
      state.module->getFunction("fork_process_and_capture_stdin");
  if (!program_called)
    return std::unexpected("fork_process_and_capture_stdin not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected(
        "Func fork_process_and_capture_stdin is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value()};

  return state.builder->CreateCall(program_called, arg_values);
}
std::expected<llvm::Value*, std::string> runtime_fork_process(
    CodegenState& state) {
  llvm::Function* program_called =
      state.module->getFunction("fork_process");
  if (!program_called)
    return std::unexpected("fork_process not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected(
        "Func fork_process is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value()};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> runtime_exit(CodegenState& state,
                                                      llvm::Value* status) {
  llvm::Function* program_called = state.module->getFunction("exit_helper");
  if (!program_called) return std::unexpected("exit_helper not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 1)
    return std::unexpected("Func exit_helper is illdefined");

  std::vector<llvm::Value*> arg_values = {status};

  return state.builder->CreateCall(program_called, arg_values);
}

std::expected<llvm::Value*, std::string> runtime_wait_two_pid(
    CodegenState& state, llvm::Value* pid1, llvm::Value* pid2) {
  llvm::Function* program_called = state.module->getFunction("wait_two_pid");
  if (!program_called) return std::unexpected("wait_two_pid not defined");

  // If argument mismatch error.
  if (program_called->arg_size() != 3)
    return std::unexpected("Func wait_two_pid is illdefined");

  if (!state.named_values["variable_memory"].has_value()) {
    return std::unexpected("Variable map does not exist");
  }

  std::vector<llvm::Value*> arg_values = {
      state.named_values["variable_memory"].value(), pid1, pid2};

  return state.builder->CreateCall(program_called, arg_values);
}
