#pragma once
#include <mutex>
#include "opus.h"
#include "packet.hh"

namespace qmedia
{
class OpusAssembler
{
public:
    explicit OpusAssembler(Packet::MediaType type);
    OpusAssembler(Packet::MediaType type,
                  Packet::MediaType decodeAs,
                  unsigned int decode_audio_channels,
                  unsigned int audio_sample_rate);
    PacketPointer push(PacketPointer packet);
    PacketPointer opusCreatePLC(const std::size_t &data_length);

protected:
    OpusDecoder *decOpus;
    std::mutex decOpusMutex;
    Packet::MediaType type;
    Packet::MediaType audio_decode_as;
    unsigned int audio_channels;
    unsigned int audio_sample_rate;
    void decodeMedia(Packet *packet);
};
}        // namespace qmedia