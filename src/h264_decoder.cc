
#include <iostream>
#include <vector>
#include <cassert>
#include <fstream>
#include <stdio.h>

#include <string.h>        // memcpy , strncat

#include "h264_decoder.hh"

using namespace neo_media;
using namespace std;

static bool debug = false;

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

    auto ret = decoder->DecodeFrame2(
        reinterpret_cast<const unsigned char *>(input_buffer),
        input_length,
        dst,
        &dst_info);

    if (dsErrorFree != ret)
    {
        std::cerr << " H264 decode frame failed" << std::endl;
        // handle IDR request
    }

    if (dst_info.iBufferStatus == 1)
    {
        width = dst_info.UsrData.sSystemBuffer.iWidth;
        height = dst_info.UsrData.sSystemBuffer.iHeight;

        auto color_fmt = dst_info.UsrData.sSystemBuffer.iFormat;
        if (debug) {
            std::cerr << "Decoded Width : " << width << std::endl;
            std::cerr << "Decoded height : " << height << std::endl;
            std::cerr << "Decoded Format : " << color_fmt << std::endl;
            std::cerr << "Decoded Stride 0  : " << dst_info.UsrData.sSystemBuffer.iStride[0] << std::endl;
            std::cerr << "Decoded Stride 1 : " << dst_info.UsrData.sSystemBuffer.iStride[1] << std::endl;
        }

        auto y_size = width * height;
        auto uv_size = y_size >> 2;

        output_frame.resize(y_size + 2 * uv_size);
        uint8_t *outp = output_frame.data();

        size_t i = 0;
        auto pPtr = dst[0];
        for(i = 0; i < height; i++)
        {
            memcpy(outp, pPtr, width);
            outp += width;
            pPtr += dst_info.UsrData.sSystemBuffer.iStride[0];
        }

        pPtr = dst[1];
        for(i = 0; i < height/2; i++)
        {
            memcpy(outp, pPtr, width/2);
            outp += width/2;
            pPtr += dst_info.UsrData.sSystemBuffer.iStride[1];
        }

        pPtr = dst[2];
        for(i = 0; i < height/2; i++)
        {
            memcpy(outp, pPtr, width/2);
            outp += width/2;
            pPtr += dst_info.UsrData.sSystemBuffer.iStride[1];
        }
    }

    return 0;
}
