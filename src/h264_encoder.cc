
#include <iostream>
#include <vector>
#include <cassert>

#include <string.h>        // memcpy , strncat

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

    // Apply client app settings
    encodeParams.eSpsPpsIdStrategy = INCREASING_ID;
    // encodeParams.bPrefixNalAddingCtrl = 1; // codec_type SVC
    encodeParams.bPrefixNalAddingCtrl = 0;        // codec_type AVC

    encodeParams.iPicWidth = video_max_width;
    encodeParams.iPicHeight = video_max_height;
    encodeParams.fMaxFrameRate = video_max_frame_rate;
    encodeParams.iTargetBitrate = video_max_bitrate;        // bits/sec in base
    encodeParams.bEnableFrameSkip = 1;

    encodeParams.bEnableDenoise = false;        // disable denoise
    encodeParams.bEnableSceneChangeDetect = 1;
    encodeParams.bEnableBackgroundDetection = 1;
    encodeParams.bEnableAdaptiveQuant = 0;
    encodeParams.bEnableLongTermReference = 1;
    encodeParams.iLtrMarkPeriod = 30;
    encodeParams.iNumRefFrame = AUTO_REF_PIC_COUNT;

    encoder->InitializeExt(&encodeParams);

    encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &video_pixel_format);
}

H264Encoder::~H264Encoder()
{
    if (encoder)
    {
        long ret = encoder->Uninitialize();
        encoder = nullptr;
    }
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

    int videoFormat = videoFormatI420;
    encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);

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

    // encode worked
    auto a = outputFrame.iFrameSizeInBytes;
    auto b = outputFrame.sLayerInfo->pNalLengthInByte;
    output_bitstream.resize(outputFrame.iFrameSizeInBytes);
    unsigned char *out_bits = output_bitstream.data();
    memcpy(out_bits,
           outputFrame.sLayerInfo->pBsBuf,
           outputFrame.iFrameSizeInBytes);
    total_bytes_encoded += output_bitstream.size();
    total_frames_encoded++;

    /*
    // Debug log for frame number, type, avg time in usec, avg size in bytes
    if (((total_frames_encoded-1) & 31) == 0) { // every 32 frames, ~1 sec
        std::cerr << std::endl << height << "p "
            << (format ? "I420" : "NV12")
            << (encodeParams.request_key_frame ? " I " : " P ")
            << "Frame#" << (total_frames_encoded-1) << " "
            << bitstream.size() << " bytes "
            << encode_time << " usec, avg "
            << (total_bytes_encoded/total_frames_encoded) << " bytes "
            << (total_time_encoded/total_frames_encoded) << " usec" <<
    std::endl;
        //av1_encoder_stats_t stats = getStats(encoder);
        //assert(0); // halt to check stats
    }
    */

    // success
    return 0;
}
