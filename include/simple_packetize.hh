#pragma once

#include <iostream>
#include <cmath>

#include "packet.hh"

using namespace neo_media;

class SimplePacketize
{
public:
    SimplePacketize(PacketPointer packet, std::size_t max_packet_size = 1200);
    ~SimplePacketize();

    std::size_t GetPacketCount() { return packet_count; }
    PacketPointer GetPacket(std::uint16_t packet_index);

protected:
    PacketPointer original_packet;
    std::size_t max_packet_size;
    std::uint64_t original_packet_size;
    std::uint64_t packet_count;
};
