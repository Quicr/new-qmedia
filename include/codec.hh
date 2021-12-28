#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace neo_media
{
// Video Encoder Interface
class VideoEncoder
{
public:
    virtual int encode(const char *input_buffer,
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
                       bool genKeyFrame) = 0;

    virtual ~VideoEncoder() = default;
};

// Video Decoder Interface
class VideoDecoder
{
public:
    virtual int decode(const char *input_buffer,
                       std::uint32_t input_length,
                       std::uint32_t &width,
                       std::uint32_t &height,
                       std::uint32_t &format,
                       std::vector<std::uint8_t> &output_frame) = 0;
};

}        // namespace neo_media