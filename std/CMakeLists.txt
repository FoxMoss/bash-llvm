set(BUILD_SHARED_LIBS ON)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

add_library(stdllsh
  main.cpp
  programs.cpp
)

# target_compile_options(stdllsh PUBLIC -static)
# target_link_options(stdllsh PUBLIC -static -static-libstdc++ -static-libgcc -static-libm)

install(TARGETS stdllsh DESTINATION lib)
