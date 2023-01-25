#pragma once

#include <cstdint>
#if defined(__linux__) || defined(__APPLE__)
#define EXPORT
#define CALL
#elif _WIN32
#define EXPORT __declspec(dllexport)
#define CALL __stdcall
#endif

#include <qmedia/callback.hh>

extern "C"
{
    EXPORT void CALL MediaClient_Create(const char *remote_address,
                                        std::uint16_t remote_port,
                                        void **media_client);

    EXPORT void CALL MediaClient_Destroy(void *media_client);

    EXPORT std::uint64_t CALL
    MediaClient_AddAudioStream(void *instance,
                               std::uint8_t media_type,
                               std::uint8_t codec_type,
                               std::uint8_t media_direction);

    EXPORT std::uint64_t CALL
    MediaClient_AddVideoStream(void *instance,
                               std::uint8_t media_type,
                               std::uint8_t codec_type,
                               std::uint8_t media_direction);

    EXPORT void CALL
    MediaClient_RemoveMediaStream(void *instance,
                                  std::uint64_t media_stream_id);

    EXPORT void CALL MediaClient_sendAudio(void *instance,
                                           uint64_t media_stream_id,
                                           const char *buffer,
                                           std::uint16_t length,
                                           std::uint64_t timestamp);

    EXPORT int CALL MediaClient_getAudio(void *instance,
                                         std::uint64_t streamId,
                                         std::uint64_t *timestamp,
                                         unsigned char **buffer);

    EXPORT void CALL MediaClient_sendVideoFrame(void *instance,
                                                std::uint64_t streamId,
                                                const char *buffer,
                                                uint32_t length,
                                                uint64_t timestamp);

    EXPORT std::uint32_t CALL MediaClient_getVideoFrame(void *instance,
                                                        std::uint64_t streamId,
                                                        std::uint64_t *timestamp,
                                                        unsigned char **buffer);


    EXPORT void CALL release_media_buffer(void *instance, void* buffer);
}
