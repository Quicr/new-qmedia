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
                         LoggerPointer logger_in) :
        domain(domain_in),
        conference_id(conference_id_in),
        client_id(client_id_in),
        logger(logger_in)
    {
    }

    virtual ~MediaStream() = default;

    void set_transport(std::shared_ptr<MediaTransport> transport)
    {
        media_transport = transport;
    }

    virtual MediaStreamId id() = 0;

    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) = 0;

    virtual void handle_media(uint64_t group_id,
                              uint64_t object_id,
                              std::vector<uint8_t> &&bytes) = 0;

protected:
    MediaConfig::MediaDirection media_direction;
    std::shared_ptr<MediaTransport> media_transport;
    LoggerPointer logger;
    Metrics::MetricsPtr metrics = nullptr;

    std::atomic<bool> mutedAudioEmptyFrames = false;

    uint64_t domain = 0;
    uint64_t conference_id = 0;
    uint64_t client_id = 0;
    MediaStreamId media_stream_id = 0;
    uint64_t encode_sequence_num = 0;
};

struct AudioStream : public MediaStream
{
    explicit AudioStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         LoggerPointer logger_in);
    ~AudioStream() = default;
    void set_config(const MediaConfig &config);

    virtual MediaStreamId id() override;
    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) override;
    virtual void handle_media(uint64_t group_id,
                              uint64_t object_id,
                              std::vector<uint8_t> &&bytes) override;

private:
    std::shared_ptr<AudioEncoder> setupAudioEncoder();
    void audio_encoder_callback(std::vector<uint8_t> &&bytes);
    // jitter helpers
    std::shared_ptr<Jitter> getJitter(uint64_t client_id);
    std::shared_ptr<Jitter> createJitter(uint64_t clientID);

    // 1;1 mapping bteween stream and the encoder
    std::shared_ptr<AudioEncoder> encoder = nullptr;
    MediaConfig config;
    const unsigned int maxJitters = 2;
    std::map<uint64_t, std::shared_ptr<Jitter>> jitters;
};

struct VideoStream : public MediaStream
{
    explicit VideoStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         LoggerPointer logger_in);
    ~VideoStream() = default;

    virtual MediaStreamId id() override;
    // media apis
    // set_config has to be invoked before calling this api
    // to ensure the latest configuration
    virtual void handle_media(MediaConfig::CodecType codec_type,
                              uint8_t *buffer,
                              unsigned int length,
                              uint64_t timestamp,
                              const MediaConfig &media_config) override;
    virtual void handle_media(uint64_t group_id,
                              uint64_t object_id,
                              std::vector<uint8_t> &&bytes) override;

    void set_config(const MediaConfig &config);

private:
    std::vector<uint8_t> encode_h264(uint8_t *buffer,
                                     unsigned int length,
                                     uint64_t timestamp,
                                     const MediaConfig &media_config);

    std::unique_ptr<VideoEncoder> encoder = nullptr;
    MediaConfig config;
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
            domain, conference_id, client_id, logger_in);
        stream->set_config(media_config);
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
            domain, conference_id, client_id, logger);
        stream->set_config(media_config);
        return stream;
    }
};

}        // namespace qmedia
