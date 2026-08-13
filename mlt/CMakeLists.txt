project(mlt)

# Decoder only. The encoder (and with it fsst, earcut and nlohmann/json) is not built, so none of
# maplibre-tile-spec's own submodules have to be checked out. MLT_WITH_JSON stays undefined.
set(MLT_ROOT "${PROJECT_SOURCE_DIR}/mlt/cpp")

set(mlt_SRC_FILES
  "${MLT_ROOT}/src/mlt/decoder.cpp"
  "${MLT_ROOT}/src/mlt/decode/int.cpp"
  "${MLT_ROOT}/src/mlt/feature.cpp"
  "${MLT_ROOT}/src/mlt/geometry_vector.cpp"
  "${MLT_ROOT}/src/mlt/layer.cpp"
  "${MLT_ROOT}/src/mlt/metadata/stream.cpp"
  "${MLT_ROOT}/src/mlt/metadata/tileset.cpp"
  "${MLT_ROOT}/src/mlt/properties.cpp"
  "${MLT_ROOT}/src/mlt/util/rle.cpp"
  "${MLT_ROOT}/vendor/fastpfor/fastpfor/bitpacking.cpp"
)

set(mlt_INCLUDE_DIRS
  "${MLT_ROOT}/include"
  "${MLT_ROOT}/src"
  "${MLT_ROOT}/vendor/fastpfor"
  PARENT_SCOPE)

if(SINGLE_LIBRARY)
  set(mlt_SRC_FILES ${mlt_SRC_FILES} PARENT_SCOPE)
  add_library(mlt INTERFACE)
else()
  include_directories("${MLT_ROOT}/include" "${MLT_ROOT}/src" "${MLT_ROOT}/vendor/fastpfor")
  add_library(mlt OBJECT ${mlt_SRC_FILES})
endif()
