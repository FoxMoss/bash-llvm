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

include(FetchContent)

FetchContent_Declare(
  CLI11 
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(CLI11)

set(IC_BUILD_TESTS Off)
FetchContent_Declare(
  isocline
  GIT_REPOSITORY https://github.com/daanx/isocline.git
  GIT_TAG v1.1.0
)

FetchContent_MakeAvailable(isocline)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wpedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
target_compile_options(llsh PRIVATE -Wall -Wextra)
target_link_options(llsh PRIVATE -Wall -Wextra)

target_link_libraries(llsh LLVM-22 isocline CLI11::CLI11)

add_custom_command(TARGET llsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")

install(TARGETS llsh DESTINATION bin)
