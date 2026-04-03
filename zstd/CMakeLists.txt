project(zstd)

file(GLOB zstd_SRC_FILES "zstd/lib/common/*.c" "zstd/lib/decompress/*.c")

set(zstd_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/zstd/lib/common" "${PROJECT_SOURCE_DIR}/zstd/lib/decompress" "${PROJECT_SOURCE_DIR}/zstd" PARENT_SCOPE)

if(SINGLE_LIBRARY)
  set(zstd_SRC_FILES ${zstd_SRC_FILES} PARENT_SCOPE)
  add_library(zstd INTERFACE)
else()
  include_directories("${PROJECT_SOURCE_DIR}/zstd/lib/common" "${PROJECT_SOURCE_DIR}/zstd/lib/decompress" "${PROJECT_SOURCE_DIR}/zstd")
  add_library(zstd OBJECT ${zstd_SRC_FILES})
endif()
