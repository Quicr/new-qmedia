#pragma once

#include <mutex>
#include <chrono>
#include <thread>
#include <functional>
#include <list>
#include <vector>

#include <qmedia/logger.hh>
#include <qmedia/media_client.hh>

#include "opus.h"
#include "packet.hh"
#include "full_Fill.hh"
#include <ostream>
#include <iostream>
#include <queue>

namespace qmedia
{

struct AudioEncoder
{
public:
    typedef std::function<void(std::vector<uint8_t> &&)> frameReadyCallback;

    AudioEncoder(unsigned int audio_sample_rate,
                 int audio_channels,
                 AudioConfig::SampleType audio_type,
                 frameReadyCallback callback,
                 MediaStreamId stream,
                 const LoggerPointer &logger = nullptr);
    ~AudioEncoder();

    void encodeFrame(const uint8_t *buffer,
                     unsigned int length,
                     std::uint64_t timestamp,
                     bool encodeEmpty);

private:
    std::vector<uint8_t> encode(const std::vector<uint8_t> &data);

    void encoderWerk();
    void pop_and_encode();
    void initOpus();

    // opus
    OpusEncoder *encoder;
    std::condition_variable cv;
    std::mutex enc_lock;
    bool immediate_encode = false;
    std::thread encode_thread;

    unsigned int sample_rate;
    unsigned int output_samples;
    unsigned int output_bytes;
    int channels;
    uint64_t stream_id;
    frameReadyCallback encoded_frame_ready = nullptr;
    LoggerPointer logger = nullptr;
    bool shutdown = false;
    // matches input rate to encoded rate
    fullFill buffers;
    AudioConfig::SampleType type;
};

}        // namespace qmedia
