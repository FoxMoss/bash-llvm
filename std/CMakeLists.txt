set(BUILD_SHARED_LIBS ON)

add_library(stdllsh
  main.cpp
  programs.cpp
)


if(TRIPLET STREQUAL "x86_64-linux-musl")
  target_compile_options(stdllsh PUBLIC -static)
  target_link_options(stdllsh PUBLIC -static -static-libstdc++ -static-libgcc)
endif()

install(TARGETS stdllsh DESTINATION lib)
