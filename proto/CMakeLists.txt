################################################################################
# project
project(media_api)

################################################################################
# protoc is not found by the initial cache generation of cmake, so we need to
# give it a hint.  Assuming that we are using vcpkg.
find_program(
    Protobuf_PROTOC_EXECUTABLE protoc
    PATHS ${CMAKE_CURRENT_BINARY_DIR}/../vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf
    REQUIRED)

################################################################################
# find the Protobuf dependencies
find_package(Protobuf REQUIRED)
set(Protobuf_DEBUG ON)

################################################################################
# compile protocol definitions
protobuf_generate_cpp(library_sources library_headers media.proto)

################################################################################
# build
add_library(media_api STATIC ${library_sources} ${library_headers})
target_include_directories(media_api PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${Protobuf_INCLUDE_DIRS})
target_link_libraries(media_api ${Protobuf_LIBRARIES})
target_compile_options(media_api
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)
set_target_properties(media_api
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)

