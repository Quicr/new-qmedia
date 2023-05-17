
#include <qmedia/neo_media_client.hh>
#include <qmedia/media_client.hh>

extern "C"
{
    void MediaClient_Create(const char* remote_address, uint16_t remote_port, uint8_t protocol, void** media_client)
    {
        if (!media_client || !remote_address)
        {
            return;
        }

        *media_client = new qmedia::MediaClient(remote_address, remote_port, static_cast<quicr::RelayInfo::Protocol>(protocol));
    }

    void MediaClient_Destroy(void* media_client)
    {
        delete (qmedia::MediaClient*) media_client;
    }

    uint64_t MediaClient_AddStreamPublishIntent(void* instance, uint32_t conf_id, uint8_t media_type, uint16_t client_id)
    {
        if (!instance)
        {
            return 0;        // invalid
        }
        auto media_client = static_cast<qmedia::MediaClient*>(instance);
        return media_client->add_publish_intent(conf_id, media_type, client_id);
    }

    uint64_t MediaClient_AddStreamSubscribe(void* instance, uint32_t conf_id, uint8_t media_type, uint16_t client_id, SubscribeCallback callback)
    {
        if (!instance)
        {
            return 0;        // invalid
        }

        auto media_client = static_cast<qmedia::MediaClient*>(instance);
        return media_client->add_stream_subscribe(conf_id, media_type, client_id, callback);
    }

    void MediaClient_sendAudio(void* instance,
                               uint64_t media_stream_id,
                               const char* buffer,
                               uint32_t length,
                               uint64_t timestamp)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient*>(instance);

        media_client->send_audio_media(
            media_stream_id, reinterpret_cast<uint8_t*>(const_cast<char*>(buffer)), length, timestamp);
    }

    void MediaClient_sendVideoFrame(void* instance,
                                    uint64_t media_stream_id,
                                    const char* buffer,
                                    uint32_t length,
                                    uint64_t timestamp,
                                    bool groupidflag)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient*>(instance);

        media_client->send_video_media(
            media_stream_id, reinterpret_cast<uint8_t*>(const_cast<char*>(buffer)), length, timestamp, groupidflag);
    }

    void MediaClient_RemoveMediaSubscribeStream(void* instance, uint64_t media_stream_id)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient*>(instance);
        media_client->remove_subscribe(media_stream_id);
    }

    void MediaClient_RemoveMediaPublishStream(void *instance, uint64_t media_stream_id)
    {
        if (!instance)
        {
            return;
        }

        auto media_client = static_cast<qmedia::MediaClient*>(instance);
        media_client->remove_publish(media_stream_id);
    }
}
