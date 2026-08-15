project(valhalla)

file(GLOB valhalla_SRC_FILES "proto/valhalla/proto/*.pb.cc" "valhalla/src/worker.cc" "valhalla/src/exceptions.cc" "valhalla/src/proto_conversions.cc" "valhalla/src/baldr/*.cc" "valhalla/src/loki/*.cc" "valhalla/src/meili/*.cc" "valhalla/src/midgard/*.cc" "valhalla/src/odin/*.cc" "valhalla/src/sif/*.cc" "valhalla/src/skadi/*.cc" "valhalla/src/thor/*.cc" "valhalla/src/tyr/*.cc")
# tz_alt.cpp is date's tz implementation with the IANA database embedded (config/tzdb),
# which is why the date subproject here is headers only.
list(APPEND valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/baldr/tz_alt.cpp")

# On Apple, tz_alt.cpp resolves the local zone through date::iOSUtils (NSTimeZone), implemented
# here. Upstream adds it the same way in src/baldr/CMakeLists.txt.
if(APPLE)
  list(APPEND valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/third_party/date/src/ios.mm")
endif()
# list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/baldr/curler.cc")
list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/meili/valhalla_run_map_match.cc" "${PROJECT_SOURCE_DIR}/valhalla/src/meili/traffic_segment_matcher.cc")
list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/thor/formlocalpath.cc" "${PROJECT_SOURCE_DIR}/valhalla/src/thor/expandfromnode.cc")
list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/tyr/actor.cc")
# /tile is the only vtzero+protozero consumer and is useless on mobile.
list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/loki/tile_action.cc")

# Matrix, isochrone, locate, height, expansion, status and optimized route. Nothing here reaches
# them with this OFF: ENABLE_SERVICES is never defined, so the workers' action dispatcher is
# compiled out, and both consumers call loki/thor/odin route(), trace_route() and narrate()
# directly then tyr::serializeDirections. Turn it ON to expose the full tyr::actor_t surface.
# They cannot be left to --gc-sections: each of these translation units has a namespace-scope
# static that roots it from .init_array, so the linker keeps it whatever else it strips.
option(VALHALLA_SERVICES "Build the Valhalla service actions beyond routing and map matching" ON)

if(NOT VALHALLA_SERVICES)
  # loki/polygon_search.cc, skadi/sample.cc and baldr/connectivity_map.cc are NOT in this list:
  # loki_worker_t holds a skadi::sample and a connectivity_map_t by value and calls edges_in_rings,
  # so dropping them fails the link. That needs the worker patched, in the nested valhalla submodule.
  foreach(VALHALLA_UNUSED_SERVICE_FILE IN ITEMS
      thor/matrix_action.cc thor/isochrone_action.cc thor/expansion_action.cc
      thor/optimized_route_action.cc thor/status_action.cc thor/optimizer.cc
      loki/matrix_action.cc loki/isochrone_action.cc loki/height_action.cc
      loki/transit_available_action.cc loki/status_action.cc loki/locate_action.cc
      loki/node_search.cc
      tyr/isochrone_serializer.cc tyr/matrix_serializer.cc tyr/locate_serializer.cc
      tyr/height_serializer.cc tyr/expansion_serializer.cc tyr/transit_available_serializer.cc)
    list(REMOVE_ITEM valhalla_SRC_FILES "${PROJECT_SOURCE_DIR}/valhalla/src/${VALHALLA_UNUSED_SERVICE_FILE}")
  endforeach()
endif()

# HAS_REMOTE_API/AUTO_DOWNLOAD keep date's tz out of curl and the shell (upstream does the same);
# the tz database is compiled in from config/tzdb.
set(valhalla_FLAGS "-DRAPIDJSON_HAS_STDSTRING=1" "-DHAS_REMOTE_API=0" "-DAUTO_DOWNLOAD=0" "-DUSE_SHELL_API=0")
if(WIN32)
  set(valhalla_FLAGS ${valhalla_FLAGS} "/wd4018" "/wd4101" "/wd4267" "/wd4305" "/wd4334" "/wd4804" "/wd4805")
elseif(ANDROID OR APPLE)
  set(valhalla_FLAGS ${valhalla_FLAGS} "-Wno-conversion" "-Wno-narrowing" "-Wno-shorten-64-to-32")
endif()

set(valhalla_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/config" "${PROJECT_SOURCE_DIR}/config/valhalla" "${PROJECT_SOURCE_DIR}/config/valhalla/odin" "${PROJECT_SOURCE_DIR}/config/tzdb" "${PROJECT_SOURCE_DIR}/proto" "${PROJECT_SOURCE_DIR}/proto/valhalla" "${PROJECT_SOURCE_DIR}/valhalla" "${PROJECT_SOURCE_DIR}/valhalla/valhalla" "${PROJECT_SOURCE_DIR}/valhalla/third_party/date/include" "${PROJECT_SOURCE_DIR}/valhalla/third_party/unordered_dense/include" PARENT_SCOPE)

if(SINGLE_LIBRARY)
  set(valhalla_SRC_FILES ${valhalla_SRC_FILES} PARENT_SCOPE)
  set(valhalla_FLAGS ${valhalla_FLAGS} PARENT_SCOPE)
  add_library(valhalla INTERFACE)
else()
  add_compile_options(${valhalla_FLAGS})
  include_directories("${PROJECT_SOURCE_DIR}/config" "${PROJECT_SOURCE_DIR}/config/valhalla" "${PROJECT_SOURCE_DIR}/config/valhalla/odin" "${PROJECT_SOURCE_DIR}/config/tzdb" "${PROJECT_SOURCE_DIR}/proto" "${PROJECT_SOURCE_DIR}/proto/valhalla" "${PROJECT_SOURCE_DIR}/valhalla" "${PROJECT_SOURCE_DIR}/valhalla/valhalla" "${PROJECT_SOURCE_DIR}/valhalla/third_party/date/include" "${PROJECT_SOURCE_DIR}/valhalla/third_party/unordered_dense/include" "${PROJECT_SOURCE_DIR}/../protobuf/protobuf/src" "${PROJECT_SOURCE_DIR}/../rapidjson/include" "${PROJECT_SOURCE_DIR}/../sqlite" "${PROJECT_SOURCE_DIR}/../sqlite3pp")
#  include_directories("${PROJECT_SOURCE_DIR}/config" "${PROJECT_SOURCE_DIR}/config/valhalla" "${PROJECT_SOURCE_DIR}/proto" "${PROJECT_SOURCE_DIR}/proto/valhalla" "${PROJECT_SOURCE_DIR}/valhalla" "${PROJECT_SOURCE_DIR}/valhalla/valhalla" "${PROJECT_SOURCE_DIR}/../protobuf/protobuf/src"  "${PROJECT_SOURCE_DIR}/../protobuf/protobuf/third_party/abseil-cpp" "${PROJECT_SOURCE_DIR}/../protobuf/protobuf/third_party/utf8_range" "${PROJECT_SOURCE_DIR}/../rapidjson/include" "${PROJECT_SOURCE_DIR}/../date/include" "${PROJECT_SOURCE_DIR}/../sqlite" "${PROJECT_SOURCE_DIR}/../sqlite3pp")
  add_library(valhalla OBJECT ${valhalla_SRC_FILES})
endif()
