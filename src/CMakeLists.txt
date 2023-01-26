add_library(qmedia
#    full_Fill.cc
#    jitter.cc
#    jitter_queues.cc
#    jitter_silence.cc
    logger.cc
    media_client.cc
#    metrics.cc
#    playout_leakybucket.cc
#    playout_sync.cc
#    playout_tools.cc
#    resampler.cc
#    metrics_reporter.cc)
)
target_link_directories(qmedia PUBLIC ${OpenH264_LIB_DIR})
target_link_libraries(qmedia
    PUBLIC
        ${LIBRARIES}
        quicr)
target_include_directories(qmedia
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include)
target_compile_options(qmedia
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)
set_target_properties(qmedia
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)

if(BUILD_EXTERN)
    add_subdirectory(extern)
endif(BUILD_EXTERN)

