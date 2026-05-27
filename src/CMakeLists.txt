add_executable(llsh
 main.cpp
 lexer.cpp
 ast.cpp
 codegen.cpp
 repl.cpp
 prototypes.cpp
 compiler.cpp
 interpret.cpp
 ../std/main.cpp
)


CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter@0.26.9"
)

CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter-bash@0.25.1"
)

CPMAddPackage(
  URI "gh:CLIUtils/CLI11@2.6.2"
)

CPMAddPackage(
  URI "gh:daanx/isocline@1.1.0"
  OPTIONS "IC_BUILD_TESTS Off"
)


# 22 or above needed!
find_package(LLVM)

if(${LLVM_VERSION} VERSION_LESS "22.1.5")
  CPMAddPackage(
    URI "gh:llvm/llvm-project#llvmorg-22.1.5"
  )
endif()

target_compile_options(llsh PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(llsh PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(llsh LLVM-22 isocline CLI11::CLI11 tree-sitter-bash tree-sitter)

add_custom_command(TARGET llsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")

install(TARGETS llsh DESTINATION bin)
