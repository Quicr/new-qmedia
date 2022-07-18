#pragma once

#include <string>
#include <functional>
#include <cstring>
#include <iostream>
#include <atomic>

#include "../src/media_transport.hh"

#include "../src/transport_manager.hh"
#include "qmedia/metrics.hh"
// todo: need a single class for handling incoming packet with and without
// jitter
#include "../src/jitter.hh"
#include "../src/jitter_interface.hh"
#include "../src/loopback-jitter.hh"
#include "simple_packetize.hh"

// todo: refactor all codec dependencies out of main interface
#include "../src/codec.hh"
#include "opus.h"
#include "../src/audio_encoder.hh"
#include "qmedia/logger.hh"
#include "qmedia/metrics.hh"

namespace neo_media
{
class Neo
{
public:
    enum struct audio_sample_type
    {
        Float32,
        PCMint16
    };
    enum struct video_pixel_format
    {
        NV12,
        I420
    };

    enum struct MediaDirection
    {
        publish_only = 1,         // sendonly
        subscribe_only,           // recvonly
        publish_subscribe,        // sendrecv
        unknown
    };

    // Enable testing via various short-circuit
    enum struct LoopbackMode
    {
        none,              // no short-circuit
        mirror,            // raw return
        codec,             // return after codec/decode
        full_media,        // return after enc, packetize, depacketize, dejiter,
                           // decode
        transport          // return from transport
    };

    using callbackSourceId = std::function<void(const uint64_t,
                                                const uint64_t,
                                                const uint64_t,
                                                const Packet::MediaType)>;

    explicit Neo(const LoggerPointer &parent_logger = nullptr);

    void init(const std::string &remote_address,
              unsigned int remote_port,
              unsigned int audio_sample_rate,
              unsigned int audio_channels,
              audio_sample_type type,
              unsigned int video_max_width,
              unsigned int video_max_height,
              unsigned int video_max_frame_rate,
              unsigned int video_max_bitrate,
              video_pixel_format video_encode_pixel_format,
              video_pixel_format video_decode_pixel_format,
              uint64_t clientID,
              uint64_t conferenceID,
              callbackSourceId callback,
              NetTransport::Type transport_type,
              MediaDirection direction,
              bool echo);

    std::atomic<bool> mutedAudioEmptyFrames = false;        // keyframe request
                                                            // in progress.
    void setMicrophoneMute(bool muted);

    // Salt-N-Peppa push
    void sendAudio(const char *buffer,
                   unsigned int length,
                   uint64_t timestamp,
                   uint64_t sourceID);
    void sendVideoFrame(const char *buffer,
                        uint32_t length,
                        uint32_t width,
                        uint32_t height,
                        uint32_t stride_y,
                        uint32_t stride_uv,
                        uint32_t offset_u,
                        uint32_t offset_v,
                        uint32_t format,
                        uint64_t timestamp,
                        uint64_t sourceID);

    // returns actual bytes filled
    int getAudio(uint64_t clientID,
                 uint64_t sourceID,
                 uint64_t &timestamp,
                 unsigned char **buffer,
                 unsigned int max_len,
                 Packet **packetToFree);
    std::uint32_t getVideoFrame(uint64_t clientID,
                                uint64_t sourceID,
                                uint64_t &timestamp,
                                uint32_t &width,
                                uint32_t &height,
                                uint32_t &format,
                                unsigned char **buffer);

    void audioEncoderCallback(PacketPointer packet);

    // Experimental/Test APIs
    void setLoopbackMode(uint8_t mode)
    {
        std::clog << "Loopback mode is " << mode << std::endl;
        loopbackMode = (LoopbackMode) mode;
    };

    // Quicr APIs
    void publish(uint64_t source_id,
                 Packet::MediaType media_type,
                 std::string url);
    void subscribe(uint64_t source_id,
                   Packet::MediaType mediaType,
                   std::string url);
    void start_transport(NetTransport::Type transport_type);

protected:
    void doWork();
    std::thread workThread;
    bool shutdown = false;

    static int neoWorkThread(Neo *neo)
    {
        assert(neo);
        neo->doWork();
        return 0;
    }

private:
    std::unique_ptr<ClientTransportManager> transport;

    const unsigned int maxJitters = 2;
    std::map<uint64_t, std::shared_ptr<JitterInterface>> jitters;
    std::shared_ptr<JitterInterface> getJitter(uint64_t clientID);
    std::shared_ptr<JitterInterface> createJitter(uint64_t clientID);
    std::map<uint64_t, std::shared_ptr<AudioEncoder>> audio_encoders;
    std::shared_ptr<AudioEncoder> getAudioEncoder(uint64_t streamID);

    uint64_t myClientID;
    uint64_t myConferenceID;
    uint64_t video_seq_no = 0;
    uint64_t seq_no = 0;

    bool firstPacket = true;
    callbackSourceId newSources;        // client callback upon new stream
    LoggerPointer log;

    /// Audio parameters and helpers
    unsigned int audio_sample_rate;
    unsigned int audio_channels;
    audio_sample_type type;

    /// Video parameters and helpers
    void encodeVideoFrame(const char *buffer,
                          uint32_t length,
                          uint32_t width,
                          uint32_t height,
                          uint32_t stride_y,
                          uint32_t stride_uv,
                          uint32_t offset_u,
                          uint32_t offset_v,
                          uint32_t format,
                          uint64_t timestamp,
                          Packet *packet);
    void decodeVideoFrame(Packet *packet,
                          uint32_t &width,
                          uint32_t &height,
                          uint32_t &format,
                          std::vector<std::uint8_t> &output_frame);
    unsigned int video_max_width;
    unsigned int video_max_height;
    unsigned int video_max_frame_rate;
    unsigned int video_max_bitrate;
    video_pixel_format video_encode_pixel_format;
    video_pixel_format video_decode_pixel_format;
    std::unique_ptr<VideoEncoder> video_encoder;
    std::atomic<bool> reqKeyFrame = false;        // keyframe request in
                                                  // progress.

    LoopbackMode loopbackMode = LoopbackMode::none;
    Metrics::MetricsPtr metrics = nullptr;
    NetTransport::Type transport_type;
    MediaDirection media_dir = MediaDirection::unknown;

    // quicr transport interface

};        // class Neo

typedef std::shared_ptr<Neo> NeoPointer;

}        // namespace neo_media
