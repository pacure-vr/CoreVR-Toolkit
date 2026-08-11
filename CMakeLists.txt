cmake_minimum_required(VERSION 3.15)
project(CoreVR-Toolkit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

find_package(pybind11 REQUIRED)

# Use a fixed local OpenVR checkout to avoid repeated downloads and preserve cache.
set(OPENVR_BASE_DIR ${PROJECT_SOURCE_DIR}/data/openvr)
find_path(OPENVR_INCLUDE_DIR openvr.h PATHS ${OPENVR_BASE_DIR}/headers /usr/include /usr/local/include)
find_library(OPENVR_LIBRARY NAMES openvr_api openvr PATHS ${OPENVR_BASE_DIR}/lib/win64 ${OPENVR_BASE_DIR}/lib/win32 /usr/lib /usr/local/lib)

if(NOT OPENVR_INCLUDE_DIR)
  message(FATAL_ERROR "OpenVR headers not found. Please clone OpenVR into ${OPENVR_BASE_DIR}.")
endif()

if(NOT OPENVR_LIBRARY)
  message(WARNING "OpenVR library not found in ${OPENVR_BASE_DIR}/lib or system paths. Build may fail until the correct library is available.")
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
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)