add_library(${PROJECT_NAME}
    SHARED
    namespace_config.cpp
    counter.cpp
    delivery.cpp
    mls_client.cpp
    mls_session.cpp
    ManifestTypes.cpp
    QController.cpp
    QuicrDelegates.cpp
    QCrypto.cpp
)

set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
target_compile_options(picotls-core PRIVATE -w)
target_compile_options(picotls-openssl PRIVATE -w)
target_compile_options(picoquic-core PRIVATE -w)
target_compile_options(quicr-transport PRIVATE -w)

set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)
find_package(OpenSSL 1.1 REQUIRED)
find_package(MLSPP REQUIRED)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBRARIES}
        quicr
        MLSPP::mlspp
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

#add_subdirectory(extern)
