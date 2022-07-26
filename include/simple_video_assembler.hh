#pragma once

#include <map>
#include "frame_assembler.hh"
#include "modulus_deque.hh"

namespace neo_media
{
class SimpleVideoAssembler : public FrameAssembler
{
    typedef ModulusDeque<std::uint32_t, PacketPointer> Depacketizer;
    typedef std::shared_ptr<Depacketizer> DepacketizerPointer;

public:
    PacketPointer push(PacketPointer packet) override;

protected:
    std::map<std::uint64_t, DepacketizerPointer> deques;
    void decodeMedia(Packet *packet) override;
};
}        // namespace neo_media
