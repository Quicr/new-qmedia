#pragma once
#include <samplerate.h>
#include "packet.hh"

namespace neo_media
{
class Resampler
{
public:
    Resampler() : audio_resample_ratio(1.0){};

    const float max_upsample_distortion_ratio = 1.15F;
    const float max_downsample_distortion_ratio = 0.85F;
    float audio_resample_ratio;
    [[nodiscard]] float getMaxUpSample() const;
    [[nodiscard]] float getMaxDownSample() const;
    void setResampleRatio(float ratio);

    void resample(Packet *packet, unsigned int audio_channels);
    void resample(Packet *packet,
                  unsigned int audio_channels,
                  double src_ratio);
    void resampleFloat32(Packet *packet, int audio_channels, double src_ratio);
    void resampleL16(Packet *packet, int audio_channels, double src_ratio);
    int simple_resample(const float *src,
                        long total_input_frames,
                        float *dst,
                        long total_output_frames,
                        int audio_channels,
                        double src_ratio);
};

}        // namespace neo_media