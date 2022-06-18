
#include "neo_media_client.hh"
#include "neo.hh"

using namespace neo_media;

extern "C"
{
    NeoMediaInstance CALL Init(const char *remote_address,
                               std::uint16_t remote_port,
                               std::uint16_t audio_sample_rate,
                               std::uint16_t audio_channels,
                               std::uint16_t audio_type,
                               std::uint16_t video_max_width,
                               std::uint16_t video_max_height,
                               std::uint16_t video_max_frame_rate,
                               std::uint32_t video_max_bitrate,
                               std::uint16_t video_encode_pixel_format,
                               std::uint16_t video_decode_pixel_format,
                               std::uint64_t clientId,
                               std::uint64_t conferenceId,
                               SourceCallback callback,
                               std::uint16_t transportType,
                               ExternLogCallback log,
                               uint16_t media_direction,
                               bool echo)
    {
        // Bridge to external logging.
        LoggerPointer logger = std::make_shared<Logger>("NEO_EXTERN");
        logger->SetLogFacility(LogFacility::NOTIFY);
        auto logCallback = [log](LogLevel /*level*/, const std::string &message)
        { log(message.c_str()); };
        logger->SetLogCallback(logCallback);

        auto wrapped_callback = [callback](uint64_t client_id,
                                           uint64_t source_id,
                                           uint64_t source_ts,
                                           Packet::MediaType media_type)
        { callback(client_id, source_id, source_ts, (int) media_type); };
        // Create media library.
        auto instance = new Neo(logger);
        instance->init(std::string(remote_address),
                       remote_port,
                       audio_sample_rate,
                       audio_channels,
                       Neo::audio_sample_type(audio_type),
                       video_max_width,
                       video_max_height,
                       video_max_frame_rate,
                       video_max_bitrate,
                       (Neo::video_pixel_format) video_encode_pixel_format,
                       (Neo::video_pixel_format) video_decode_pixel_format,
                       clientId,
                       conferenceId,
                       wrapped_callback,
                       (NetTransport::Type) transportType,
                       (Neo::MediaDirection) media_direction,
                       echo);
        return instance;
    }

    void CALL sendAudio(NeoMediaInstance instance,
                        const char *buffer,
                        std::uint16_t length,
                        std::uint64_t timestamp,
                        std::uint64_t source_id)
    {
        auto neo = (Neo *) instance;
        neo->sendAudio(buffer, length, timestamp, source_id);
    }

    int CALL getAudio(NeoMediaInstance instance,
                      std::uint64_t client_id,
                      std::uint64_t source_id,
                      std::uint64_t &timestamp,
                      unsigned char **buffer,
                      std::uint16_t max_len,
                      void **packet_to_free)
    {
        auto neo = (Neo *) instance;
        return neo->getAudio(client_id,
                             source_id,
                             timestamp,
                             buffer,
                             max_len,
                             (Packet **) packet_to_free);
    }

    void CALL sendVideoFrame(NeoMediaInstance instance,
                             const char *buffer,
                             std::uint32_t length,
                             std::uint32_t width,
                             std::uint32_t height,
                             std::uint32_t stride_y,
                             std::uint32_t stride_uv,
                             std::uint32_t offset_u,
                             std::uint32_t offset_v,
                             std::uint32_t format,
                             std::uint64_t timestamp,
                             std::uint64_t source_id)
    {
        auto neo = (Neo *) instance;
        neo->sendVideoFrame(buffer,
                            length,
                            width,
                            height,
                            stride_y,
                            stride_uv,
                            offset_u,
                            offset_v,
                            format,
                            timestamp,
                            source_id);
    }

    std::uint32_t CALL getVideoFrame(NeoMediaInstance instance,
                                     std::uint64_t client_id,
                                     std::uint64_t source_id,
                                     std::uint64_t &timestamp,
                                     std::uint32_t &width,
                                     std::uint32_t &height,
                                     std::uint32_t &format,
                                     unsigned char **buffer)
    {
        auto neo = (Neo *) instance;
        return neo->getVideoFrame(
            client_id, source_id, timestamp, width, height, format, buffer);
    }

    void CALL setLoopbackMode(NeoMediaInstance instance, std::uint8_t mode)
    {
        auto neo = (Neo *) instance;
        return neo->setLoopbackMode(mode);
    }
    void CALL freePacket(void *packet) { delete (Packet *) packet; }

    void CALL Destroy(NeoMediaInstance *instance) { delete (Neo *) instance; }

    void CALL setMicrophoneMute(NeoMediaInstance instance, bool muted)
    {
        auto neo = (Neo *) instance;
        neo->setMicrophoneMute(muted);
    }

    void CALL publish(NeoMediaInstance instance,
                      std::uint64_t source_id,
                      std::uint16_t media_type,
                      const char *url,
                      std::uint16_t url_length)
    {
        auto neo = (Neo *) instance;
        neo->publish(source_id,
                     (Packet::MediaType) media_type,
                     std::string(url, url + url_length));
    }

    void CALL subscribe(NeoMediaInstance instance,
                        std::uint64_t source_id,
                        std::uint16_t media_type,
                        const char *url,
                        std::uint16_t url_length)
    {
        auto neo = (Neo *) instance;
        neo->subscribe(source_id,
                       (Packet::MediaType) media_type,
                       std::string(url, url + url_length));
    }

    void CALL start_transport(NeoMediaInstance instance,
                              std::uint16_t transport_type)
    {
        auto neo = (Neo *) instance;
        neo->start_transport((NetTransport::Type) transport_type);
    }
}
