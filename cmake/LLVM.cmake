
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



if((ZIG_ABI STREQUAL "gnu") OR (NOT ZIG_ABI))
  CPMAddPackage(
      NAME LLVM-deb
      URL https://apt.llvm.org/bullseye/pool/main/l/llvm-toolchain-22/libllvm22_22.1.7~%2B%2B20260522062526%2B81c69e140401-1~exp1~20260522182547.83_amd64.deb)
  CPMAddPackage(
      NAME LLVM
      URL ${LLVM-deb_SOURCE_DIR}/data.tar.xz)
    #link_directories(${LLVM_SOURCE_DIR}/lib/x86_64-linux-gnu/)

  list(APPEND EXTERNAL_SOURCES ${LLVM_SOURCE_DIR}/lib/x86_64-linux-gnu/libLLVM-22.so)

    CPMAddPackage(
      NAME LLVM-dev-deb
      URL https://apt.llvm.org/bullseye/pool/main/l/llvm-toolchain-22/llvm-22-dev_22.1.7~%2B%2B20260522062526%2B81c69e140401-1~exp1~20260522182547.83_amd64.deb)
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
    URL http://security.debian.org/debian-security/pool/updates/main/libx/libxml2/libxml2_2.9.10+dfsg-6.7+deb11u9_amd64.deb) 
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

elseif(ZIG_ABI STREQUAL "musl")
  CPMAddPackage(
      NAME LLVM
      URL https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/llvm22-libs-22.1.3-r0.apk)

  CPMAddPackage(
      NAME LLVM-dev
      URL https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/llvm22-dev-22.1.3-r0.apk)

  CPMAddPackage(
      NAME musl
      URL https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/musl-1.2.6-r2.apk)


  link_directories(${LLVM_SOURCE_DIR}/usr/lib/)
  link_directories(${musl_SOURCE_DIR}/lib/)
  include_directories(${LLVM-dev_SOURCE_DIR}/usr/lib/llvm22/include/)
endif()
