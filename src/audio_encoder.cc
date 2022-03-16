#include <cstring>        // memeset

#include "audio_encoder.hh"

using namespace neo_media;

AudioEncoder::AudioEncoder(unsigned int audio_sample_rate,
                           int audio_channels,
                           Packet::MediaType audio_type,
                           frameReadyCallback callback,
                           uint64_t sourceID,
                           const LoggerPointer &logger) :
    sample_rate(audio_sample_rate),
    channels(audio_channels),
    frameReady(callback),
    sID(sourceID),
    logger(logger)
{
    initOpus();
    output_samples = 480 * channels;
    type = audio_type;

    switch (type)
    {
        case Packet::MediaType::L16:
            output_bytes = output_samples * sizeof(uint16_t);
            buffers.sample_divisor = channels * sizeof(uint16_t);
            break;
        case Packet::MediaType ::F32:
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

void AudioEncoder::encodeFrame(const char *buffer,
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
        buffers.addBuffer((const uint8_t *) buffer, length, timestamp);

    if (frameReady != nullptr)
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
            // std::lock_guard<std::mutex> lock(buffer_lock);
            pop_and_encode();
            immediate_encode = false;
        }
    }
}

PacketPointer AudioEncoder::createPacket(const std::vector<uint8_t> &data,
                                         std::uint64_t timestamp)
{
    PacketPointer packet = std::make_unique<Packet>();
    unsigned char encodedAudio[1500];
    int encodedAudioLen = 0;
    {
        switch (type)
        {
            case Packet::MediaType::L16:
                encodedAudioLen = opus_encode(
                    encoder,
                    (const opus_int16 *) data.data(),
                    data.size() / channels / sizeof(uint16_t),
                    encodedAudio,
                    sizeof(encodedAudio));
                break;
            case Packet::MediaType::F32:
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
    packet->data.resize(0);
    packet->data.reserve(encodedAudioLen);
    copy(&encodedAudio[0],
         &encodedAudio[encodedAudioLen],
         back_inserter(packet->data));

    packet->mediaType = Packet::MediaType::Opus;
    packet->sourceID = sID;
    packet->sourceRecordTime = timestamp;
    return packet;
}

void AudioEncoder::pop_and_encode()
{
    PacketPointer packet = nullptr;

    while (buffers.getTotalInBuffers() >= output_bytes)
    {
        std::vector<uint8_t> fill_buffer;
        unsigned int fill_length = output_bytes;
        uint64_t timestamp;
        if (buffers.fill(fill_buffer, fill_length, timestamp))
        {
            packet = createPacket(fill_buffer, timestamp);
            if (packet != nullptr)
            {
                frameReady(std::move(packet));
            }
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
