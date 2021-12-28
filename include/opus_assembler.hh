#pragma once
#include <mutex>
#include "frame_assembler.hh"
#include "opus.h"

namespace neo_media
{
class OpusAssembler : public FrameAssembler
{
public:
    explicit OpusAssembler(Packet::MediaType type);
    OpusAssembler(Packet::MediaType type,
                  Packet::MediaType decodeAs,
                  unsigned int decode_audio_channels,
                  unsigned int audio_sample_rate);
    PacketPointer push(PacketPointer packet) override;
    PacketPointer opusCreatePLC(const std::size_t &data_length);

protected:
    OpusDecoder *decOpus;
    std::mutex decOpusMutex;
    Packet::MediaType type;
    Packet::MediaType audio_decode_as;
    unsigned int audio_channels;
    unsigned int audio_sample_rate;
    void decodeMedia(Packet *packet) override;
};
}        // namespace neo_media