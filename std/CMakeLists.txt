set(BUILD_SHARED_LIBS ON)

add_library(stdllsh STATIC
  main.cpp
  programs.cpp
)

target_compile_options(stdllsh PUBLIC -static)
target_link_options(stdllsh PUBLIC -static -static-libstdc++ -static-libgcc -static-libm)
target_link_libraries(stdllsh m)

install(TARGETS stdllsh DESTINATION lib)
