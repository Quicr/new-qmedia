project(neo_media_client)

set_property(TARGET qmedia PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET quicr PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET quicr-transport PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library( ${PROJECT_NAME} SHARED neo_media_client.cc)

target_include_directories( ${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include)
        
target_link_libraries(${PROJECT_NAME} PRIVATE qmedia)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)
