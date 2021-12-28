################################################################################
# Pull in dependency by using FetchContent
# https://cmake.org/cmake/help/v3.18/module/FetchContent.html
include(FetchContent)

################################################################################
# Project URL: https://github.com/cisco/sframe
#     Git URL: https://github.com/cisco/sframe.git
#      Branch: master
#     Targets: sframe
FetchContent_Declare(   sframe
    GIT_REPOSITORY      https://github.com/cisco/sframe.git
    GIT_TAG             master)
FetchContent_MakeAvailable(sframe)

################################################################################
# all of the "needs work" libraries are found in an OS specific zip file which
# includes the headers and the static libraries.  The use of the libraries may
# vary from library to library, and that logic should exist where the linking is
# done.
if(CMAKE_SYSTEM_NAME MATCHES "Windows") 
    FetchContent_Declare(project_libs
        URL http://gallia-rcdn-lab-dev1.cisco.com:9080/cto-media-binary-deps/05-windows.zip)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    FetchContent_Declare(project_libs
        URL http://gallia-rcdn-lab-dev1.cisco.com:9080/cto-media-binary-deps/05-darwin.zip)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    FetchContent_Declare(project_libs
        URL http://gallia-rcdn-lab-dev1.cisco.com:9080/cto-media-binary-deps/05-linux.zip)
else ()
    message(FATAL_ERROR "Unknown build configuration: ${CMAKE_BUILD_TYPE}  ${CMAKE_SYSTEM_NAME} ${CMAKE_GENERATOR_PLATFORM}")
endif()

FetchContent_MakeAvailable(project_libs)

if(NOT project_libs_POPULATED)
    FetchContent_Populate(project_libs)
endif(NOT project_libs_POPULATED)

if( CMAKE_BUILD_TYPE MATCHES "Release")
    set(PROJECT_LIBS_LIBRARY_DIR ${project_libs_SOURCE_DIR}/lib/Release)
else(CMAKE_BUILD_TYPE MATCHES "Release")
    set(PROJECT_LIBS_LIBRARY_DIR ${project_libs_SOURCE_DIR}/lib/Debug)
endif(CMAKE_BUILD_TYPE MATCHES "Release")

set(PROJECT_LIBS_INCLUDE_DIR ${project_libs_SOURCE_DIR}/include)
link_directories(${PROJECT_LIBS_LIBRARY_DIR})
include_directories(${PROJECT_LIBS_INCLUDE_DIR})

message(STATUS "PROJECT_LIBS_LIBRARY_DIR: ${PROJECT_LIBS_LIBRARY_DIR}")
message(STATUS "PROJECT_LIBS_INCLUDE_DIR: ${PROJECT_LIBS_INCLUDE_DIR}")
