
cmake_minimum_required(VERSION 3.13)

#set(CMAKE_TOOLCHAIN_FILE
#        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
#        CACHE STRING "Vcpkg toolchain file")
#if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
#    message(WARNING
#            "Missing vcpkg toolchain! Do you forget to initialize the vcpkg "
#            "submodule?")
#endif(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})

project(qmedia VERSION 0.1 LANGUAGES C CXX)

# submodules
#find_package(Git QUIET)
#if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
## Update submodules as needed
#    option(GIT_SUBMODULE "Check submodules during build" ON)
#    if(GIT_SUBMODULE)
#        message(STATUS "Submodule update")
#        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
#                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
#        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
#            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
#        endif()
#    endif()
#endif()

option(BUILD_EXTERN "build external library" ON)
option(BUILD_TESTS  "build tests" ON)
option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)

# Build PIC dependencies when created extern shared library.
if (BUILD_EXTERN)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(BUILD_EXTERN)

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

#set(LIBRARIES media_api gsl sframe)
#set(LIBRARIES gsl sframe)

# samplerate
#find_library(SAMPLE_RATE samplerate REQUIRED)
#list(APPEND LIBRARIES ${SAMPLE_RATE})

# curl
#find_package(CURL REQUIRED)
#list(APPEND LIBRARIES ${CURL_LIBRARIES})

# opus
##if (APPLE)
#    find_package(PkgConfig REQUIRED)
#    pkg_check_modules(opus REQUIRED IMPORTED_TARGET opus)
#    list(APPEND LIBRARIES PkgConfig::opus)
#else(APPLE)
#    find_package(Opus REQUIRED)
#    list(APPEND LIBRARIES Opus::opus)
#endif(APPLE)

#add_subdirectory( proto )
#find_package(Opus REQUIRED)


#find_package(OpenSSL 1.1 REQUIRED)

# custom openh264 - need a better way to support
# multiple platforms

###
### Library Source
###
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
    add_subdirectory(test)
endif()

add_subdirectory(ctest)
