#include <doctest/doctest.h>
#include <cstring>

#include "../src/packet.hh"
#include "simple_packetize.hh"

using namespace neo_media;

PacketPointer
GeneratePacket(std::size_t packet_size = 12111,
               std::uint64_t client_id = 1001,
               std::uint64_t source_id = 2002,
               std::uint64_t timestamp = 3003,
               std::uint64_t conference_id = 4004,
               Packet::Type packet_type = Packet::Type::Unknown,
               Packet::MediaType media_type = Packet::MediaType::Bad)
{
    auto packet = std::make_unique<Packet>();
    packet->data.resize(packet_size);
    memset(packet->data.data(), 0, packet->data.size());
    packet->clientID = client_id;
    packet->sourceID = source_id;
    packet->sourceRecordTime = timestamp;
    packet->conferenceID = conference_id;
    packet->packetType = packet_type;
    packet->mediaType = media_type;
    return packet;
}

TEST_CASE("SimplePacketization::GetPacketCount -- Small Packet")
{
    auto packet = GeneratePacket(1000, 1, 2, 3, 4);
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1200);
    CHECK_EQ(packets->GetPacketCount(), 1);
}

TEST_CASE("SimplePacketization::GetPacket -- Small Packet")
{
    auto packet = GeneratePacket(
        1000, 1, 2, 3, 4, Packet::Type::StreamContent, Packet::MediaType::Raw);
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1200);

    // packet
    CHECK_EQ(packets->GetPacket(0)->data.size(), 1000);
    CHECK_EQ(packets->GetPacket(0)->clientID, 1);
    CHECK_EQ(packets->GetPacket(0)->sourceID, 2);
    CHECK_EQ(packets->GetPacket(0)->sourceRecordTime, 3);
    CHECK_EQ(packets->GetPacket(0)->conferenceID, 4);
    CHECK_EQ(packets->GetPacket(0)->fragmentCount, 1);
    CHECK_EQ(packets->GetPacket(0)->chunkFragmentNum, 0);
    CHECK_EQ(packets->GetPacket(0)->frameSize, 1000);
    CHECK_EQ(packets->GetPacket(0)->packetizeType, Packet::PacketizeType::None);
    CHECK_EQ(packets->GetPacket(0)->packetType, Packet::Type::StreamContent);
    CHECK_EQ(packets->GetPacket(0)->mediaType, Packet::MediaType::Raw);

    // packet out of range
    CHECK_FALSE(packets->GetPacket(-1));

    // packet out of range
    CHECK_FALSE(packets->GetPacket(1));
}

TEST_CASE("SimplePacketization::GetPacketCount -- Large Packet")
{
    auto packet = GeneratePacket();
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1200);
    CHECK_EQ(packets->GetPacketCount(), 11);
}

TEST_CASE("SimplePacketization::GetPacket -- Large Packet")
{
    auto packet = GeneratePacket(
        12111, 1, 2, 3, 4, Packet::Type::StreamContent, Packet::MediaType::Raw);
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1200);

    // first packet
    CHECK_EQ(packets->GetPacket(0)->data.size(), 1200);
    CHECK_EQ(packets->GetPacket(0)->clientID, 1);
    CHECK_EQ(packets->GetPacket(0)->sourceID, 2);
    CHECK_EQ(packets->GetPacket(0)->sourceRecordTime, 3);
    CHECK_EQ(packets->GetPacket(0)->conferenceID, 4);
    CHECK_EQ(packets->GetPacket(0)->fragmentCount, 11);
    CHECK_EQ(packets->GetPacket(0)->chunkFragmentNum, 0);
    CHECK_EQ(packets->GetPacket(0)->frameSize, 12111);
    CHECK_EQ(packets->GetPacket(0)->packetizeType,
             Packet::PacketizeType::Simple);
    CHECK_EQ(packets->GetPacket(0)->packetType, Packet::Type::StreamContent);
    CHECK_EQ(packets->GetPacket(0)->mediaType, Packet::MediaType::Raw);

    // last packet
    CHECK_EQ(packets->GetPacket(10)->data.size(), 111);
    CHECK_EQ(packets->GetPacket(10)->clientID, 1);
    CHECK_EQ(packets->GetPacket(10)->sourceID, 2);
    CHECK_EQ(packets->GetPacket(10)->sourceRecordTime, 3);
    CHECK_EQ(packets->GetPacket(10)->conferenceID, 4);
    CHECK_EQ(packets->GetPacket(10)->fragmentCount, 11);
    CHECK_EQ(packets->GetPacket(10)->chunkFragmentNum, 10);
    CHECK_EQ(packets->GetPacket(10)->frameSize, 12111);
    CHECK_EQ(packets->GetPacket(10)->packetizeType,
             Packet::PacketizeType::Simple);
    CHECK_EQ(packets->GetPacket(10)->packetType, Packet::Type::StreamContent);
    CHECK_EQ(packets->GetPacket(10)->mediaType, Packet::MediaType::Raw);

    // packet out of range
    CHECK_FALSE(packets->GetPacket(-1));

    // packet out of range
    CHECK_FALSE(packets->GetPacket(11));
}

TEST_CASE("SimplePacketize should not create a packet with data of size 0.")
{
    auto packet = GeneratePacket(2000);
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1000);
    CHECK_EQ(packets->GetPacketCount(), 2);
    CHECK_EQ(packets->GetPacket(0)->data.size(), 1000);
    CHECK_EQ(packets->GetPacket(1)->data.size(), 1000);
}

TEST_CASE("SimplePacketize should create an extra packets when just one byte "
          "over")
{
    auto packet = GeneratePacket(2001);
    auto packets = std::make_unique<SimplePacketize>(std::move(packet), 1000);
    CHECK_EQ(packets->GetPacketCount(), 3);
    CHECK_EQ(packets->GetPacket(0)->data.size(), 1000);
    CHECK_EQ(packets->GetPacket(1)->data.size(), 1000);
    CHECK_EQ(packets->GetPacket(2)->data.size(), 1);
}