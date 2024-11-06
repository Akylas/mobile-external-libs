project(date)

file(GLOB date_SRC_FILES "date/src/*.cpp")

set(date_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/date/include" PARENT_SCOPE)
set(date_FLAGS "-DUSE_OS_TZDB=1")
if(SINGLE_LIBRARY)
  set(date_SRC_FILES ${date_SRC_FILES} PARENT_SCOPE)
  set(date_FLAGS ${date_FLAGS} PARENT_SCOPE)
  add_library(date INTERFACE)
else()
  add_compile_options(${date_FLAGS})
  include_directories("${PROJECT_SOURCE_DIR}/date/include")
  add_library(date OBJECT ${date_SRC_FILES})
endif()

