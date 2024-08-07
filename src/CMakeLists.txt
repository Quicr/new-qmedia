add_library(${PROJECT_NAME}
    SHARED
    ManifestTypes.cpp
    QController.cpp
    QuicrDelegates.cpp
    QSFrameContext.cpp
)

set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        quicr
        cantina::logger
        sframe
        nlohmann_json::nlohmann_json)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)

#add_subdirectory(extern)
