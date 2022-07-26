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

namespace
{
using ExternalLogCallback = CallbackType<void CALL(const char *message)>;
using NewStreamCallback = CallbackType<void CALL(std::uint64_t client,
                                                 std::uint64_t source,
                                                 std::uint64_t start_time,
                                                 int source_type)>;
}        // namespace

extern "C"
{
    EXPORT void CALL MediaClient_Create(ExternalLogCallback log_callback,
                                        NewStreamCallback stream_callback,
                                        const char *remote_address,
                                        std::uint16_t remote_port,

                                        void **media_client);

    EXPORT void CALL MediaClient_Destroy(void *media_client);

    EXPORT std::uint64_t CALL
    MediaClient_AddAudioStream(void *instance,
                               std::uint64_t domain,
                               std::uint64_t conference_id,
                               std::uint64_t client_id,
                               std::uint8_t media_direction,
                               std::uint8_t sample_type,
                               std::uint16_t sample_rate,
                               std::uint8_t channels);

    EXPORT std::uint64_t CALL
    MediaClient_AddVideoStream(void *instance,
                               std::uint64_t domain,
                               std::uint64_t conference_id,
                               std::uint64_t client_id,
                               std::uint8_t media_direction,
                               std::uint8_t pixel_format,
                               std::uint32_t video_max_width,
                               std::uint32_t video_max_height,
                               std::uint32_t video_max_frame_rate,
                               std::uint32_t video_max_bitrate);

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
                                         unsigned char **buffer,
                                         unsigned int max_len,
                                         void **to_free);

    EXPORT void CALL MediaClient_sendVideoFrame(void *instance,
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
                                                uint64_t timestamp);

    EXPORT std::uint32_t CALL MediaClient_getVideoFrame(void *instance,
                                                        std::uint64_t streamId,
                                                        std::uint64_t *timestamp,
                                                        std::uint32_t *width,
                                                        std::uint32_t *height,
                                                        std::uint32_t *format,
                                                        unsigned char **buffer,
                                                        void **to_free);

    EXPORT void CALL release_media_buffer(void *instance, void* buffer);
}
