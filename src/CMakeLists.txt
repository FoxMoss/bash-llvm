add_executable(llsh
 main.cpp
 lexer.cpp
 ast.cpp
)

target_compile_options(llsh PRIVATE -Wall -Wextra -Wpedantic -fsanitize=address)
target_link_options(llsh PRIVATE -Wall -Wextra -Wpedantic -fsanitize=address)

target_link_libraries(llsh LLVM-22)

add_custom_command(TARGET llsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")

install(TARGETS llsh DESTINATION bin)
