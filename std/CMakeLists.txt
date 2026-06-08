set(BUILD_SHARED_LIBS ON)

add_library(stdllsh
  main.cpp
  programs.cpp
)

install(TARGETS stdllsh DESTINATION lib)

if(TRIPLET STREQUAL "x86_64-linux-gnu")
  list(APPEND REL_EXTERNAL_SOURCES stdllsh)
endif()

