add_library(${PROJECT_NAME}
    SHARED
    basicLogger.cpp
    QController.cpp
    QuicrDelegates.cpp
    QSFrameContext.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBRARIES}
        quicr
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


