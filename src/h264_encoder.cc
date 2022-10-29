
#include <iostream>
#include <vector>
#include <cassert>

#include <string.h>        // memcpy , strncat
#include <iomanip>

#include "metrics_reporter.hh"
#include "h264_encoder.hh"

using namespace qmedia;

static bool debug = false;

static std::string to_hex(unsigned char *data, int stop)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    for (int i = 0; i < stop; i++)
    {
        hex << std::setw(2) << std::setfill('0') << int(data[i]);
    }
    return hex.str();
}

static VideoEncoder::EncodedFrameType toEncodedFrameType(EVideoFrameType frame)
{
    switch (frame)
    {
        case EVideoFrameType::videoFrameTypeIDR:
            return VideoEncoder::EncodedFrameType::IDR;
        case EVideoFrameType::videoFrameTypeI:
            return VideoEncoder::EncodedFrameType::I;
        case EVideoFrameType::videoFrameTypeP:
            return VideoEncoder::EncodedFrameType::P;
        case EVideoFrameType::videoFrameTypeSkip:
            return VideoEncoder::EncodedFrameType::Skip;
        case EVideoFrameType::videoFrameTypeInvalid:
        default:
            return VideoEncoder::EncodedFrameType::Invalid;
    }
}

H264Encoder::H264Encoder(MediaStreamId  media_stream_id,
                        unsigned int video_max_width,
                         unsigned int video_max_height,
                         unsigned int video_max_frame_rate,
                         unsigned int video_max_bitrate,
                         std::uint32_t video_pixel_format,
                         const LoggerPointer &logger_in)
{
    stream_id = media_stream_id;
    logger = logger_in;
    int rv = WelsCreateSVCEncoder(&encoder);

    assert(rv == cmResultSuccess && encoder);

    // for debugging openH264
    int logLevel = WELS_LOG_ERROR;
    encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &logLevel);

    // ./h264enc -org your_input_I420.yuv -numl 1 numtl 1 -sw 1280 -sh 720 -dw 0
    // 1280 -dh 0 720 -frin 30 -frout 0 30 -rc -1 -lqp 0 24 -utype 0 -iper 128
    // -nalSize 1300 -complexity 1 -denoise -1 -bf test.264
    rv = encoder->GetDefaultParams(&encParmExt);
    assert(rv == cmResultSuccess);

    encParmExt.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encParmExt.iPicWidth = video_max_width;
    encParmExt.iPicHeight = video_max_height;
    encParmExt.iTargetBitrate = video_max_bitrate;
    encParmExt.fMaxFrameRate = 30;
    encParmExt.bEnableDenoise = 1;
    encParmExt.iRCMode = RC_OFF_MODE;
    encParmExt.iComplexityMode = HIGH_COMPLEXITY;

    // only one layer
    encParmExt.iSpatialLayerNum = 1;
    encParmExt.sSpatialLayers[0].iVideoWidth = video_max_width;
    encParmExt.sSpatialLayers[0].iVideoHeight = video_max_height;
    encParmExt.sSpatialLayers[0].fFrameRate = 30;
    encParmExt.sSpatialLayers[0].iSpatialBitrate = video_max_bitrate;
    encParmExt.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
    encParmExt.sSpatialLayers[0].sSliceArgument.uiSliceSizeConstraint = 1300;
    encParmExt.sSpatialLayers[0].iDLayerQp = 24;
    // encParmExt.iMultipleThreadIdc = 1;           // multi-threading not
    // tested for mode SM_SIZELIMITED_SLICE
    encParmExt.uiMaxNalSize = 1300;        // max NAL size must fit in MTU limit

    rv = encoder->InitializeExt(&encParmExt);
    assert(rv == cmResultSuccess);

    int videoFormat = videoFormatI420;        // openH264 only supports I420
    rv = encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);
    assert(rv == cmResultSuccess);

    // set periodic I-frame interval
    auto idrInterval = 64;
    rv = encoder->SetOption(ENCODER_OPTION_IDR_INTERVAL, &idrInterval);
    assert(rv == cmResultSuccess);

    // set up data buffers
    memset(&encodedFrame, 0, sizeof(SFrameBSInfo));

    memset(&inputFrame, 0, sizeof(SSourcePicture));
    inputFrame.iPicWidth = video_max_width;
    inputFrame.iPicHeight = video_max_height;
    inputFrame.iColorFormat = videoFormatI420;
    inputFrame.iStride[0] = inputFrame.iPicWidth;
    inputFrame.iStride[1] = inputFrame.iStride[2] = inputFrame.iPicWidth >> 1;

    logger->info << "H264Encoder Created: w:" << video_max_width
                 << ",h:" << video_max_height << std::flush;
    max_width = video_max_width;
    max_height = video_max_height;
}

H264Encoder::~H264Encoder()
{
    if (encoder)
    {
        long ret = encoder->Uninitialize();
        assert(ret == cmResultSuccess);
        encoder = nullptr;
    }
}

