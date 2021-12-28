################################################################################
# project
project(media_api)

################################################################################
# generate a list of proto definitions
set(PROTOCOL_DEFINITIONS media)

foreach(file IN LISTS PROTOCOL_DEFINITIONS)
    list(APPEND protocol_files ${CMAKE_CURRENT_SOURCE_DIR}/${file}.proto)
endforeach()

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
protobuf_generate_cpp(library_sources library_headers ${protocol_files})

################################################################################
# build
add_library(${PROJECT_NAME} STATIC ${library_sources} ${library_headers})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${Protobuf_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Protobuf_LIBRARIES})
