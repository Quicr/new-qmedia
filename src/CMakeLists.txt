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
        ${LIBRARIES}
        quicr
        cantina::logger
        numero_uri_lib
        sframe)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)

if (USE_MBEDTLS)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            USE_MBEDTLS)
    target_sources(${PROJECT_NAME} PRIVATE mbedtls.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE mbedtls)
else()
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            sframe_openssl)
endif()
#add_subdirectory(extern)