VideoEncoder::EncodedFrameType
H264Encoder::encode(const char *input_buffer,
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
    // static unsigned char uv_array[max_width * max_height] = {0};
    static auto uv_array = (unsigned char *) malloc(stride_uv * stride_y);

    if (stride_uv != max_width || height != max_height)
    {
        logger->warning << "!!!! image dimension change: stride_uv:"
                        << stride_uv << ", height:" << height << std::flush;
    }

    auto now = std::chrono::system_clock::now();
    auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // nv12 to i420 planar
    auto *input = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer));
    auto *p = input + (stride_y * height);        // data buffer of inpt UV
                                                  // plane (Y+Ystride*Yheight)
    int w = width / 2;
    int h = height / 2;            // width,height of input UV plane
                                   // (Ywidth/2,Yheight/2)
    int suv = stride_uv;           // stride of input UV plane (often same as Y)
    int su = stride_uv / 2;        // stride of output U plane (often half of Y)
    int sv = stride_uv / 2;        // stride of output V plane (often half of Y)

    // auto uv = (unsigned char*) malloc(suv*h); // temp buffer to copy input UV
    // plane
    auto uv = uv_array;
    auto *u = p;        // U plane of output (overwrites input)
    auto orig_u = p;
    auto *v = u + su * h;        // V plane of output (overwrites input)
    auto orig_v = orig_u + su * h;
    memcpy(uv, p, suv * h);        // copy input UV plane to temp buffer

    // de-interleave UV plane
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            // copy the even bytes to U place
            u[x] = uv[x * 2];
            // copy the odd bytes to V plane
            v[x] = uv[x * 2 + 1];
        }
        u += su;
        v += sv;
        uv += suv;
    }

    output_bitstream.clear();

    SSourcePicture sourcePicture;
    memset(&sourcePicture, 0, sizeof(sourcePicture));
    sourcePicture.iColorFormat = videoFormatI420;
    sourcePicture.iPicHeight = (int) height;
    sourcePicture.iPicWidth = (int) width;
    sourcePicture.uiTimeStamp = timestamp;
    sourcePicture.pData[0] = reinterpret_cast<unsigned char *>(
        const_cast<char *>(input_buffer));
    sourcePicture.pData[1] = orig_u;
    sourcePicture.pData[2] = orig_v;
    sourcePicture.iStride[0] = (int) stride_y;
    sourcePicture.iStride[1] = (int) su;
    sourcePicture.iStride[2] = (int) sv;

    bool idr_frame = total_frames_encoded & 63 ? false : true;
    if (genKeyFrame || idr_frame)
    {
        logger->debug << "h264Encoder:: Force IDR, total_frames_encoded: "
                     << total_frames_encoded << std::flush;
        auto ret = encoder->ForceIntraFrame(true);
        logger->debug << "h264Encoder:: IDR Frame Generation Result " << ret
                      << std::flush;
    }

    memset(&encodedFrame, 0, sizeof(SFrameBSInfo));
    int ret = encoder->EncodeFrame(&sourcePicture, &encodedFrame);
    if (ret == 0)
    {
        switch (encodedFrame.eFrameType)
        {
            case videoFrameTypeSkip:
                logger->info << "h264Encoder:: videoFrameTypeSkip"
                             << std::flush;
                return VideoEncoder::EncodedFrameType::Skip;
            case videoFrameTypeInvalid:
                logger->info << "h264Encoder: failed: " << ret << std::flush;
                return VideoEncoder::EncodedFrameType::Invalid;
        }
    }

    // encode worked
    if (debug)
    {
        logger->debug << "Encoded iFrameSizeInBytes: "
                      << encodedFrame.iFrameSizeInBytes << std::flush;
        logger->debug << "Encoded num_layer: " << encodedFrame.iLayerNum
                      << std::flush;
        logger->debug << "Frame Type: " << encodedFrame.eFrameType
                      << std::flush;
    }

    // move the encoded data to output bitstream
    output_bitstream.resize(encodedFrame.iFrameSizeInBytes);
    unsigned char *out_bits = output_bitstream.data();
    for (int i = 0; i < encodedFrame.iLayerNum; i++)
    {
        auto len = 0;
        // pNalLengthInByte[0]+pNalLengthInByte[1]+…+pNalLengthInByte[iNalCount-1].
        for (int j = 0; j < encodedFrame.sLayerInfo[i].iNalCount; j++)
        {
            len += encodedFrame.sLayerInfo[i].pNalLengthInByte[j];
        }

        memcpy(out_bits, encodedFrame.sLayerInfo[i].pBsBuf, len);

        out_bits += len;
    }

    total_bytes_encoded += output_bitstream.size();
    total_frames_encoded++;

    auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto diff = end_ms - start_ms;
    logger->debug << "h264Encoder: encode delta:" << diff << std::flush;

    MetricsReporter::Report(stream_id,
                            MediaType::video,
                            metrics::MeasurementType::EncodeTime,
                            diff);

    // success
    return toEncodedFrameType(encodedFrame.eFrameType);
}
