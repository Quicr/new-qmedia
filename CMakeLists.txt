
cmake_minimum_required(VERSION 3.13)

project(qmedia VERSION 0.1.1 LANGUAGES C CXX)

option(BUILD_EXTERN "build external library" ON)
option(BUILD_TESTS  "build tests" OFF)
option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON)
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
#set(LIBRARIES media_api )
#add_subdirectory(lib)

add_subdirectory(src)

###
### Applications
###

#add_subdirectory(cmd)

###
### Tests
###
include(CTest)
if(BUILD_TESTING AND qmedia_BUILD_TESTS)
#    add_subdirectory(test)
endif()

add_subdirectory(ctest)
