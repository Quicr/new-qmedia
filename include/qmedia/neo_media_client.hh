#ifndef NEO_MEDIA_CLIENT_HH
#define NEO_MEDIA_CLIENT_HH

#include <stdint.h>
#include <stdbool.h>

typedef void (*SubscribeCallback)(uint64_t id,
                                  uint8_t media_id,
                                  uint16_t client_id,
                                  uint8_t* data,
                                  uint32_t length,
                                  uint64_t timestamp);

#ifdef __cplusplus
extern "C"
{
#endif
    void MediaClient_Create(const char* remote_address, uint16_t remote_port, uint8_t protocol, void** media_client);

    void MediaClient_Destroy(void* media_client);

    uint64_t MediaClient_AddAudioStreamPublishIntent(void* instance, uint8_t media_type, uint16_t client_id);

    uint64_t MediaClient_AddAudioStreamSubscribe(void* instance, uint8_t media_type, uint16_t client_id, SubscribeCallback callback);

    uint64_t MediaClient_AddVideoStreamPublishIntent(void* instance, uint8_t media_type, uint16_t client_id);

    uint64_t MediaClient_AddVideoStreamSubscribe(void* instance, uint8_t media_type, uint16_t client_id, SubscribeCallback callback);

    void MediaClient_RemoveMediaSubscribeStream(void* instance, uint64_t media_stream_id);
    void MediaClient_RemoveMediaPublishStream(void *instance, uint64_t media_stream_id);

    void MediaClient_sendAudio(void* instance,
                               uint64_t media_stream_id,
                               const char* buffer,
                               uint32_t length,
                               uint64_t timestamp);

    void MediaClient_sendVideoFrame(void* instance,
                                    uint64_t streamId,
                                    const char* buffer,
                                    uint32_t length,
                                    uint64_t timestamp,
                                    bool flag);

#ifdef __cplusplus
}
#endif
#endif
