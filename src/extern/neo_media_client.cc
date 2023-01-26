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


        // Create media library.
        auto client = std::make_unique<MediaClient>(remote_address,
                        remote_port,
                        0,
                        logger);

        *media_client = client.release();
    }

    void CALL MediaClient_Destroy(void *media_client)
    {
        delete (MediaClient *) media_client;
    }

    std::uint64_t CALL MediaClient_AddAudioStreamPublish(void *instance,
                                                  std::uint8_t media_type,
                                                  std::uint8_t codec_type)
    {
        if (!instance)
        {
            return 0;        // invalid
        }
        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->add_audio_publish_intent(media_type, codec_type);  
    }



     std::uint64_t CALL 
     MediaClient_AddAudioStreamSubscribe(void *instance,
                                                  std::uint8_t media_type,
                                                  std::uint8_t codec_type,
                                                  SubscribeCallback callback)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->add_audio_stream_subscribe(media_type, codec_type, callback);
    }


    std::uint64_t CALL
    MediaClient_AddVideoStreamPublishIntent(void *instance,
                               std::uint8_t media_type,
                               std::uint8_t codec_type)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<MediaClient *>(instance);
        return media_client->add_audio_publish_intent(media_type, codec_type);
    }
/*
    std::uint64_t CALL
    MediaClient_AddVideoStreamSubscribe((void *instance,
                                        std::uint8_t media_type,
                                        std::uint8_t codec_type
                                        SubscribeCallback callback)
    {

    }
*/
    void CALL MediaClient_RemoveMediaStream(void *instance,
                                            std::uint64_t media_stream_id)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<MediaClient *>(instance);
   
       // media_client->remove_object_stream(media_stream_id);
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

        media_client->send_audio_media(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp
        );
    }

 void CALL MediaClient_sendVideo(void *instance,
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

        media_client->send_video_media(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp
        );
    }
}
