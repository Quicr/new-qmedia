
#include <iostream>
#include <vector>
#include <cassert>

#include <string.h>        // memcpy , strncat
#include <sstream>
#include <iomanip>

#include "h264_encoder.hh"

using namespace neo_media;

static bool debug = false;

H264Encoder::H264Encoder(unsigned int video_max_width,
                         unsigned int video_max_height,
                         unsigned int video_max_frame_rate,
                         unsigned int video_max_bitrate,
                         std::uint32_t video_pixel_format,
                         const LoggerPointer& logger_in)
{
    logger = logger_in;
    int rv = WelsCreateSVCEncoder(&encoder);
    if (rv || !encoder)
    {
        logger->error << "H264 video encoder create failed" << std::flush;
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

    // nv12 to i420 planar
    auto* input = reinterpret_cast<unsigned char *>(const_cast<char *>(input_buffer));
    auto *p = input + (stride_y * height); // data buffer of inpt UV plane (Y+Ystride*Yheight)
    int w = width/2;
    int h = height/2;    // width,height of input UV plane (Ywidth/2,Yheight/2)
    int suv = stride_uv;  // stride of input UV plane (often same as Y)
    int su  = stride_uv/2; // stride of output U plane (often half of Y)
    int sv  = stride_uv/2; // stride of output V plane (often half of Y)

    auto uv = (unsigned char*) malloc(suv*h); // temp buffer to copy input UV plane
    auto *u=p;      // U plane of output (overwrites input)
    auto orig_u = p;
    auto *v=u+su*h; // V plane of output (overwrites input)
    auto orig_v = orig_u+su*h;
    memcpy(uv,p,suv*h); // copy input UV plane to temp buffer

    // de-interleave UV plane
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            // copy the even bytes to U place
            u[x]=uv[x*2];
            // copy the odd bytes to V plane
            v[x]=uv[x*2+1];
        }
        u+=su;
        v+=sv;
        uv+=suv;
    }

    output_bitstream.clear();

    SSourcePicture sourcePicture;
    memset(&sourcePicture, 0, sizeof(sourcePicture));
    sourcePicture.iColorFormat = videoFormatI420;
    sourcePicture.iPicHeight = (int) height;
    sourcePicture.iPicWidth = (int) width;
    sourcePicture.uiTimeStamp = timestamp * 9 / 100;
    sourcePicture.pData[0] = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer));
    sourcePicture.pData[1] = orig_u;
    sourcePicture.pData[2] = orig_v;
    sourcePicture.iStride[0] = (int) stride_y;
    sourcePicture.iStride[1] = (int) su;
    sourcePicture.iStride[2] = (int) sv;


    //assert(0);
    int videoFormat = videoFormatI420;
    encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);

    if(genKeyFrame) {
        logger->info << "Encode: Force IDR" << std::flush;
        auto ret = encoder->ForceIntraFrame(true);
        logger->error << "Encode: IDR Frame Generation Error " << ret << std::flush;
    }

    memset(&outputFrame, 0, sizeof (SFrameBSInfo));
    int ret = encoder->EncodeFrame(&sourcePicture, &outputFrame);
    if (ret == 0)
    {
        switch (outputFrame.eFrameType)
        {
            case videoFrameTypeSkip:
                logger->debug << "Encode: videoFrameTypeSkip" << std::flush;
                return 0;
            case videoFrameTypeInvalid:
                logger->debug << "Encode failed: " << ret << std::flush;
                return -1;
        }
    }

    // encode worked
    if(debug) {
        logger->debug << "Encoded iFrameSizeInBytes: " << outputFrame.iFrameSizeInBytes << std::flush;
        logger->debug << "Encoded num_layer: " << outputFrame.iLayerNum << std::flush;
        logger->debug << "Frame Type: " << outputFrame.eFrameType << std::flush;
    }

    // move the encoded data to output bitstream
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

        out_bits += len;
    }

    total_bytes_encoded += output_bitstream.size();
    total_frames_encoded++;

    // success
    return outputFrame.eFrameType == videoFrameTypeIDR ? 1 : 0;
}
