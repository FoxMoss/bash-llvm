CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter@0.26.9"
  OPTIONS "BUILD_SHARED_LIBS OFF"
  GIT_SUBMODULES_RECURSE OFF
)

include_directories(${tree-sitter_SOURCE_DIR}/lib/include/)
message(${tree-sitter_SOURCE_DIR}/lib/include/)


CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter-bash@0.25.1"
  PATCHES "../tree-sitter-bash.patch"
  OPTIONS "IGNORE_CLI ON" "BUILD_STATIC ON" "BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
  URI "gh:CLIUtils/CLI11@2.6.2"
)

if(USE_BELLADONNA)
  CPMAddPackage(
    URI "gh:FoxMoss/belladonna#b4ac652"
    OPTIONS "CMAKE_CXX_FLAGS ${FUSE_INCLUDE} -O3 -DFUSE_USE_VERSION=317"
    "CMAKE_C_FLAGS ${FUSE_INCLUDE} -O3 -DFUSE_USE_VERSION=317"
  )
endif()

CPMAddPackage(
  URI "gh:daanx/isocline@1.1.0"
  PATCHES "../isocline.patch"
  OPTIONS "IC_BUILD_TESTS OFF"
)

add_executable(llsh
 main.cpp
 lexer.cpp
 repl.cpp
 prototypes.cpp
 compiler.cpp
 interpret.cpp
 whereami.cpp
 completions.cpp
 ast/ast.cpp
 ast/args.cpp
 ast/helper.cpp
 codegen/codegen.cpp
 codegen/loops.cpp
 codegen/helper.cpp
 codegen/math.cpp
 ../std/main.cpp
 ../std/programs.cpp
)

target_precompile_headers(llsh
  PRIVATE
  <llvm/ADT/ArrayRef.h>
  <llvm/IR/BasicBlock.h>
  <llvm/IR/Constant.h>
  <llvm/IR/IRBuilder.h>
  <llvm/IR/Intrinsics.h>
  <llvm/IR/Value.h>
  <llvm/Support/Alignment.h>
  <llvm/Support/raw_ostream.h>
  <llvm/LinkAllPasses.h>
  <llvm/Passes/CodeGenPassBuilder.h>
  <llvm/Passes/PassBuilder.h>
  <llvm/Passes/StandardInstrumentations.h>
  <llvm/Support/ModRef.h>
  <llvm/Support/TargetSelect.h>
  <llvm/Transforms/Scalar/Reassociate.h>
  <llvm/Transforms/Scalar/SimplifyCFG.h>
  <llvm/IR/DerivedTypes.h>
  ast/ast.h
  lexer.h
  codegen.h
  ../std/main.h
)


target_compile_options(llsh PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(llsh PRIVATE -Wall -Wextra -Wpedantic)

if(USE_BELLADONNA)
  list(APPEND REL_EXTERNAL_SOURCES belladonna)
  target_compile_definitions(USE_BELLADONNA)
endif()

list(APPEND LINKED_LIBRARIES isocline CLI11::CLI11 tree-sitter-bash tree-sitter ${REL_EXTERNAL_SOURCES})

target_link_libraries(llsh ${LINKED_LIBRARIES})
target_include_directories(llsh PRIVATE .)

add_custom_command(TARGET llsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")

install(TARGETS llsh DESTINATION bin)

if(USE_BELLADONNA)
  install(TARGETS belladonna DESTINATION lib)
endif()

