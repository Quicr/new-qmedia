
#include <qmedia/neo_media_client.hh>
#include <qmedia/media_client.hh>

extern "C"
{
    void  MediaClient_Create(const char *remote_address,
                             uint16_t remote_port,
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
        auto client = std::make_unique<qmedia::MediaClient>(remote_address,
                        remote_port,
                        0,
                        logger);

        *media_client = client.release();
    }

    void  MediaClient_Destroy(void *media_client)
    {
        delete (qmedia::MediaClient *) media_client;
    }

    uint64_t
    MediaClient_AddAudioStreamPublish(void *instance,
                                      uint8_t codec_type)
    {
        if (!instance)
        {
            return 0;        // invalid
        }
        auto media_client = static_cast<qmedia::MediaClient *>(instance);
        return media_client->add_audio_publish_intent(codec_type);  
    }

     uint64_t  
     MediaClient_AddAudioStreamSubscribe(void *instance,
                                                  uint8_t codec_type,
                                                  SubscribeCallback callback)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);
        return media_client->add_audio_stream_subscribe(codec_type, callback);
    }


    uint64_t 
    MediaClient_AddAudioStreamPublishIntent(void *instance,
                               uint8_t codec_type)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);
        return media_client->add_audio_publish_intent( codec_type);
    }

    uint64_t 
    MediaClient_AddVideoStreamPublishIntent(void *instance,
                               uint8_t codec_type)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);
        return media_client->add_video_publish_intent(codec_type);
    }

     uint64_t  
     MediaClient_AddVideoStreamSubscribe(void *instance,
                                        uint8_t codec_type,
                                        SubscribeCallback callback)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);
        return media_client->add_video_stream_subscribe(codec_type, callback);
    }

    void  MediaClient_RemoveMediaStream(void *instance,
                                            uint64_t media_stream_id)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);
   
       // media_client->remove_object_stream(media_stream_id);
    }

    void  MediaClient_sendAudio(void *instance,
                                    uint64_t media_stream_id,
                                    const char *buffer,
                                    uint32_t length,
                                    uint64_t timestamp)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);

        media_client->send_audio_media(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp
        );
    }

 void  MediaClient_sendVideoFrame(void *instance,
                                    uint64_t media_stream_id,
                                    const char *buffer,
                                    uint32_t length,
                                    uint64_t timestamp,
                                    bool flag)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient *>(instance);

        media_client->send_video_media(
            media_stream_id,
            reinterpret_cast<uint8_t *>(const_cast<char *>(buffer)),
            length,
            timestamp
        );
    }
}
