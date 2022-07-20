#pragma once
#include <memory>
#include <map>
#include <optional>
#include <functional>

#include <qmedia/media_client.hh>
#include "media_transport.hh"

#include "audio_encoder.hh"
#include "codec.hh"
#include "names.hh"
#include "jitter.hh"
#include "metrics.hh"

namespace qmedia
{

// Captures a stream of media and has associated QuicR name
struct MediaStream
{
public:
    explicit MediaStream(uint64_t domain_in,
                         uint64_t conference_id_in,
                         uint64_t client_id_in,
                         const MediaConfig &config_in,
                         LoggerPointer logger_in) :
        domain(domain_in),
        conference_id(conference_id_in),
        client_id(client_id_in),
        config(config_in),
        logger(logger_in)
    {
    }

    virtual ~MediaStream() = default;

    void set_transport(std::shared_ptr<MediaTransport> transport)
    {
        media_transport = transport;
    }
    void handle_media(uint64_t group_id,
                      uint64_t object_id,
                      std::vector<uint8_t> &&bytes);
    virtual MediaStreamId id() = 0;
    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) = 0;
    virtual size_t get_media(uint64_t &timestamp,
                             MediaConfig &config,
                             unsigned char **buffer) = 0;

protected:
    // jitter helpers
    const unsigned int maxJitters = 2;
    std::map<uint64_t, std::shared_ptr<Jitter>> jitters;
    std::shared_ptr<Jitter> getJitter(uint64_t client_id);
    std::shared_ptr<Jitter> createJitter(uint64_t clientID,
                                         const MediaConfig &config);

    std::atomic<bool> mutedAudioEmptyFrames = false;

    uint64_t domain = 0;
    uint64_t conference_id = 0;
    uint64_t client_id = 0;
    MediaStreamId media_stream_id = 0;
    MediaConfig config;
    MediaConfig::MediaDirection media_direction;
    std::shared_ptr<MediaTransport> media_transport;
    LoggerPointer logger;
    Metrics::MetricsPtr metrics = nullptr;
};

struct AudioStream : public MediaStream
{
    explicit AudioStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         const MediaConfig &media_config,
                         LoggerPointer logger_in);
    ~AudioStream() = default;
    void configure();
    virtual MediaStreamId id() override;
    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) override;
    virtual size_t get_media(uint64_t &timestamp,
                             MediaConfig &config,
                             unsigned char **buffer) override;

private:
    std::shared_ptr<AudioEncoder> setupAudioEncoder();
    void audio_encoder_callback(std::vector<uint8_t> &&bytes);
    std::shared_ptr<AudioEncoder> encoder = nullptr;
    uint64_t encode_sequence_num = 0;
};

struct VideoStream : public MediaStream
{
    explicit VideoStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         const MediaConfig &media_config,
                         LoggerPointer logger_in);
    ~VideoStream() = default;

    void configure();

    virtual MediaStreamId id() override;
    // media apis
    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) override;

    virtual size_t get_media(uint64_t &timestamp,
                             MediaConfig &config,
                             unsigned char **buffer) override;

private:
    PacketPointer encode_h264(uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config);

    std::unique_ptr<VideoEncoder> encoder = nullptr;
    uint64_t encode_sequence_num = 0;
};

// MediaStream Factory
struct MediaStreamFactory
{
    static std::shared_ptr<MediaStream>
    create_audio_stream(uint64_t domain,
                        uint64_t conference_id,
                        uint64_t client_id,
                        const MediaConfig &media_config,
                        LoggerPointer logger_in)
    {
        auto stream = std::make_shared<AudioStream>(
            domain, conference_id, client_id, media_config, logger_in);
        stream->configure();
        return stream;
    }

    static std::shared_ptr<MediaStream>
    create_video_stream(uint64_t domain,
                        uint64_t conference_id,
                        uint64_t client_id,
                        const MediaConfig &media_config,
                        LoggerPointer logger)
    {
        auto stream = std::make_shared<VideoStream>(
            domain, conference_id, client_id, media_config, logger);
        stream->configure();
        return stream;
    }
};

}        // namespace qmedia
