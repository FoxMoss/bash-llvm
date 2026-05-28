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

set(CPM_SOURCE_CACHE ${CMAKE_BINARY_DIR}/cpmcache)

CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter@0.26.9"
  OPTIONS "BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
  URI "gh:tree-sitter/tree-sitter-bash@0.25.1"
  PATCHES "../tree-sitter-bash.patch"
  OPTIONS "IGNORE_CLI ON" "BUILD_STATIC ON" "BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
  URI "gh:CLIUtils/CLI11@2.6.2"
)

CPMAddPackage(
  URI "gh:daanx/isocline@1.1.0"
  PATCHES "../isocline.patch"
  OPTIONS "IC_BUILD_TESTS OFF"
)

# 22 or above needed!
find_package(LLVM REQUIRED)

target_compile_options(llsh PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(llsh PRIVATE -Wall -Wextra -Wpedantic)

list(APPEND LINKED_LIBRARIES isocline CLI11::CLI11 tree-sitter-bash tree-sitter)

if(BUILD_STATIC)
  include_directories(/usr/lib/llvm22/include/)
  link_directories(/usr/lib/llvm22/lib/)

  execute_process(
    COMMAND llvm-config --libdir
    OUTPUT_VARIABLE LLVM_LIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(
    COMMAND llvm-config --link-static --libnames all
    OUTPUT_VARIABLE LLVM_LIB_NAMES OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(
    COMMAND llvm-config --system-libs --link-static
    OUTPUT_VARIABLE LLVM_SYS_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

  separate_arguments(LLVM_LIB_NAMES_LIST UNIX_COMMAND "${LLVM_LIB_NAMES}")
  foreach(libname IN LISTS LLVM_LIB_NAMES_LIST)
    list(APPEND LLVM_STATIC_LIBS "${LLVM_LIB_DIR}/${libname}")
  endforeach()

  list(APPEND LINKED_LIBRARIES ${LLVM_STATIC_LIBS} z zstd)
else()
  list(APPEND LINKED_LIBRARIES LLVM-22)
endif()

target_link_libraries(llsh ${LINKED_LIBRARIES})

add_custom_command(TARGET llsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")

install(TARGETS llsh DESTINATION bin)
