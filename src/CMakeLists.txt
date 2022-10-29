add_library(qmedia
    audio_encoder.cc
    full_Fill.cc
    h264_decoder.cc
    h264_encoder.cc
    jitter.cc
    jitter_queues.cc
    jitter_silence.cc
    logger.cc
    media_client.cc
    media_stream.cc
    media_transport.cc
    metrics.cc
    opus_assembler.cc
    packet.cc
    playout_leakybucket.cc
    playout_sync.cc
    playout_tools.cc
    resampler.cc
    video_stream.cc
    metrics_reporter.cc)
target_link_directories(qmedia PUBLIC ${OpenH264_LIB_DIR})
target_link_libraries(qmedia
    PUBLIC
        ${LIBRARIES}
        OpenSSL::Crypto
        openh264
        metrics
        quicr)
target_include_directories(qmedia
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${Protobuf_INCLUDE_DIRS}
        ${OpenH264_INCLUDE_DIR})
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

