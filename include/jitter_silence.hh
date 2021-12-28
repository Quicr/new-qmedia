#pragma once

#include <cmath>
#include <algorithm>

namespace neo_media
{
class AudioLevel
{
public:
    void init(int bufferSize, int samplerate);
    void findLevelAndUpdate(int nchannels,
                            float *AbsLevel,
                            const float *data,
                            bool consecutiveData,
                            const float *data_ch2);

    float getSNR();
    float getSignalLevel();
    float getNoiseLevel();
    unsigned int getNumUpdates();

private:
    const float sign_attack = 0.001f;
    const float sign_decay = 0.1f;
    const float noise_attack = 50.0f;
    const float noise_decay = 0.03f;
    const float sig_min = 5.0e-5f;

    float sigAbsLev;
    float noiseAbsLev;
    float signalAttackRate;
    float signalDecayRate;
    float noiseAttackRate;
    float noiseDecayRate;
    float InvBufLength;
    int buflen;
    unsigned int numUpdates;

    void updateSigAndNoise(float currinlevel);
    void findLevel(float *AbsLevel,
                   int nchannels,
                   const float *data,
                   bool consecutiveData,
                   const float *data_ch2);
    float findAbsLevel(const float *buf, int buflen);
    float sp_vec_sum_abs(const float *x, int len);
    float sp_vec_mean_abs(const float *x, int len);
};

class Silence
{
public:
    bool isInited();
    void init(int bufferSize, int samplerate_, int channels_);
    void updateSilenceDetection(const float *buffer);
    bool isSilence();

private:
    const float skew_drop_snr_threshold = 2.3f;
    float absLevel[2];        // max 2 channels
    int channels;
    AudioLevel level;
    bool inited = false;
};

}        // namespace neo_media