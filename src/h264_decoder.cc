
#include <iostream>
#include <vector>
#include <cassert>

#include <string.h>        // memcpy , strncat

#include "h264_decoder.hh"

using namespace neo_media;

H264Decoder::H264Decoder(std::uint32_t video_pixel_format)
{
    auto ret = WelsCreateDecoder(&decoder);
    if (ret || !decoder)
    {
        std::cerr << "H264 video decoder create failed" << std::endl;
        assert(0);        // todo: throw exception?
    }

    SDecodingParam dec_param;
    memset(&dec_param, 0, sizeof(SDecodingParam));
    dec_param.uiTargetDqLayer = 255;
    dec_param.uiCpuLoad = 0;
    dec_param.eEcActiveIdc = ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
    dec_param.sVideoProperty.size = sizeof(SVideoProperty);
    dec_param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    // dec_param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_SVC;

    ret = decoder->Initialize(&dec_param);
    if (dsErrorFree != ret)
    {
        decoder->Uninitialize();
        delete decoder;
        decoder = nullptr;
        assert(0);
    }
}

H264Decoder::~H264Decoder()
{
    if (decoder)
    {
        decoder->Uninitialize();
        decoder = nullptr;
    }
}

/** Wrap underlying async APIs to expose sync API to decode input bitstream to
 * raw YUV frame. Returns 0 on success and fills output_frame with decoded
 * frame, or DAV1D_ERR(errno) if any error.
 */
int H264Decoder::decode(const char *input_buffer,
                        std::uint32_t input_length,
                        std::uint32_t &width,
                        std::uint32_t &height,
                        std::uint32_t &format,
                        std::vector<std::uint8_t> &output_frame)
{
    if (!decoder)
    {
        assert(0);
    }

    unsigned char *dst[3];
    SBufferInfo dst_info;

    auto ret = decoder->DecodeFrameNoDelay(
        reinterpret_cast<const unsigned char *>(input_buffer),
        input_length,
        dst,
        &dst_info);
    if (dsErrorFree != ret)
    {
        std::cerr << " 1st decode frame failed" << std::endl;
        // handle IDR request
    }

    if (dst_info.iBufferStatus == 1)
    {
    }

    width = dst_info.UsrData.sSystemBuffer.iWidth;
    height = dst_info.UsrData.sSystemBuffer.iHeight;

    const int y_size = width * height;
    const int uv_size = y_size >> 2;        // YUV420

    output_frame.resize(y_size + 2 * uv_size);
    uint8_t *outp = output_frame.data();

    // Copy Y plane
    memcpy(outp, dst[0], y_size);
    memcpy(outp + y_size, dst[1], uv_size);
    memcpy(outp + y_size + uv_size, dst[2], uv_size);

    return 0;
}
