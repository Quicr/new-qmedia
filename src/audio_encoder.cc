#include <cstring>        // memeset

#include <qmedia/media_client.hh>
#include "audio_encoder.hh"

namespace qmedia
{

AudioEncoder::AudioEncoder(unsigned int audio_sample_rate,
                           int audio_channels,
                           AudioConfig::SampleType audio_type,
                           frameReadyCallback callback,
                           MediaStreamId stream,
                           const LoggerPointer &logger) :
    sample_rate(audio_sample_rate),
    channels(audio_channels),
    encoded_frame_ready(callback),
    stream_id(stream),
    logger(logger)
{
    initOpus();
    output_samples = 480 * channels;
    type = audio_type;

    switch (type)
    {
        case AudioConfig::SampleType::PCMint16:
            output_bytes = output_samples * sizeof(uint16_t);
            buffers.sample_divisor = channels * sizeof(uint16_t);
            break;
        case AudioConfig::SampleType::Float32:
            output_bytes = output_samples * sizeof(float);
            buffers.sample_divisor = channels * sizeof(uint16_t);
            break;
        default:
            break;
    }

    if (callback != nullptr)
    {
        encode_thread = std::thread(&AudioEncoder::encoderWerk, this);
        encode_thread.detach();
    }
}

AudioEncoder::~AudioEncoder()
{
    if (encode_thread.joinable())
    {
        shutdown = true;
        encode_thread.join();
    }
}

void AudioEncoder::encodeFrame(const uint8_t *buffer,
                               unsigned int length,
                               std::uint64_t timestamp,
                               bool encodeEmpty)
{
    if (encodeEmpty)
    {
        auto *empty_buff = new uint8_t[length];
        memset(empty_buff, 0, length);
        buffers.addBuffer(empty_buff, length, timestamp);
        delete[] empty_buff;
    }
    else
    {
        buffers.addBuffer((const uint8_t *) buffer, length, timestamp);
    }

    if (encoded_frame_ready != nullptr)
    {
        immediate_encode = true;
        cv.notify_all();
    }
}

void AudioEncoder::encoderWerk()
{
    while (!shutdown)
    {
        std::unique_lock<std::mutex> ulock(enc_lock);
        cv.wait(ulock, [&]() -> bool { return shutdown || immediate_encode; });
        if (immediate_encode)
        {
            pop_and_encode();
            immediate_encode = false;
        }
    }
}

std::vector<uint8_t> AudioEncoder::encode(const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> output;
    unsigned char encodedAudio[1500];
    int encodedAudioLen = 0;
    {
        switch (type)
        {
            case AudioConfig::SampleType::PCMint16:
                encodedAudioLen = opus_encode(
                    encoder,
                    (const opus_int16 *) data.data(),
                    data.size() / channels / sizeof(uint16_t),
                    encodedAudio,
                    sizeof(encodedAudio));
                break;
            case AudioConfig::SampleType::Float32:
                encodedAudioLen = opus_encode_float(
                    encoder,
                    (const float *) data.data(),
                    data.size() / channels / sizeof(float),
                    encodedAudio,
                    sizeof(encodedAudio));
                break;
            default:
                logger->error << "audio encoder create packet - unknown media "
                                 "format"
                              << std::flush;
        }

        if (encodedAudioLen < 0)
        {
            logger->error << "opus encode error: " << encodedAudioLen
                          << std::flush;
        }
    }
    output.resize(0);
    output.reserve(encodedAudioLen);
    copy(&encodedAudio[0],
         &encodedAudio[encodedAudioLen],
         back_inserter(output));
    return output;
}

void AudioEncoder::pop_and_encode()
{
    while (buffers.getTotalInBuffers(logger) >= output_bytes)
    {
        std::vector<uint8_t> fill_buffer;
        unsigned int fill_length = output_bytes;
        uint64_t timestamp;
        if (buffers.fill(logger, fill_buffer, fill_length, timestamp))
        {
            auto output = encode(fill_buffer);
            encoded_frame_ready(std::move(output), timestamp);
        }
    }
}

void AudioEncoder::initOpus()
{
    int opusErr;
    encoder = opus_encoder_create(
        sample_rate, channels, OPUS_APPLICATION_VOIP, &opusErr);
    if (opusErr != OPUS_OK)
    {
        logger->error << "opus encoder create error: " << opusErr << std::flush;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(40000));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(6));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_APPLICATION_AUDIO));
}

}        // namespace qmedia