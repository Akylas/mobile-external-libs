cmake_minimum_required(VERSION 3.15)
# Declare ASM in the project languages so CMake sets up ASM-related internal variables
project(zstd C ASM)

# convenience root
set(ZSTD_ROOT "${PROJECT_SOURCE_DIR}/zstd/lib")

# collect sources (C and assembly .S) from the same folders the original Makefile uses
file(GLOB ZSTD_COMMON "${ZSTD_ROOT}/common/*.c" "${ZSTD_ROOT}/common/*.S")
# compression intentionally excluded (project only needs decompression)
# file(GLOB ZSTD_COMPRESS "${ZSTD_ROOT}/compress/*.c" "${ZSTD_ROOT}/compress/*.S")
file(GLOB ZSTD_DECOMPRESS "${ZSTD_ROOT}/decompress/*.c" "${ZSTD_ROOT}/decompress/*.S")
# dictBuilder/ deprecated/ etc excluded to reduce symbols
# file(GLOB ZSTD_DICTBUILDER "${ZSTD_ROOT}/dictBuilder/*.c" "${ZSTD_ROOT}/dictBuilder/*.S")
file(GLOB ZSTD_LEGACY "${ZSTD_ROOT}/legacy/*.c" "${ZSTD_ROOT}/legacy/*.S")
# file(GLOB ZSTD_DEPRECATED "${ZSTD_ROOT}/deprecated/*.c" "${ZSTD_ROOT}/deprecated/*.S")
file(GLOB ZSTD_TOP "${ZSTD_ROOT}/*.c" "${ZSTD_ROOT}/*.S")

set(zstd_SRC_FILES
  ${ZSTD_COMMON}
  ${ZSTD_DECOMPRESS}
#  ${ZSTD_LEGACY}
  ${ZSTD_TOP}
)

set(zstd_FLAGS "")

# Mac Catalyst is built as a macOS Xcode project with -target ...-macabi forced onto the compiles,
# so Xcode's prelink (ld -r, macOS) rejects any real Mach-O object stamped Mac Catalyst. Under
# -flto every other translation unit is bitcode and carries no platform, which leaves this .S as
# the only object that trips it. Drop it there and take zstd's C huffman path.
if(APPLE AND (NOT IOS))
  list(REMOVE_ITEM zstd_SRC_FILES "${ZSTD_ROOT}/decompress/huf_decompress_amd64.S")
  set(zstd_FLAGS ${zstd_FLAGS} "-DZSTD_DISABLE_ASM")
endif()

set(zstd_INCLUDE_DIRS
  "${ZSTD_ROOT}/common"
  "${ZSTD_ROOT}/decompress"
  "${ZSTD_ROOT}"
  PARENT_SCOPE)

if(NOT DEFINED CMAKE_ASM_COMPILE_OBJECT)
  enable_language(ASM)
endif()

if(NOT DEFINED CMAKE_ASM_COMPILE_OBJECT)
  message(FATAL_ERROR "CMake assembler support not available (CMAKE_ASM_COMPILE_OBJECT undefined). "
                      "Ensure your generator/toolchain supports ASM or use a CMake >= 3.15-compatible toolchain.")
endif()

if(SINGLE_LIBRARY)
  # propagate sources to parent scope for single-library build
  set(zstd_SRC_FILES ${zstd_SRC_FILES} PARENT_SCOPE)
  set(zstd_FLAGS ${zstd_FLAGS} PARENT_SCOPE)
  add_library(zstd INTERFACE)
else()
  include_directories("${ZSTD_ROOT}/common" "${ZSTD_ROOT}/decompress" "${ZSTD_ROOT}")
  add_compile_options(${zstd_FLAGS})

  # create object library to be linked into your project
  add_library(zstd OBJECT ${zstd_SRC_FILES})

  # Ensure .S files are treated as assembler (preprocessed if capital S)
  foreach(_sfile ${zstd_SRC_FILES})
    get_filename_component(_ext ${_sfile} EXT)
    if(_ext STREQUAL ".S")
      set_source_files_properties(${_sfile} PROPERTIES LANGUAGE ASM)
    endif()
  endforeach()

  # replicate common flags from Makefile if needed (example)
  # target_compile_definitions(zstd PRIVATE ZSTD_MULTITHREAD) # enable when building dynamic lib
endif()
