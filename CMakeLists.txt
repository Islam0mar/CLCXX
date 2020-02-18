cmake_minimum_required(VERSION 2.8.12)
project(clcxx)

# Dependencies
# ============
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})


# Settings
# ============
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_MODULE_PATH}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_VERBOSE_MAKEFILE 1)

# includes
# ============
include(CheckCXXCompilerFlag)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

# Compilation flags
# =================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-parameter -Wextra -Wreorder")
  CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

  if (HAS_CPP14_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  else()
    message(FATAL_ERROR "Unsupported compiler -- clcxx requires C++14 support!")
  endif()
endif()

if(WIN32 AND DEFINED ENV{MSYSTEM})
  if($ENV{MSYSTEM} STREQUAL "MINGW32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=pentium4 -static-libstdc++")
  endif()
endif()


# Liker flags
# =================
if(APPLE)
  set(D_LINER_FLAGS  "-undefined error" "-flat_namespace")
else()
  set(D_LINER_FLAGS  "-Wl,--no-undefined" "-Wl,--no-allow-shlib-undefined")
endif()

set(CMAKE_INSTALL_LIBDIR "lib")

# Source files
# ============

set(CLCXX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CLCXX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB HEADER_GLOB
  ${CLCXX_INCLUDE_DIR}/clcxx/*.hpp
  )

file(GLOB SOURCES_GLOB
  ${CLCXX_SOURCE_DIR}/*.cpp
  )

set(CLCXX_HEADERS
  ${HEADER_GLOB}
  )

set(CLCXX_SOURCES
  ${SOURCES_GLOB}
  )

# Versioning
# ==========

file(STRINGS "${CLCXX_INCLUDE_DIR}/clcxx/clcxx_config.hpp" clcxx_version_defines
  REGEX "#define CLCXX_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${clcxx_version_defines})
  if(ver MATCHES "#define CLCXX_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
    set(CLCXX_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
  endif()
endforeach()
set(${PROJECT_NAME}_VERSION
  ${CLCXX_VERSION_MAJOR}.${CLCXX_VERSION_MINOR}.${CLCXX_VERSION_PATCH})
message(STATUS "${PROJECT_NAME} version: v${${PROJECT_NAME}_VERSION}")

# Output
# ======

set(CLCXX_TARGET "${PROJECT_NAME}")
add_library(${CLCXX_TARGET} SHARED ${CLCXX_SOURCES} ${CLCXX_HEADERS})

target_include_directories(${CLCXX_TARGET} PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  $<BUILD_INTERFACE:${CLCXX_INCLUDE_DIR}>
  )

target_compile_options(${CLCXX_TARGET} PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:-std=c++14>"  "-Wall")

target_link_libraries(${CLCXX_TARGET}  ${D_LINER_FLAGS})
set_target_properties(${CLCXX_TARGET} PROPERTIES
  PUBLIC_HEADER "${CLCXX_HEADERS}"
  COMPILE_DEFINITIONS "CLCXX_EXPORTS")
set_property(TARGET ${CLCXX_TARGET} PROPERTY VERSION ${${PROJECT_NAME}_VERSION})
set_property(TARGET ${CLCXX_TARGET} PROPERTY SOVERSION ${CLCXX_VERSION_MAJOR})
set_property(TARGET ${CLCXX_TARGET} PROPERTY
  INTERFACE_${CLCXX_TARGET}_MAJOR_VERSION ${CLCXX_VERSION_MAJOR})
set_property(TARGET ${CLCXX_TARGET} APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING ${CLCXX_TARGET}_MAJOR_VERSION
  )

generate_export_header(${CLCXX_TARGET})

# Installation
# ============

# Use this snippet *after* PROJECT(xxx):
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "~/.local" CACHE PATH "install directory" FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

install(TARGETS ${CLCXX_TARGET} EXPORT ClCxxTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clcxx)

# Uninstall
# ==========
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()


