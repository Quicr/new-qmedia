#pragma once

#include <chrono>
#include <deque>
#include <thread>
#include <cmath>
#include "packet.hh"
#include "opus_assembler.hh"
#include "resampler.hh"
#include "qmedia/logger.hh"
#include "full_Fill.hh"
#include "metrics.hh"
#include "jitter_queues.hh"
#include "playout_tools.hh"
#include "playout_leakybucket.hh"
#include "playout_sync.hh"
#include "jitter_silence.hh"
#include "codec.hh"

namespace qmedia
{
class Jitter
{
public:
    Jitter(const LoggerPointer &parent_logger = nullptr,
           Metrics::MetricsPtr metricsPtr = nullptr);

    void set_audio_params(unsigned int audio_sample_rate,
                          unsigned int audio_channels,
                          Packet::MediaType audio_decode_type);
    void set_video_params(uint32_t video_max_width,
                          uint32_t video_max_height,
                          uint32_t video_decode_pixel_format);

    ~Jitter();

    bool push(PacketPointer packet);
    bool push(PacketPointer raw_packet,
              std::chrono::steady_clock::time_point now);
    int popVideo(uint64_t sourceID,
                 uint32_t &width,
                 uint32_t &height,
                 uint32_t &format,
                 uint64_t &timestamp,
                 unsigned char **buffer);
    int popVideo(uint64_t sourceID,
                 uint32_t &width,
                 uint32_t &height,
                 uint32_t &format,
                 uint64_t &timestamp,
                 unsigned char **buffer,
                 std::chrono::steady_clock::time_point now);

    PacketPointer popAudio(uint64_t sourceID, unsigned int length);
    PacketPointer popAudio(uint64_t sourceID,
                           unsigned int length,
                           std::chrono::steady_clock::time_point now);

    class Audio
    {
    public:
        void push(PacketPointer raw_packet,
                  uint64_t last_seq_popped,
                  std::chrono::steady_clock::time_point now);
        PacketPointer pop(std::chrono::steady_clock::time_point now);
        void insertAudioPLCs();
        PacketPointer createPLC(unsigned int size);
        PacketPointer createZeroPayload(unsigned int size);

        unsigned int getMsPerAudioPacket();
        unsigned int getMsInQueue();
        void pruneAudioQueue(std::chrono::steady_clock::time_point now,
                             unsigned int prune_target);
        unsigned int lostInQueue(unsigned int &numPlc,
                                 uint64_t last_seq_popped);
        unsigned int getFrameSize();

        MetaQueue mq;
        fullFill playout;        // using fullFill to achieve client asked
                                 // playout lengths
        uint64_t sourceID;
        PopFrequencyCounter fps;
        unsigned int ms_per_audio_packet;
        unsigned int audio_channels = 1;
        unsigned int audio_sample_rate = 48000;
        std::shared_ptr<OpusAssembler> opus_assembler;
        Silence silence;

    private:
        unsigned int getMsPerPacketInQueue();
    } audio;

    class Video
    {
    public:
        void push(PacketPointer raw_packet,
                  uint64_t last_seq_popped,
                  std::chrono::steady_clock::time_point now);
        PacketPointer pop(std::chrono::steady_clock::time_point now);
        MetaQueue mq;
        uint64_t sourceID = 0;
        PopFrequencyCounter fps;
        std::unique_ptr<VideoDecoder> decoder;
        std::vector<std::uint8_t> lastDecodedFrame;
        uint32_t last_decoded_width;
        uint32_t last_decoded_height;
        uint32_t last_decoded_format;
        uint64_t last_decoded_timestamp;
    } video;

    LeakyBucket bucket;
    JitterCalc audio_jitter;
    void QueueMonitor(std::chrono::steady_clock::time_point now);

private:
    Sync sync;
    Metrics::MetricsPtr metrics;
    bool idle_client;
    PacketPointer interpolatePlc(PacketPointer before, PacketPointer after);
    Resampler resampler;
    bool shutdown = false;
    const unsigned int maxStreams = 4;
    Packet::MediaType decode_audio_as;
    LoggerPointer logger;
    Metrics::MeasurementPtr measurement = nullptr;
    void recordMetrics(MetaQueue &q,
                       MetaQueue::media_type type,
                       uint64_t clientID,
                       uint64_t sourceID);
    int setDecodedFrame(uint64_t sourceID,
                        uint32_t &width,
                        uint32_t &height,
                        uint32_t &format,
                        uint64_t &timestamp,
                        unsigned char **buffer);
    void decodeVideoPacket(PacketPointer packet,
                           uint64_t sourceID,
                           uint32_t &width,
                           uint32_t &height,
                           uint32_t &format,
                           uint64_t &timestamp,
                           unsigned char **buffer,
                           std::chrono::steady_clock::time_point now);

    void idleClientPruneAudio(std::chrono::steady_clock::time_point now);
};

}        // namespace qmedia