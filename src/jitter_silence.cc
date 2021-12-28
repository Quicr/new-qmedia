#include "jitter_silence.hh"
#include <assert.h>

using namespace neo_media;

void Silence::init(int buffersize_, int samplerate_, int channels_)
{
    channels = channels_;
    level.init(buffersize_, samplerate_);
    inited = true;
}

bool Silence::isInited()
{
    return inited;
}

void Silence::updateSilenceDetection(const float *buffer)
{
    level.findLevelAndUpdate(channels, absLevel, buffer, true, nullptr);
}

bool Silence::isSilence()
{
    bool hasConverged = level.getNumUpdates() > 20;
    float snr = level.getSNR();

    if (hasConverged && snr < skew_drop_snr_threshold) return true;

    return false;
}

void AudioLevel::init(int buffersize, int samplerate)
{
    numUpdates = 0;
    buflen = buffersize;
    InvBufLength = 1.0f / ((float) buffersize);
    sigAbsLev = 0.0f;
    noiseAbsLev = 0.0f;
    signalAttackRate = 1 - expf(-1.0f / sign_attack * buffersize / samplerate);
    signalDecayRate = 1 - expf(-1.0f / sign_decay * buffersize / samplerate);
    noiseAttackRate = 1 - expf(-1.0f / noise_attack * buffersize / samplerate);
    noiseDecayRate = 1 - expf(-1.0f / noise_decay * buffersize / samplerate);
}

void AudioLevel::updateSigAndNoise(float currinlevel)
{
    if (sigAbsLev < currinlevel)
        sigAbsLev += signalAttackRate * (currinlevel - sigAbsLev);
    else
        sigAbsLev += signalDecayRate * (currinlevel - sigAbsLev);

    sigAbsLev = std::max(sig_min, sigAbsLev);

    if (noiseAbsLev < currinlevel)
        noiseAbsLev += noiseAttackRate * (currinlevel - noiseAbsLev);
    else
        noiseAbsLev += noiseDecayRate * (currinlevel - noiseAbsLev);

    noiseAbsLev = std::max(sig_min, noiseAbsLev);

    numUpdates++;
}

void AudioLevel::findLevel(float *AbsLevel,
                           int nchannels,
                           const float *data,
                           bool consecutiveData,
                           const float *data_ch2)
{
    int i;
    const float *bufstart;

    assert(nchannels == 1 || nchannels == 2);

    for (i = 0; i < nchannels; i++)
    {
        if (!consecutiveData)
        {
            if (i == 0)
                bufstart = data;
            else
                bufstart = data_ch2;
        }
        else
            bufstart = data + i * buflen; /* points to start of channel-data.
                                             (L(/R)) */

        AbsLevel[i] = findAbsLevel(bufstart, buflen);
    }
}

/*
 * Is only used for debug/tracing (finding level of a single buffer)
 * and internal use
 */
float AudioLevel::findAbsLevel(const float *buf, int buflen)
{
    return sp_vec_mean_abs(buf, buflen);
}

/*
 * Find abslevel and update signal and noise levels in struct.
 * NB! Must only be called once for each frame, else the update of levels will
 * become wrong.
 */
void AudioLevel::findLevelAndUpdate(int nchannels,
                                    float *AbsLevel,
                                    const float *data,
                                    bool consecutiveData,
                                    const float *data_ch2)
{
    float absLevel;
    findLevel(AbsLevel, nchannels, data, consecutiveData, data_ch2);

    absLevel = AbsLevel[0];        //  mono
    if (nchannels == 2)            // stereo
        absLevel = AbsLevel[0] + AbsLevel[1];
    updateSigAndNoise(absLevel);
}

float AudioLevel::getSNR()
{
    if (noiseAbsLev == 0.0f)
        return 0.0f;
    else
        return (sigAbsLev / noiseAbsLev);
}

float AudioLevel::getSignalLevel()
{
    return sigAbsLev;
}

float AudioLevel::getNoiseLevel()
{
    return noiseAbsLev;
}

unsigned int AudioLevel::getNumUpdates()
{
    return numUpdates;
}

float AudioLevel::sp_vec_sum_abs(const float *x, int len)
{
    float f1 = 0.0f, f2 = 0.0f;
    for (; len >= 2; len -= 2)
    {
        f1 += fabsf(*x++);
        f2 += fabsf(*x++);
    }
    if (len > 0) f1 += fabsf(*x);
    return f1 + f2;
}
float AudioLevel::sp_vec_mean_abs(const float *x, int len)
{
    float fp = sp_vec_sum_abs(x, len);
    return fp / (float) len;
}