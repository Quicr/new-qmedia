#include "neo_media_client.hh"
#include <qmedia/media_client.hh>

using namespace qmedia;

extern "C"
{
    void CALL MediaClient_Create(ExternalLogCallback log_callback,
                                 NewStreamCallback stream_callback,
                                 const char *remote_address,
                                 std::uint16_t remote_port,
                                 void **media_client)
    {
        if (!media_client || !remote_address)
        {
            return;
        }

        // Bridge to external logging.
        LoggerPointer logger = std::make_shared<Logger>("QMediaExtern");
        logger->SetLogFacility(LogFacility::NOTIFY);
        auto logCallback =
            [log_callback](LogLevel /*level*/, const std::string &message)
        { log_callback(message.c_str()); };
        logger->SetLogCallback(logCallback);

        // configure new stream callback
        auto wrapped_stream_callback = [stream_callback](uint64_t client_id,
                                                         uint64_t source_id,
                                                         uint64_t source_ts,
                                                         MediaType media_type)
        { stream_callback(client_id, source_id, source_ts, (int) media_type); };

        // Create media library.
        auto client = std::make_unique<MediaClient>(wrapped_stream_callback,
                                                    logger);
        client->init_transport(
            TransportType::QUIC, std::string(remote_address), remote_port);
        *media_client = client.release();
    }

    void CALL MediaClient_Destroy(void *media_client)
    {
        delete (MediaClient *) media_client;
    }

    std::uint64_t CALL MediaClient_AddAudioStream(void *instance,
                                                  std::uint64_t domain,
                                                  std::uint64_t conference_id,
                                                  std::uint64_t client_id,
                                                  std::uint8_t media_direction,
                                                  std::uint8_t sample_type,
                                                  std::uint16_t sample_rate,
                                                  std::uint8_t channels)
    {
        if (!instance)
        {
            return 0;        // invalid
        }
        auto media_client = static_cast<MediaClient *>(instance);

        MediaConfig config{};
        config.media_direction = (MediaConfig::MediaDirection) media_direction;
        config.sample_type = (AudioConfig::SampleType) sample_type;
        config.sample_rate = sample_rate;
        config.channels = channels;
        return media_client->add_audio_stream(
            domain, conference_id, client_id, config);
    }

    std::uint64_t CALL
    MediaClient_AddVideoStream(void *instance,
                               std::uint64_t domain,
                               std::uint64_t conference_id,
                               std::uint64_t client_id,
                               std::uint8_t media_direction,
                               std::uint8_t pixel_format,
                               std::uint32_t video_max_width,
                               std::uint32_t video_max_height,
                               std::uint32_t video_max_frame_rate,
                               std::uint32_t video_max_bitrate)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<MediaClient *>(instance);
        MediaConfig config{};
        config.media_direction = (MediaConfig::MediaDirection) media_direction;
        auto pixel_format_type = (VideoConfig::PixelFormat) pixel_format;
        if (config.media_direction == MediaConfig::MediaDirection::sendonly)
        {
            config.video_encode_pixel_format = pixel_format_type;
        }

        if (config.media_direction == MediaConfig::MediaDirection::recvonly)
        {
            config.video_decode_pixel_format = pixel_format_type;
        }

        if (config.media_direction == MediaConfig::MediaDirection::sendrecv)
        {
            config.video_encode_pixel_format = pixel_format_type;
            config.video_decode_pixel_format = pixel_format_type;
        }

        config.video_max_height = video_max_height;
        config.video_max_width = video_max_width;
        config.video_max_bitrate = video_max_bitrate;
        config.video_max_frame_rate = video_max_frame_rate;

        return media_client->add_video_stream(
            domain, conference_id, client_id, config);
    }

    void CALL MediaClient_RemoveVideoStream(std::uint64_t /*media_stream_id*/)
    {
    }

    void CALL MediaClient_sendAudio(void *instance,
                                    uint64_t media_stream_id,
                                    const char *buffer,
                                    std::uint16_t length,
                                    std::uint64_t timestamp)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        media_client->send_audio(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp);
    }

    int CALL MediaClient_getAudio(void *instance,
                                  std::uint64_t streamId,
                                  std::uint64_t *timestamp,
                                  unsigned char **buffer,
                                  unsigned int max_len,
                                  void **to_free)
    {
        if (!instance)
        {
            return -1;
        }
        auto media_client = static_cast<MediaClient *>(instance);

        return media_client->get_audio(streamId, *timestamp, buffer, max_len, to_free);
    }

    void CALL MediaClient_sendVideoFrame(void *instance,
                                         std::uint64_t streamId,
                                         const char *buffer,
                                         uint32_t length,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride_y,
                                         uint32_t stride_uv,
                                         uint32_t offset_u,
                                         uint32_t offset_v,
                                         uint32_t format,
                                         uint64_t timestamp)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        media_client->send_video(
            streamId,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            width,
            height,
            stride_y,
            stride_uv,
            offset_u,
            offset_v,
            format,
            timestamp);
    }

    std::uint32_t CALL MediaClient_getVideoFrame(void *instance,
                                                 std::uint64_t streamId,
                                                 std::uint64_t *timestamp,
                                                 std::uint32_t *width,
                                                 std::uint32_t *height,
                                                 std::uint32_t *format,
                                                 unsigned char **buffer,
                                                 void **to_free)
    {
        if (!instance)
        {
            return -1;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->get_video(
            streamId, *timestamp, *width, *height, *format, buffer, to_free);
    }

    void CALL release_media_buffer(void *instance, void* buffer) {
        if (!instance || !buffer)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->release_media_buffer(buffer);
    }
}
