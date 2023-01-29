
cmake_minimum_required(VERSION 3.13)

project(qmedia VERSION 0.1.1 LANGUAGES C CXX)


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(QMEDIA_BUILD_TESTS "Build tests for quicr" OFF)
    option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" OFF) 
else()
    option(QUICR QMEDIA_BUILD_TESTS "Build tests for quicr" ON)
    option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON) 
endif()


option(BUILD_EXTERN "build external library" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)

add_subdirectory(dependencies)

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

#add_subdirectory(lib)

add_subdirectory(src)

###
### Applications
###

add_subdirectory(cmd)

###
### Tests
###
include(CTest)
if(BUILD_TESTING AND QMEDIA_BUILD_TESTS)
      #find_package(doctest QUIET)
     add_subdirectory(test)
endif()

add_subdirectory(ctest)
