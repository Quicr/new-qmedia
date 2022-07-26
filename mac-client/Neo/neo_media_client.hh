/*
 *  neo_media_client.hh
 *
 *  Copyright (C) 2020
 *  Cisco Systems, Inc.
 *  All Rights Reserved.
 *
 *  Description:
 *      This is an external header file that will be used by media client to
 *      send and receive media on the fancy new cto_media_pipeline.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <Foundation/Foundation.h>
//#include <cstdint>
#if defined(__linux__) || defined(__APPLE__)
#define EXPORT
#define CALL
#elif _WIN32
#define EXPORT __declspec(dllexport)
#define CALL __stdcall
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    /// <summary>
    /// C callback for Neo::callbackSourceId.
    /// </summary>
    typedef void(CALL *NewStreamCallback)(uint64_t client,
                                       uint64_t source,
                                       uint64_t start_time,
                                       int source_type);

    typedef void(CALL *ExternalLogCallback)(const char *message);

    /// <summary>
    /// Represents a Neo instance.
    /// </summary>
    typedef void *MediaClientInstance;

    EXPORT void CALL MediaClient_Create(ExternalLogCallback log_callback,
                                        NewStreamCallback stream_callback,
                                        const char *remote_address,
                                        uint16_t remote_port,
                                        void **media_client);

   EXPORT uint64_t CALL
    MediaClient_AddAudioStream(void *instance,
                               uint64_t domain,
                               uint64_t conference_id,
                               uint64_t client_id,
                               uint8_t media_direction,
                               uint8_t sample_type,
                               uint16_t sample_rate,
                               uint8_t channels);

    EXPORT uint64_t CALL
    MediaClient_AddVideoStream(void *instance,
                               uint64_t domain,
                               uint64_t conference_id,
                               uint64_t client_id,
                               uint8_t media_direction,
                               uint8_t pixel_format,
                               uint32_t video_max_width,
                               uint32_t video_max_height,
                               uint32_t video_max_frame_rate,
                               uint32_t video_max_bitrate);

    EXPORT void CALL MediaClient_sendAudio(void *instance,
                                           uint64_t media_stream_id,
                                           const char *buffer,
                                           uint16_t length,
                                           uint64_t timestamp);

    EXPORT void CALL MediaClient_sendVideoFrame(void *instance,
                                                uint64_t streamId,
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

    EXPORT int CALL MediaClient_getAudio(void *instance,
                                         uint64_t streamId,
                                         uint64_t *timestamp,
                                         unsigned char **buffer,
                                         uint16_t max_len,
                                         void **to_free);

    EXPORT int CALL MediaClient_getVideoFrame(void *instance,
                                              uint64_t streamId,
                                              uint64_t *timestamp,
                                              uint32_t *width,
                                              uint32_t *height,
                                              uint32_t *format,
                                              unsigned char **buffer,
                                              void **to_free);

   EXPORT void CALL MediaClient_RemoveMediaStream(void *instance,
                                                  uint64_t media_stream_id);

   EXPORT void CALL release_media_buffer(void *instance, void* buffer);

#ifdef __cplusplus
}
#endif
