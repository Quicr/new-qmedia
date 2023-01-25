#include "neo_media_client.hh"
#include <qmedia/media_client.hh>

using namespace qmedia;

extern "C"
{
    void CALL MediaClient_Create(const char *remote_address,
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
#if 0
        auto logCallback =
            [log_callback](LogLevel /*level*/, const std::string &message)
        { log_callback(message.c_str()); };
        logger->SetLogCallback(logCallback);
#endif

        // Create media library.
        auto client = std::make_unique<MediaClient>(logger);
        *media_client = client.release();
    }

    void CALL MediaClient_Destroy(void *media_client)
    {
        delete (MediaClient *) media_client;
    }

    std::uint64_t CALL MediaClient_AddAudioStream(void *instance,
                                                  std::uint8_t media_type,
                                                  std::uint8_t codec_type,
                                                  std::uint8_t media_direction)
    {
        if (!instance)
        {
            return 0;        // invalid
        }
        auto media_client = static_cast<MediaClient *>(instance);

        MediaDirection direction = (MediaDirection) media_direction;

        quicr::QUICRName name;
        name.hi = media_type;
        name.low = codec_type;

        return media_client->add_object_stream(name, direction);
    }

    std::uint64_t CALL
    MediaClient_AddVideoStream(void *instance,
                               std::uint8_t media_type,
                               std::uint8_t codec_type,
                               std::uint8_t media_direction)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<MediaClient *>(instance);

        MediaDirection direction = (MediaDirection) media_direction;

        quicr::QUICRName name;
        name.hi = media_type;
        name.low = codec_type;

        return media_client->add_object_stream(name, direction);
    }

    void CALL MediaClient_RemoveMediaStream(void *instance,
                                            std::uint64_t media_stream_id)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        media_client->remove_object_stream(media_stream_id);
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
        media_client->send_object(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp);
    }

    int CALL MediaClient_getAudio(void *instance,
                                  std::uint64_t streamId,
                                  std::uint64_t *timestamp,
                                  unsigned char **buffer)
    {
        if (!instance)
        {
            return -1;
        }
        auto media_client = static_cast<MediaClient *>(instance);

        return media_client->get_object(streamId, *timestamp, buffer);
    }

    void CALL MediaClient_sendVideoFrame(void *instance,
                                         std::uint64_t streamId,
                                         const char *buffer,
                                         uint32_t length,
                                         uint64_t timestamp)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        media_client->send_object(
            streamId,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp);
    }

    std::uint32_t CALL MediaClient_getVideoFrame(void *instance,
                                                 std::uint64_t streamId,
                                                 std::uint64_t *timestamp,
                                                 unsigned char **buffer)
    {
        if (!instance)
        {
            return -1;
        }

        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->get_object(
            streamId, *timestamp, buffer);
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
