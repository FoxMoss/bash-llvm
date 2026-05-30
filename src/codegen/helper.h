#pragma once

#include <llvm/IR/Value.h>

#include <expected>
#include <string>

#include "codegen.h"

std::expected<llvm::Value*, std::string> reduce_to_bool(CodegenState& state,
                                                        llvm::Value* val);
std::expected<llvm::Value*, std::string> runtime_store_args_variable_memory(
    CodegenState& state, llvm::Value* argc, llvm::Value* argv);
std::expected<llvm::Value*, std::string> store_variable_memory(
    CodegenState& state, std::string name, llvm::Value* value);
std::expected<llvm::Value*, std::string> cast_to_float(CodegenState& state,
                                                       llvm::Value* val);
std::expected<llvm::Value*, std::string> cast_to_str(CodegenState& state,
                                                     llvm::Value* val);
std::expected<llvm::Value*, std::string> cast_to_int(CodegenState& state,
                                                     llvm::Value* val);
std::expected<llvm::Value*, std::string> runtime_strlen(CodegenState& state,
                                                        llvm::Value* val);
std::expected<llvm::Value*, std::string> get_variable_memory(
    CodegenState& state, std::string name);
