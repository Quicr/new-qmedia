#pragma once

#include <mutex>
#include <chrono>
#include <thread>
#include <functional>
#include <list>
#include <vector>

#include "opus.h"
#include "packet.hh"
#include "logger.hh"
#include "full_Fill.hh"
#include <ostream>
#include <iostream>
#include <queue>

namespace neo_media
{
class AudioEncoder
{
public:
    typedef std::function<void(PacketPointer packet)> frameReadyCallback;

    AudioEncoder(unsigned int audio_sample_rate,
                 int audio_channels,
                 Packet::MediaType audio_type,
                 frameReadyCallback callback,
                 uint64_t sourceID,
                 const LoggerPointer &logger = nullptr);
    ~AudioEncoder();
    void encodeFrame(const char *buffer,
                     unsigned int length,
                     std::uint64_t timestamp,
                     bool encodeEmpty);
    PacketPointer createPacket(const std::vector<uint8_t> &data,
                               std::uint64_t timestamp);

    std::thread encode_thread;
    unsigned int sample_rate;
    unsigned int output_samples;
    unsigned int output_bytes;

    int channels;
    uint64_t sID;
    frameReadyCallback frameReady = nullptr;
    LoggerPointer logger = nullptr;
    bool shutdown = false;
    void encoderWerk();
    void pop_and_encode();
    fullFill buffers;

    OpusEncoder *encoder;
    void initOpus();

    std::condition_variable cv;
    std::mutex enc_lock;
    bool immediate_encode = false;
    Packet::MediaType type;
};

}        // namespace neo_media
