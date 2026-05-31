CPMAddPackage(
  URI "gh:nlohmann/json@3.12.0"
)

add_executable(llsh-test-runner
 runner.cpp
)

target_link_libraries(llsh-test-runner nlohmann_json::nlohmann_json)

add_custom_command(TARGET llsh-test-runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Add clangd to the base directory")
