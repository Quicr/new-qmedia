#pragma once

#include "packet.hh"

namespace neo_media
{
class FrameAssembler
{
public:
    virtual PacketPointer push(PacketPointer packet) = 0;

protected:
    virtual void decodeMedia(Packet *packet) = 0;
};
}        // namespace neo_media