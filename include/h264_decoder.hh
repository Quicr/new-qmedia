#pragma once

#include <vector>
#include <wels/codec_api.h>
#include "codec.hh"

namespace neo_media
{
class H264Decoder : public VideoDecoder
{
public:
    H264Decoder(std::uint32_t video_pixel_format);

    ~H264Decoder();

    int decode(const char *input_buffer,
               std::uint32_t input_length,
               std::uint32_t &width,
               std::uint32_t &height,
               std::uint32_t &format,
               std::vector<std::uint8_t> &output_frame) override;

private:
    ISVCDecoder *decoder;
    unsigned char fakeNal[6];        // HACK for openH264
    SBufferInfo decodedInfo;
};
}        // namespace neo_media