cmake_minimum_required(VERSION 3.15)
project(CoreVR-Toolkit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 REQUIRED)

include(FetchContent)

# Try to find OpenVR headers and library locally
find_path(OPENVR_INCLUDE_DIR openvr.h PATHS /usr/include /usr/local/include)
find_library(OPENVR_LIBRARY NAMES openvr_api openvr PATHS /usr/lib /usr/local/lib)

if(NOT OPENVR_INCLUDE_DIR OR NOT OPENVR_LIBRARY)
  message(STATUS "OpenVR not found locally; Fetching OpenVR via FetchContent...")
  FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(openvr)
  if(NOT OPENVR_INCLUDE_DIR)
    set(OPENVR_INCLUDE_DIR ${openvr_SOURCE_DIR}/headers)
  endif()
  if(NOT OPENVR_LIBRARY)
    # The fetched project provides a target `openvr_api` when built; attempt to link to it by name
    set(OPENVR_LIBRARY openvr_api)
  endif()
endif()

include_directories(${PROJECT_SOURCE_DIR}/include ${OPENVR_INCLUDE_DIR})

add_library(corevr_engine STATIC src/engine/main.cpp)
target_include_directories(corevr_engine PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(corevr_engine PUBLIC cxx_std_20)
if(OPENVR_LIBRARY)
  target_link_libraries(corevr_engine PUBLIC ${OPENVR_LIBRARY})
endif()
if(WIN32)
  target_link_libraries(corevr_engine PUBLIC d3d11 dxgi)
endif()

pybind11_add_module(corevr_bridge src/bridge/bindings.cpp)
target_link_libraries(corevr_bridge PRIVATE corevr_engine)
target_include_directories(corevr_bridge PRIVATE ${PROJECT_SOURCE_DIR}/include ${OPENVR_INCLUDE_DIR})

install(TARGETS corevr_bridge corevr_engine
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
