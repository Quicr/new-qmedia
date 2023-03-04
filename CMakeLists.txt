
cmake_minimum_required(VERSION 3.13)

project(qmedia VERSION 0.1.1 LANGUAGES C CXX)


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(QMEDIA_BUILD_TESTS "Build tests for quicr" ON)
    option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON) 
else()
    option(QMEDIA_BUILD_TESTS "Build tests for quicr" OFF)
    option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" OFF) 
endif()


option(BUILD_EXTERN "build external library" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)

add_subdirectory(dependencies)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#set(CMAKE_OSX_ARCHITECTURES "arm64")
#CFLAG -target x86_64-apple-ios13.0-macabi.

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

add_subdirectory(src)

###
### Applications
###

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS)
  add_subdirectory(cmd)
endif()

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS)
  add_subdirectory(ctest)
endif()

###
### Tests
###
if(BUILD_TESTING AND QMEDIA_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()
