
set(LLVM_VERSION "22.1.5")
set(LLVM_OS "")
set(LLVM_ARCH "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LLVM_OS "Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(LLVM_OS "macOS")
endif()


if(ZIG_ARCH STREQUAL "arm")
  set(LLVM_ARCH "ARM64")
elseif(ZIG_ARCH STREQUAL "x86_64")
  set(LLVM_ARCH "X64")
endif()



if(TRIPLET STREQUAL "x86_64-linux-gnu")
  CPMAddPackage(
      NAME LLVM-deb
      URL https://apt.llvm.org/bullseye/pool/main/l/llvm-toolchain-22/libllvm22_22.1.8~%2B%2B20260613092316%2Be80beda6e255-1~exp1~20260613092335.86_amd64.deb)
  CPMAddPackage(
      NAME LLVM
      URL ${LLVM-deb_SOURCE_DIR}/data.tar.xz)
    #link_directories(${LLVM_SOURCE_DIR}/lib/x86_64-linux-gnu/)

  list(APPEND EXTERNAL_SOURCES ${LLVM_SOURCE_DIR}/lib/x86_64-linux-gnu/libLLVM-22.so)

    CPMAddPackage(
      NAME LLVM-dev-deb
      URL https://apt.llvm.org/bullseye/pool/main/l/llvm-toolchain-22/llvm-22-dev_22.1.8~%2B%2B20260613092316%2Be80beda6e255-1~exp1~20260613092335.86_amd64.deb)
    CPMAddPackage(
      NAME LLVM-dev
      URL ${LLVM-dev-deb_SOURCE_DIR}/data.tar.xz)

  include_directories(${LLVM-dev_SOURCE_DIR}/include/llvm-22/)
  include_directories(${LLVM-dev_SOURCE_DIR}/include/llvm-c-22/)

  CPMAddPackage(
    NAME z3-deb 
    URL http://http.us.debian.org/debian/pool/main/z/z3/libz3-4_4.8.10-1_amd64.deb)
  CPMAddPackage(
    NAME z3 
    URL ${z3-deb_SOURCE_DIR}/data.tar.xz)

  #link_directories(${z3_SOURCE_DIR}/lib/x86_64-linux-gnu/)

  list(APPEND EXTERNAL_SOURCES ${z3_SOURCE_DIR}/lib/x86_64-linux-gnu/libz3.so.4)

  CPMAddPackage(
    NAME ffi7-deb 
    URL http://http.us.debian.org/debian/pool/main/libf/libffi/libffi7_3.3-6_amd64.deb)
  CPMAddPackage(
    NAME ffi7 
    URL ${ffi7-deb_SOURCE_DIR}/data.tar.xz)

  #link_directories(${ffi7_SOURCE_DIR}/lib/x86_64-linux-gnu/)

  list(APPEND EXTERNAL_SOURCES ${ffi7_SOURCE_DIR}/lib/x86_64-linux-gnu/libffi.so.7)

  CPMAddPackage(
    NAME edit2-deb 
    URL http://http.us.debian.org/debian/pool/main/libe/libedit/libedit2_3.1-20191231-2+b1_amd64.deb)
  CPMAddPackage(
    NAME edit2 
    URL ${edit2-deb_SOURCE_DIR}/data.tar.xz)

  #link_directories(${edit2_SOURCE_DIR}/lib/x86_64-linux-gnu/)
  list(APPEND EXTERNAL_SOURCES ${edit2_SOURCE_DIR}/lib/x86_64-linux-gnu/libedit.so.2)

  CPMAddPackage(
    NAME xml2-deb 
    URL https://security.debian.org/debian-security/pool/updates/main/libx/libxml2/libxml2_2.9.10+dfsg-6.7+deb11u10_amd64.deb) 
  CPMAddPackage(
    NAME xml2 
    URL ${xml2-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${xml2_SOURCE_DIR}/lib/x86_64-linux-gnu/libxml2.so.2)

  CPMAddPackage(
    NAME icu-deb 
    URL http://security.debian.org/debian-security/pool/updates/main/i/icu/libicu67_67.1-7+deb11u1_amd64.deb) 
  CPMAddPackage(
    NAME icu 
    URL ${icu-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${icu_SOURCE_DIR}/lib/x86_64-linux-gnu/libicuuc.so.67)
  list(APPEND EXTERNAL_SOURCES ${icu_SOURCE_DIR}/lib/x86_64-linux-gnu/libicudata.so.67)

  CPMAddPackage(
    NAME bsd-deb 
    URL http://http.us.debian.org/debian/pool/main/libb/libbsd/libbsd0_0.11.3-1+deb11u1_amd64.deb) 
  CPMAddPackage(
    NAME bsd 
    URL ${bsd-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${bsd_SOURCE_DIR}/lib/x86_64-linux-gnu/libbsd.so.0)

  CPMAddPackage(
    NAME md-deb 
    URL http://http.us.debian.org/debian/pool/main/libm/libmd/libmd0_1.0.3-3_amd64.deb) 
  CPMAddPackage(
    NAME md 
    URL ${md-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${md_SOURCE_DIR}/lib/x86_64-linux-gnu/libmd.so.0)

  CPMAddPackage(
    NAME tinfo6-deb 
    URL http://http.us.debian.org/debian/pool/main/n/ncurses/libtinfo6_6.4-4_amd64.deb) 
  CPMAddPackage(
    NAME tinfo6 
    URL ${tinfo6-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${tinfo6_SOURCE_DIR}/lib/x86_64-linux-gnu/libtinfo.so.6)

  CPMAddPackage(
    NAME zlib1g-deb 
    URL http://http.us.debian.org/debian/pool/main/z/zlib/zlib1g_1.2.13.dfsg-1_amd64.deb)
  CPMAddPackage(
    NAME zlib1g 
    URL ${zlib1g-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${zlib1g_SOURCE_DIR}/lib/x86_64-linux-gnu/libz.so.1)

  CPMAddPackage(
    NAME zstd-deb 
    URL http://http.us.debian.org/debian/pool/main/libz/libzstd/libzstd1_1.5.4+dfsg2-5_amd64.deb)
  CPMAddPackage(
    NAME zstd 
    URL ${zstd-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${zstd_SOURCE_DIR}/lib/x86_64-linux-gnu/libzstd.so.1)

  CPMAddPackage(
    NAME lzma-deb 
    URL http://http.us.debian.org/debian/pool/main/x/xz-utils/liblzma5_5.8.3-1_amd64.deb)
  CPMAddPackage(
    NAME lzma 
    URL ${lzma-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${lzma_SOURCE_DIR}/lib/x86_64-linux-gnu/liblzma.so.5)

  CPMAddPackage(
    NAME fuse-deb 
    URL http://http.us.debian.org/debian/pool/main/f/fuse3/libfuse3-3_3.14.0-4_amd64.deb)
  CPMAddPackage(
    NAME fuse 
    URL ${fuse-deb_SOURCE_DIR}/data.tar.xz)

  list(APPEND EXTERNAL_SOURCES ${fuse_SOURCE_DIR}/lib/x86_64-linux-gnu/libfuse3.so.3)

  CPMAddPackage(
    NAME fuse-dev-deb 
    URL http://http.us.debian.org/debian/pool/main/f/fuse3/libfuse3-dev_3.14.0-4_amd64.deb)
  CPMAddPackage(
    NAME fuse-dev 
    URL ${fuse-dev-deb_SOURCE_DIR}/data.tar.xz)

  include_directories(${fuse-dev_SOURCE_DIR}/include/fuse3/)
  set(FUSE_INCLUDE "-I${fuse-dev_SOURCE_DIR}/include/fuse3/")

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
  foreach(SOFILE ${EXTERNAL_SOURCES})
    get_filename_component(SOFILE_BARE ${SOFILE} NAME)
    get_filename_component(SOFILE_DIR ${SOFILE} DIRECTORY)

    file(COPY "${SOFILE}" DESTINATION "${CMAKE_BINARY_DIR}/lib/" FOLLOW_SYMLINK_CHAIN)

    file(REAL_PATH "${CMAKE_BINARY_DIR}/lib/${SOFILE_BARE}" SOFILE_LIB)

    list(APPEND REL_EXTERNAL_SOURCES "${SOFILE_LIB}")
    if(IS_SYMLINK ${SOFILE})
      file(READ_SYMLINK ${SOFILE} SOFILE_REAL)
      list(APPEND EXTERNAL_SOURCES "${SOFILE_DIR}/${SOFILE_REAL}")
    endif()
  endforeach()


  install(FILES ${EXTERNAL_SOURCES} DESTINATION lib)

  set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
  set(BUILD_SHARED_LIBS ON)

elseif(TRIPLET STREQUAL "x86_64-linux-musl")
  find_package(LLVM REQUIRED)

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

  separate_arguments(LLVM_SYS_LIBS_LIST UNIX_COMMAND "${LLVM_SYS_LIBS}")

  find_package(PkgConfig REQUIRED)

  pkg_check_modules(FUSE REQUIRED fuse3)

  include_directories(${FUSE_INCLUDE_DIRS})
  add_compile_options(${FUSE_CFLAGS_OTHER})
  link_libraries(${FUSE_LIBRARIES})

  add_compile_definitions(NDEBUG)

  list(APPEND REL_EXTERNAL_SOURCES  
    -Wl,--start-group ${LLVM_STATIC_LIBS} -Wl,--end-group
    ${LLVM_SYS_LIBS_LIST}
  )

  include_directories(/usr/include/llvm22/)

endif()
