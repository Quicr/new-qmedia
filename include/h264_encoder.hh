#pragma once

#include <stdint.h>
#include <vector>
#include <chrono>
#include <wels/codec_api.h>

#include "logger.hh"
#include "codec.hh"

namespace neo_media
{
class H264Encoder : public VideoEncoder
{
public:
    H264Encoder(unsigned int video_max_width,
                unsigned int video_max_height,
                unsigned int video_max_frame_rate,
                unsigned int video_max_bitrate,
                std::uint32_t video_pixel_format,
                const LoggerPointer &logger);

    ~H264Encoder();

    int encode(const char *input_buffer,
               std::uint32_t input_length,
               std::uint32_t width,
               std::uint32_t height,
               std::uint32_t stride_y,
               std::uint32_t stride_uv,
               std::uint32_t offset_u,
               std::uint32_t offset_v,
               std::uint32_t format,
               std::uint64_t timestamp,
               std::vector<std::uint8_t> &output_bitstream,
               bool genKeyFrame) override;

    int width;
    int height;
    float fps;
    int rate;
    unsigned int idr_interval = 30;
    const unsigned int mtu_size = 1300;

    ISVCEncoder *encoder = nullptr;
    SEncParamExt encParmExt;
    SFrameBSInfo encodedFrame;
    SSourcePicture inputFrame;
    LoggerPointer logger;
};
}        // namespace neo_media