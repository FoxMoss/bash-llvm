#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/SelfExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

#include <array>
#include <expected>
#include <filesystem>
#include <memory>
#include <print>
#include <vector>

#include "../std/main.h"
#include "whereami.h"

// based on
// https://github.com/llvm/llvm-project/blob/main/llvm/examples/Kaleidoscope/include/KaleidoscopeJIT.h

class BashJIT {
 public:
  std::unique_ptr<llvm::orc::ExecutionSession> execution_session;
  llvm::orc::MangleAndInterner mangle;

  llvm::orc::RTDyldObjectLinkingLayer object_layer;
  llvm::orc::IRCompileLayer compile_layer;
  llvm::DataLayout data_layout;
  llvm::orc::JITDylib& main_jit_dylib;

  BashJIT(std::unique_ptr<llvm::orc::ExecutionSession> execution_session,
          llvm::orc::JITTargetMachineBuilder jit_target_builder,
          llvm::DataLayout data_layout, SandboxingOptions sandbox)
      : execution_session(std::move(execution_session)),
        mangle(*this->execution_session, this->data_layout),
        object_layer(*this->execution_session,
                     [](const llvm::MemoryBuffer&) {
                       return std::make_unique<llvm::SectionMemoryManager>();
                     }),
        compile_layer(*this->execution_session, object_layer,
                      std::make_unique<llvm::orc::ConcurrentIRCompiler>(
                          std::move(jit_target_builder))),
        data_layout(std::move(data_layout)),
        main_jit_dylib(this->execution_session->createBareJITDylib("<main>")) {
    auto bundled_generator =
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            data_layout.getGlobalPrefix());
    if (bundled_generator.takeError()) {
#define SYMBOL(symb) \
  {mangle(#symb), {llvm::orc::ExecutorAddr::fromPtr(&(symb)), {}}},

      // clang-format off
          cantFail(main_jit_dylib.define(llvm::orc::absoluteSymbols({
              #include "symbols.inc"
          })));

          if (!sandbox.block_external_programs) {
            cantFail(main_jit_dylib.define(llvm::orc::absoluteSymbols({
              SYMBOL(external_program)
            })));
          }

      // clang-format on
#undef SYMBOL

    } else {
      main_jit_dylib.addGenerator(cantFail(std::move(bundled_generator)));

#define SYMBOL(symb)                           \
  {mangle(#symb),                              \
   {llvm::orc::ExecutorAddr::fromPtr(&(symb)), \
    llvm::JITSymbolFlags::Exported}},

      // clang-format off
      cantFail(main_jit_dylib.define(llvm::orc::absoluteSymbols({

          #include "symbols.inc"
      })));

      if (!sandbox.block_external_programs) {
        cantFail(main_jit_dylib.define(llvm::orc::absoluteSymbols({
          SYMBOL(external_program)
        })));
      }

      // clang-format on
#undef SYMBOL
    }

    if (jit_target_builder.getTargetTriple().isOSBinFormatCOFF()) {
      object_layer.setOverrideObjectFlagsWithResponsibilityFlags(true);
      object_layer.setAutoClaimResponsibilityForObjectSymbols(true);
    }
  }

  ~BashJIT() {
    if (auto err = execution_session->endSession())
      execution_session->reportError(std::move(err));
  }

  static std::expected<std::unique_ptr<BashJIT>, std::string> create(
      SandboxingOptions sandbox) {
    auto executor_process = llvm::orc::SelfExecutorProcessControl::Create();
    if (!executor_process)
      return std::unexpected("Failed to make SelfExecutorProcessControl");

    auto execution_session = std::make_unique<llvm::orc::ExecutionSession>(
        std::move(*executor_process));

    llvm::orc::JITTargetMachineBuilder jit_target_builder(
        execution_session->getExecutorProcessControl().getTargetTriple());

    auto data_layout = jit_target_builder.getDefaultDataLayoutForTarget();
    if (!data_layout) return std::unexpected("Failed to make data layout");

    return std::make_unique<BashJIT>(std::move(execution_session),
                                     std::move(jit_target_builder),
                                     std::move(*data_layout), sandbox);
  }

  std::expected<bool, std::string> add_module(
      llvm::orc::ThreadSafeModule thread_safe_module,
      llvm::orc::ResourceTrackerSP resource_tracker = nullptr) {
    if (!resource_tracker)
      resource_tracker = main_jit_dylib.getDefaultResourceTracker();
    auto compile_res =
        compile_layer.add(resource_tracker, std::move(thread_safe_module));
    if (compile_res) {
      return std::unexpected("Failed to add module");
    }
    return true;
  }

  std::expected<llvm::orc::ExecutorSymbolDef, std::string> lookup(
      std::string name) {
    auto lookup_res =
        execution_session->lookup({&main_jit_dylib}, mangle(name));
    if (lookup_res) {
      return lookup_res.get();
    }
    return std::unexpected("Failed to lookup");
  }
};
