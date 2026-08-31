cmake_minimum_required(VERSION 3.22)

project(box3d_defold_dependencies LANGUAGES C CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT BOX3D_SOURCE_DIR)
    message(FATAL_ERROR "Set BOX3D_SOURCE_DIR to the fetched upstream Box3D source directory")
endif()

# Build Box3D as a child project. This is important for wasm-web: upstream only
# enables its pthread executable flags when it is the top-level project.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_PROFILE OFF CACHE BOOL "" FORCE)
set(BOX3D_VALIDATE OFF CACHE BOOL "" FORCE)

add_subdirectory("${BOX3D_SOURCE_DIR}" upstream-box3d EXCLUDE_FROM_ALL)
set_target_properties(box3d PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(EXTENSION_BOX3D_BUILD_TESTS "Build the small upstream integration smoke test" OFF)
if(EXTENSION_BOX3D_BUILD_TESTS)
    enable_testing()
    add_executable(box3d_smoke tests/box3d_smoke.c)
    target_link_libraries(box3d_smoke PRIVATE box3d)
    add_test(NAME box3d_smoke COMMAND box3d_smoke)
endif()
