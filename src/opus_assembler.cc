#include <iostream>
#include <cassert>
#include <cstring>
#include "opus_assembler.hh"

using namespace neo_media;

OpusAssembler::OpusAssembler(Packet::MediaType stream_type) : type(stream_type)
{
    // for unit tests not running actual decoder
    audio_decode_as = Packet::MediaType::Bad;
}

OpusAssembler::OpusAssembler(Packet::MediaType stream_type,
                             Packet::MediaType decodeAs,
                             unsigned int channels,
                             unsigned int sample_rate) :
    type(stream_type),
    audio_decode_as(decodeAs),
    audio_channels(channels),
    audio_sample_rate(sample_rate)
{
    if (stream_type == Packet::MediaType::Opus)
    {
        std::lock_guard<std::mutex> lock(decOpusMutex);
        int opusErr;
        decOpus = opus_decoder_create(
            audio_sample_rate, audio_channels, &opusErr);
        if (opusErr != OPUS_OK)
        {
            std::cerr << "opus decoder create error:  "
                      << opus_strerror(opusErr) << std::endl;
        }
    }
}

PacketPointer OpusAssembler::push(PacketPointer packet)
{
    // TODO: Do we want to support raw L16 or F32 here?
    assert(packet->mediaType == Packet::MediaType::Opus);
    decodeMedia(packet.get());
    return packet;
}

void OpusAssembler::decodeMedia(Packet *packet)
{
    assert(packet);

    unsigned char decodedAudio[3840];
    int decodedAudioLen = 0;

    {
        std::lock_guard<std::mutex> lock(decOpusMutex);

        switch (audio_decode_as)
        {
            case Packet::MediaType::L16:
                decodedAudioLen = opus_decode(
                    decOpus,
                    (unsigned char *) packet->data.data(),
                    packet->data.size(),
                    (opus_int16 *) decodedAudio,
                    sizeof(decodedAudio) / sizeof(uint16_t) / audio_channels,
                    0 /* FEC */);
                decodedAudioLen *= sizeof(uint16_t) * audio_channels;
                break;
            case Packet::MediaType::F32:
                decodedAudioLen = opus_decode_float(
                    decOpus,
                    (unsigned char *) packet->data.data(),
                    packet->data.size(),
                    (float *) decodedAudio,
                    sizeof(decodedAudio) / sizeof(float) / audio_channels,
                    0 /* FEC */);
                decodedAudioLen *= sizeof(float) * audio_channels;
                break;
            default:
                break;
        }
    }

    if (decodedAudioLen <= 0)
    {
        std::clog << "opus decode failed: " << opus_strerror(decodedAudioLen)
                  << std::endl;
        return;
    }

    packet->data.resize(0);
    packet->data.reserve(decodedAudioLen);
    copy(&decodedAudio[0],
         &decodedAudio[decodedAudioLen],
         back_inserter(packet->data));

    packet->mediaType = audio_decode_as;
}

PacketPointer OpusAssembler::opusCreatePLC(const std::size_t &data_length)
{
    unsigned char *decodedAudio = new unsigned char[data_length];
    memset(decodedAudio, 0, data_length);
    int decodedAudioLen = 0;
    {
        std::lock_guard<std::mutex> lock(decOpusMutex);

        switch (audio_decode_as)
        {
            case Packet::MediaType::L16:
                decodedAudioLen = opus_decode(
                    decOpus,
                    NULL,
                    0,
                    (opus_int16 *) decodedAudio,
                    data_length / sizeof(uint16_t) / audio_channels,
                    0 /* FEC */);
                decodedAudioLen *= sizeof(uint16_t) * audio_channels;
                break;
            case Packet::MediaType::F32:
                decodedAudioLen = opus_decode_float(
                    decOpus,
                    NULL,
                    0,
                    (float *) decodedAudio,
                    data_length / sizeof(float) / audio_channels,
                    0 /* FEC */);
                decodedAudioLen *= sizeof(float) * audio_channels;
                break;
            default:
                break;
        }

        if (audio_decode_as != Packet::MediaType::Bad && decodedAudioLen <= 0)
        {
            std::clog << "opus decode failed: "
                      << opus_strerror(decodedAudioLen) << std::endl;
            return nullptr;
        }
    }
    PacketPointer packet = std::make_unique<Packet>();
    packet->data.resize(0);
    packet->data.reserve(decodedAudioLen);
    copy(&decodedAudio[0],
         &decodedAudio[decodedAudioLen],
         back_inserter(packet->data));
    delete[] decodedAudio;
    packet->mediaType = audio_decode_as;
    return packet;
}
