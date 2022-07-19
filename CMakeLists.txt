
cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
        CACHE STRING "Vcpkg toolchain file")
if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
    message(WARNING
            "Missing vcpkg toolchain! Do you forget to initialize the vcpkg "
            "submodule?")
endif(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})

project(qmedia VERSION 0.1 LANGUAGES C CXX)

if("${CMAKE_GENERATOR}" MATCHES "Makefiles" AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if("${CMAKE_GENERATOR}" MATCHES "Makefiles")
    set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
endif()

option(BUILD_EXTERN "build external library" OFF)
option(BUILD_TESTS  "build tests" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)

# Build PIC dependencies when created extern shared library.
if (BUILD_EXTERN)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(BUILD_EXTERN)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# clang & gcc
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-pedantic -Wextra)
elseif(MSVC)
    #add_compile_options(/W4)
endif()

if(CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY  ${CLANG_TIDY_EXE})
    else()
        message(WARNING "clang-tidy requested, but not found")
    endif()
endif()



# include_directories( include src/extern )
include_directories( include )


###
### Dependencies
###
add_subdirectory(contrib/quicr)
add_subdirectory(contrib/sframe)

include_directories(include lib tests ${OPENSSL_INCLUDE_DIR})

set(LIBRARIES media_api gsl sframe quicr)

# samplerate
find_library(SAMPLE_RATE samplerate REQUIRED)
list(APPEND LIBRARIES ${SAMPLE_RATE})

# curl
find_package(CURL REQUIRED)
list(APPEND LIBRARIES ${CURL_LIBRARIES})

# opus
if (APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(opus REQUIRED IMPORTED_TARGET opus)
    list(APPEND LIBRARIES PkgConfig::opus)
else(APPLE)
    find_package(Opus REQUIRED)
    list(APPEND LIBRARIES Opus::opus)
endif(APPLE)

add_subdirectory( proto )
find_package(Opus REQUIRED)

# Windows sockets.
if (WIN32)
    list(APPEND LIBRARIES Ws2_32)
endif(WIN32)

find_package(OpenSSL 1.1 REQUIRED)

# custom openh264 - need a better way to support
# multiple platforms
if (APPLE)
     execute_process(
      COMMAND uname -m
      RESULT_VARIABLE result
      OUTPUT_VARIABLE OSX_NATIVE_ARCHITECTURE
      OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(OSX_NATIVE_ARCHITECTURE STREQUAL "arm64")
      set(OpenH264_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contrib/openh264/include)
      set(OpenH264_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contrib/openh264/lib/arm64)
  else()
      set(OpenH264_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contrib/openh264/include)
      set(OpenH264_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contrib/openh264/lib/x86_64)
  endif()

  message(STATUS "openh64: ${OpenH264_LIB_DIR}")
  message(STATUS "openh64-inc ${OpenH264_INCLUDE_DIR}")
endif()

###
### Library Source
###
# add_subdirectory(lib)
add_subdirectory(src)

###
### Library Config
###

set(LIB_NAME "${PROJECT_NAME}")

file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_library(${LIB_NAME} ${LIB_HEADERS} ${LIB_SOURCES})
target_link_directories(${LIB_NAME} PUBLIC ${PROJECT_LIBS_LIBRARY_DIR} ${OpenH264_LIB_DIR})
target_link_libraries(${LIB_NAME} ${LIBRARIES} OpenSSL::Crypto openh264)
target_include_directories(${LIB_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
        ${OpenH264_INCLUDE_DIR}
        $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
        )


###
### Applications
###

#add_subdirectory(cmd)

###
### Tests
###
if(BUILD_TESTS)
   # add_subdirectory(test)
endif(BUILD_TESTS)
