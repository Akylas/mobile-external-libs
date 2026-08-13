project(date)

file(GLOB date_SRC_FILES "src/*.cpp")

set(date_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include" PARENT_SCOPE)

# Built as C++17: under C++20 libc++ declares its own chrono stream operators, which are ambiguous
# with date's own.
if(MSVC)
  set(date_FLAGS "/std:c++17")
else()
  set(date_FLAGS "-std=c++17")
endif()

if(SINGLE_LIBRARY)
  set(date_SRC_FILES ${date_SRC_FILES} PARENT_SCOPE)
  set(date_FLAGS ${date_FLAGS} PARENT_SCOPE)
  add_library(date INTERFACE)
else()
  include_directories("${PROJECT_SOURCE_DIR}/include")
  add_compile_options(${date_FLAGS})
  add_library(date OBJECT ${date_SRC_FILES})
endif()

