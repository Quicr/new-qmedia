#include <iostream>
#include <vector>
#include <cassert>

#include <string.h>        // memcpy , strncat
#include <sstream>
#include <iomanip>

#include "h264_encoder.hh"

using namespace neo_media;

H264Encoder::H264Encoder(unsigned int video_max_width,
                         unsigned int video_max_height,
                         unsigned int video_max_frame_rate,
                         unsigned int video_max_bitrate,
                         std::uint32_t video_pixel_format)
{
    int rv = WelsCreateSVCEncoder(&encoder);
    if (rv || !encoder)
    {
        std::cerr << "H264 video encoder create failed" << std::endl;
        assert(0);        // todo: throw exception?
    }

    encParamBase.fMaxFrameRate = 30;
    encParamBase.iPicHeight = video_max_height;
    encParamBase.iPicWidth = video_max_width;
    encParamBase.iTargetBitrate = video_max_bitrate;
    encParamBase.iRCMode = RC_OFF_MODE;
    encoder->Initialize(&encParamBase);

}

H264Encoder::~H264Encoder()
{
    if (encoder)
    {
        long ret = encoder->Uninitialize();
        encoder = nullptr;
    }
}

static std::string to_hex(const std::vector<uint8_t> &data)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    int i = 0;
    for (const auto &byte : data)
    {
        hex << std::setw(2) << std::setfill('0') << int(byte);
        i++;
        if (i > 25)
            break;
    }
    return hex.str();
}

static std::string to_hex(unsigned char* data, int stop)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    for (int i = 0; i < stop; i++)
    {
        hex << std::setw(2) << std::setfill('0') << int(data[i]);
    }
    return hex.str();
}

int H264Encoder::encode(const char *input_buffer,
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
                        bool genKeyFrame)
{
    static std::uint64_t total_frames_encoded = 0;
    static std::uint64_t total_bytes_encoded = 0;
    static std::uint64_t total_time_encoded = 0;        // microseconds

    output_bitstream.clear();

    SSourcePicture sourcePicture;
    memset(&sourcePicture, 0, sizeof(sourcePicture));
    sourcePicture.iColorFormat = videoFormatI420;
    sourcePicture.iPicHeight = (int) height;
    sourcePicture.iPicWidth = (int) width;
    sourcePicture.uiTimeStamp = timestamp * 9 / 100;
    sourcePicture.pData[0] = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer));
    sourcePicture.pData[1] = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer + offset_u));
    sourcePicture.pData[2] = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer + offset_v));
    sourcePicture.iStride[0] = (int) stride_y;
    sourcePicture.iStride[1] = (int) stride_uv;
    sourcePicture.iStride[2] = (int) stride_uv;

    //assert(0);
    int videoFormat = videoFormatI420;
    encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);

    if(genKeyFrame) {
        auto ret = encoder->ForceIntraFrame(true);
        std::cerr << "H264Enc: IDR Frame: " << ret << std::endl;
    }

    memset(&outputFrame, 0, sizeof (SFrameBSInfo));
    int ret = encoder->EncodeFrame(&sourcePicture, &outputFrame);
    if (ret == 0)
    {
        switch (outputFrame.eFrameType)
        {
            case videoFrameTypeSkip:
                return 0;
            case videoFrameTypeInvalid:
                std::cerr << "Encode failed" << std::endl;
                return -1;
        }
    }

    output_bitstream.resize(outputFrame.iFrameSizeInBytes);
    unsigned char *out_bits = output_bitstream.data();
    for(int i = 0; i < outputFrame.iLayerNum; i++) {
        auto len = 0;
        // pNalLengthInByte[0]+pNalLengthInByte[1]+…+pNalLengthInByte[iNalCount-1].
        for(int j =0; j < outputFrame.sLayerInfo[i].iNalCount; j++) {
            len += outputFrame.sLayerInfo[i].pNalLengthInByte[j];
        }

        memcpy(out_bits,
               outputFrame.sLayerInfo[i].pBsBuf,
               len);

        auto stop = len;
        if(len > 25)
            stop = 25;
        std::cerr << to_hex(out_bits, stop) << std::endl;

        out_bits += len;
    }

    total_bytes_encoded += output_bitstream.size();
    total_frames_encoded++;


    // success
    return outputFrame.eFrameType == videoFrameTypeIDR ? 1 : 0;
}
