#include "simple_packetize.hh"

SimplePacketize::SimplePacketize(PacketPointer packet,
                                 std::size_t max_packet_size)
{
    original_packet = std::move(packet);
    original_packet_size = original_packet->data.size();
    packet_count = std::ceil(original_packet->data.size() * 1.0 /
                             max_packet_size);
    this->max_packet_size = max_packet_size;
}

SimplePacketize::~SimplePacketize()
{
}

PacketPointer SimplePacketize::GetPacket(std::uint16_t packet_index)
{
    if (packet_index >= GetPacketCount())
    {
        return nullptr;
    }

    PacketPointer packet = std::make_unique<Packet>();
    packet->clientID = original_packet->clientID;
    packet->sourceID = original_packet->sourceID;
    packet->conferenceID = original_packet->conferenceID;
    packet->sourceRecordTime = original_packet->sourceRecordTime;
    packet->packetType = original_packet->packetType;
    packet->encodedSequenceNum = original_packet->encodedSequenceNum;
    packet->transportSequenceNumber = original_packet->transportSequenceNumber;
    packet->mediaType = original_packet->mediaType;
    packet->videoFrameType = original_packet->videoFrameType;

    if (GetPacketCount() == 1)
    {
        packet->packetizeType = Packet::PacketizeType::None;
    }
    else
    {
        packet->packetizeType = Packet::PacketizeType::Simple;
    }
    packet->fragmentCount = GetPacketCount();
    packet->chunkFragmentNum = packet_index;
    packet->frameSize = original_packet->data.size();

    // Does the new packet require the full size?
    if (packet->frameSize < max_packet_size * (packet_index + 1))
    {
        std::size_t packet_length = packet->frameSize -
                                    max_packet_size * packet_index;

        // attempt to not copy the data from the original packet.
        packet->data = std::vector<std::uint8_t>(
            original_packet->data.begin() + packet_index * max_packet_size,
            original_packet->data.begin() + packet_index * max_packet_size +
                packet_length);
    }
    else
    {
        // attempt to not copy the data from the original packet.
        packet->data = std::vector<std::uint8_t>(
            original_packet->data.begin() + packet_index * max_packet_size,
            original_packet->data.begin() +
                (packet_index + 1) * max_packet_size);
    }
    return packet;
}
