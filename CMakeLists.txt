################################################################################
# set include path for cmake functionality
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

################################################################################
# minimum cmake version - if your CMake version is less that this, then
# configuration will fail.
cmake_minimum_required(VERSION 3.10)

################################################################################
# we have vcpkg installed as a submodule, and this will ensure that we install
# the dependencies that we need without much knowledge that vcpkg actually
# exists.
set(CMAKE_TOOLCHAIN_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "Vcpkg toolchain file")
if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
    message(WARNING
        "Missing vcpkg toolchain! Do you forget to initialize the vcpkg "
        "submodule?")
endif(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})

################################################################################
# top-level project name, version, and languages - this project will have
# several projects, but this is declared as the top level project.
project(neoMedia VERSION 0.1 LANGUAGES C CXX)

################################################################################
# If this is a Makefile generator and the build type was not defined, then the
# project should default to a debug build. Also sets the generated
# configurations to match the build type.
if("${CMAKE_GENERATOR}" MATCHES "Makefiles" AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if("${CMAKE_GENERATOR}" MATCHES "Makefiles")
    set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
endif()

################################################################################
# Options are public build variables that will be surfaced to a CMake GUI if
# you might using one.
option(BUILD_EXTERN "build external library" ON)
option(BUILD_TESTS  "build tests" ON)
option(BUILD_SEND_VIDEO_FRAME "build sendVideoFrame cmd" ON)
option(CLANG_TIDY "Perform linting with clang-tidy" OFF)
option(BUILD_WITH_QUIC "Build with quic transport" OFF)
option(BUILD_WITH_QUICR "Build with quicr transport" ON)

################################################################################
# dependencies
include(dependencies)

# Build PIC dependencies when created extern shared library.
if (BUILD_EXTERN)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(BUILD_EXTERN)

###
### Global Config
###
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# update the general compiler configuration
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



################################################################################
# subdirectories - if we want to build tests or anything else, we could add the
# subdirectories here.  Generally, the subdirectories on this level would be
# include, contrib, src, and tests.

include_directories( include src/extern )


###
### Dependencies
###

find_package(Picoquic REQUIRED)
message(STATUS "Picoquic/include: ${Picoquic_INCLUDE_DIRS}" )
message(STATUS "Picoquic library: ${Picoquic_LIBRARIES}" )

find_package(PTLS REQUIRED)
message(STATUS "picotls/include: ${PTLS_INCLUDE_DIRS}" )
message(STATUS "picotls libraries: ${PTLS_LIBRARIES}" )

find_package(Quicr REQUIRED)
message(STATUS "quicr/include: ${QUICR_INCLUDE_DIRS}" )
message(STATUS "quicr libraries: ${QUICR_LIBRARIES}" )

include_directories(include lib tests
        ${QUICR_INCLUDE_DIRS} ${Picoquic_INCLUDE_DIRS} ${PTLS_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

set(LIBRARIES media_api gsl sframe)

# samplerate
find_library(SAMPLE_RATE samplerate REQUIRED)
list(APPEND LIBRARIES ${SAMPLE_RATE})

# openh264
find_library(openh264 openh264 REQUIRED)
list(APPEND LIBRARIES ${openh264})

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



###
### Library Source
###

add_subdirectory(src)

###
### Library Config
###

set(LIB_NAME "${PROJECT_NAME}")

file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_library(${LIB_NAME} ${LIB_HEADERS} ${LIB_SOURCES})
target_link_directories(${LIB_NAME} PUBLIC ${PROJECT_LIBS_LIBRARY_DIR})
target_link_libraries(${LIB_NAME} ${LIBRARIES} OpenSSL::Crypto)
target_include_directories(${LIB_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
        $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
        )

if(BUILD_WITH_QUIC)
    # picoquic and friends
    if (WIN32)
        target_link_libraries(${LIB_NAME} picotls-openssl picotls-core picotls-fusion picoquic)
    else(WIN32)
        target_link_libraries(${LIB_NAME} picotls-openssl picotls-core picotls-fusion picoquic-core)
    endif(WIN32)
    add_compile_definitions(${LIB_NAME} ENABLE_QUIC)
endif()

if(BUILD_WITH_QUICR)
    message(STATUS "PICO = ${Picoquic_LIBRARIES}")
    message(STATUS "PICO_TLS = ${PTLS_LIBRARIES}")
    # picoquic and friends
    if (WIN32)
        target_link_libraries(${LIB_NAME} ${QUICR_LIBRARIES} ${Picoquic_LIBRARIES} ${PTLS_LIBRARIES})
    else(WIN32)
        target_link_libraries(${LIB_NAME} ${QUICR_LIBRARIES} ${Picoquic_LIBRARIES} ${PTLS_LIBRARIES})
    endif(WIN32)
    add_compile_definitions(${LIB_NAME} ENABLE_QUICR)
endif()


###
### Applications
###

add_subdirectory(cmd)

###
### Tests
###
if(BUILD_TESTS)
    add_subdirectory(test)
endif(BUILD_TESTS)
