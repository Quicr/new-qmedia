
cmake_minimum_required(VERSION 3.13)

project( new-qmedia VERSION 0.1 LANGUAGES C CXX)

option(BUILD_EXTERN "build external library" ON)
option(BUILD_TESTS  "build tests" ON)
option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)

# Build PIC dependencies when created extern shared library.
if (BUILD_EXTERN)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(BUILD_EXTERN)

add_subdirectory( dependencies )

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set(CMAKE_OSX_ARCHITECTURES "arm64")

if(CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY  ${CLANG_TIDY_EXE})
    else()
        message(WARNING "clang-tidy requested, but not found")
    endif()
endif()

###
### Dependencies
###

set(LIBRARIES media_api )


# Windows sockets.
if (WIN32)
    list(APPEND LIBRARIES Ws2_32)
endif(WIN32)


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
add_subdirectory(lib)
add_subdirectory(src)

###
### Applications
###
add_subdirectory(cmd)

###
### Tests
###
include(CTest)
if(BUILD_TESTING AND qmedia_BUILD_TESTS)
    add_subdirectory(test)
endif()
