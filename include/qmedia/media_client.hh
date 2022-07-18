#pragma once

#include <string>
#include <functional>
#include <cstring>
#include <iostream>
#include <map>

#include "callback.hh"
#include "logger.hh"

namespace qmedia
{

enum struct TransportType
{
    QUIC = 0        // configure quicr-lib with QUIC transport
};

enum MediaType
{
    audio = 0,
    video = 1,
};

struct AudioConfig
{
    enum struct SampleType
    {
        Float32,
        PCMint16
    };
    SampleType sample_type;
    unsigned int sample_rate;
    unsigned int channels;
};

struct VideoConfig
{
    enum struct PixelFormat
    {
        NV12,
        I420
    };

    unsigned int video_max_width;
    unsigned int video_max_height;
    unsigned int video_max_frame_rate;
    unsigned int video_max_bitrate;
    PixelFormat video_encode_pixel_format;
    PixelFormat video_decode_pixel_format;
    uint32_t stride_y;
    uint32_t stride_uv;
    uint32_t offset_u;
    uint32_t offset_v;
};

struct MediaConfig : public AudioConfig, public VideoConfig
{
    enum struct MediaDirection
    {
        sendonly = 0,
        recvonly,
        sendrecv,
        unknown
    };

    enum struct CodecType
    {
        opus = 0,
        h264,
        raw
    };

    MediaDirection media_direction;
    CodecType media_codec;
};

// handy typedefs
using MediaStreamId = uint64_t;

struct MediaStream;

class MediaClient
{
public:
    // Report new remote source
    using callbackSourceId = std::function<
        void(const uint64_t, const uint64_t, const uint64_t, const MediaType)>;

    explicit MediaClient(const LoggerPointer &parent_logger = nullptr);

    // configure transport for this client
    void init_transport(TransportType transport_type,
                        const std::string &remote_address,
                        unsigned int remote_port);

    // Stream API
    MediaStreamId add_audio_stream(uint64_t domain,
                                   uint64_t conference_id,
                                   uint64_t client_id,
                                   const AudioConfig &media_config);

    MediaStreamId add_video_stream(uint64_t domain,
                                   uint64_t conference_id,
                                   uint64_t client_id,
                                   const VideoConfig &media_config);

    void remove_media_stream(MediaStreamId media_stream_id);

    // media apis
    void send_audio(MediaStreamId streamId,
                    uint8_t *buffer,
                    unsigned int length,
                    uint64_t timestamp);

    void send_video(MediaStreamId streamId,
                    uint8_t *buffer,
                    uint32_t length,
                    uint32_t width,
                    uint32_t height,
                    uint32_t stride_y,
                    uint32_t stride_uv,
                    uint32_t offset_u,
                    uint32_t offset_v,
                    uint32_t format,
                    uint64_t timestamp);

    // returns actual bytes filled
    int get_audio(MediaStreamId streamId,
                  uint64_t &timestamp,
                  unsigned char **buffer,
                  unsigned int max_len,
                  Packet **packetToFree);

    std::uint32_t get_video(uint64_t clientID,
                            uint64_t sourceID,
                            uint64_t &timestamp,
                            uint32_t &width,
                            uint32_t &height,
                            uint32_t &format,
                            unsigned char **buffer);

private:
    void do_work();
    std::thread work_thread;
    bool shutdown = false;

    static int start_work_thread(MediaClient *media_client)
    {
        assert(media_client);
        media_client->do_work();
        return 0;
    }

    bool firstPacket = true;
    callbackSourceId newSources;
    LoggerPointer log;

    // list of media streams
    std::map<MediaStreamId, std::shared_ptr<MediaStream>> active_streams;

    // underlying media transport
    struct MediaTransport;
    std::shared_ptr<MediaTransport> media_transport;
};

}        // namespace qmedia
