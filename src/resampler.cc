#include <iostream>
#include <utility>
#include "resampler.hh"

namespace neo_media
{
void Resampler::resample(Packet *packet, unsigned int audio_channels)
{
    resample(packet, audio_channels, audio_resample_ratio);
}

void Resampler::resample(Packet *packet,
                         unsigned int audio_channels,
                         double src_ratio)
{
    switch (packet->mediaType)
    {
        case Packet::MediaType::F32:
            resampleFloat32(packet, audio_channels, src_ratio);
            break;
        case Packet::MediaType::L16:
            resampleL16(packet, audio_channels, src_ratio);
            break;
        default:
            std::clog << "Can't resample media format:"
                      << (int) packet->mediaType << std::endl;
            break;
    }
}

void Resampler::resampleFloat32(Packet *packet,
                                int audio_channels,
                                double src_ratio)
{
    auto *src = (float *) &packet->data[0];
    long total_input_frames = packet->data.size() / sizeof(float);

    long total_output_frames = total_input_frames * src_ratio;
    auto *resampled_data = new float[total_output_frames];

    int ret = simple_resample(src,
                              total_input_frames,
                              resampled_data,
                              total_output_frames,
                              audio_channels,
                              src_ratio);
    if (ret)
    {
        std::clog << "Resample failed: " << ret << std::endl;
    }
    else
    {
        int length_bytes = total_output_frames * sizeof(float);
        packet->data.resize(0);
        packet->data.reserve(length_bytes);
        uint8_t *bytes = (uint8_t *) &resampled_data[0];
        copy(bytes, bytes + length_bytes, back_inserter(packet->data));
    }
    delete[] resampled_data;
}

void Resampler::resampleL16(Packet *packet,
                            int audio_channels,
                            double src_ratio)
{
    long total_input_frames = packet->data.size() / sizeof(uint16_t);
    auto *src_data = new float[total_input_frames];
    src_short_to_float_array(
        (short *) &packet->data[0], src_data, total_input_frames);

    long total_output_frames = total_input_frames * src_ratio;
    auto *resampled_data = new float[total_output_frames];

    int ret = simple_resample(src_data,
                              total_input_frames,
                              resampled_data,
                              total_output_frames,
                              audio_channels,
                              src_ratio);
    if (ret)
    {
        std::clog << "Resample failed: " << src_strerror(ret) << std::endl;
    }
    else
    {
        int length_bytes = total_output_frames * sizeof(uint16_t);
        auto *L16_data = new short[total_output_frames];
        src_float_to_short_array(resampled_data, L16_data, total_output_frames);
        packet->data.resize(0);
        packet->data.reserve(length_bytes);
        copy(
            &L16_data[0], &L16_data[length_bytes], back_inserter(packet->data));
    }
    delete[] resampled_data;
}

int Resampler::simple_resample(const float *src,
                               long total_input_frames,
                               float *dst,
                               long total_output_frames,
                               int audio_channels,
                               double src_ratio)
{
    SRC_DATA src_data;

    src_data.data_in = src;
    src_data.input_frames = total_input_frames / audio_channels;

    src_data.data_out = dst;
    src_data.output_frames = total_output_frames / audio_channels;

    src_data.src_ratio = src_ratio;
    src_data.end_of_input = 0;

    return src_simple(&src_data, SRC_SINC_BEST_QUALITY, audio_channels);
}

float Resampler::getMaxUpSample() const
{
    return max_upsample_distortion_ratio;
}

float Resampler::getMaxDownSample() const
{
    return max_downsample_distortion_ratio;
}

void Resampler::setResampleRatio(float ratio)
{
    float new_ratio = ratio;

    if (new_ratio > max_upsample_distortion_ratio)
        new_ratio = max_upsample_distortion_ratio;
    else if (new_ratio < max_downsample_distortion_ratio)
        new_ratio = max_downsample_distortion_ratio;

    audio_resample_ratio = new_ratio;
}

}        // namespace neo_media
