#include <doctest/doctest.h>
#include "simple_video_assembler.hh"
#include "packet.hh"
#include <iostream>
#include "simple_packetize.hh"

using namespace neo_media;

PacketPointer Gen(std::string data, std::uint64_t ts, int num, int count)
{
    auto packet = std::make_unique<Packet>();
    packet->sourceRecordTime = ts;
    packet->chunkFragmentNum = num;
    packet->fragmentCount = count;
    packet->data.assign(data.begin(), data.end());
    packet->frameSize = packet->data.size();
    return packet;
}

TEST_CASE("SimplePacketization::Push -- Test Packet")
{
    uint64_t ts = 1234;
    auto packet1 = Gen("foo", ts, 0, 3);
    auto packet2 = Gen("bar", ts, 1, 3);
    auto packet3 = Gen("baz", ts, 2, 3);

    auto assembler = std::make_unique<SimpleVideoAssembler>();
    PacketPointer result;
    result = assembler->push(std::move(packet1));
    CHECK_EQ(result, nullptr);
    result = assembler->push(std::move(packet2));
    CHECK_EQ(result, nullptr);
    result = assembler->push(std::move(packet3));
    CHECK_FALSE(result == nullptr);
    CHECK_EQ(std::string(result->data.begin(), result->data.end()),
             "foobarbaz");
}

TEST_CASE("SimplePacketization and SimpleAssembler should have matching input "
          "and output")
{
    // made up values for packets.
    std::uint64_t ts = 1234;
    std::uint64_t client_id = 55;
    std::uint64_t source_id = 0;
    std::uint64_t conference_id = 123;
    std::size_t image_width = 1920;
    std::size_t image_height = 1080;
    std::size_t image_size = (image_width * image_height * 12) / 8;
    bool all_good = true;

    // create the packet
    PacketPointer packet = std::make_unique<Packet>();
    packet->sourceRecordTime = ts;
    packet->chunkFragmentNum = 0;
    packet->fragmentCount = 1;
    packet->clientID = client_id;
    packet->sourceID = source_id;
    packet->conferenceID = conference_id;
    packet->data.resize(image_size);

    // create a fake image that we can test the values afterwards.
    for (std::uint64_t i = 0; i < image_size; i++)
    {
        packet->data[i] = static_cast<std::uint8_t>(i % 127);
    }
    packet->frameSize = packet->data.size();

    // packetize the packet
    auto packets = std::make_shared<SimplePacketize>(std::move(packet), 1200);
    auto assembler = std::make_unique<SimpleVideoAssembler>();
    PacketPointer result;

    // assemble the packets backwards to ensure they get put back in the correct
    // order
    for (std::size_t i = packets->GetPacketCount() - 1; i >= 0; i--)
    {
        PacketPointer partial_packet = packets->GetPacket(i);
        result = assembler->push(std::move(partial_packet));

        // when we get a packet back, then we should break
        if (result) break;
    }

    // result should be non-null
    CHECK(result);

    // packetize values should be reset
    CHECK_EQ(result->fragmentCount, 1);
    CHECK_EQ(result->chunkFragmentNum, 0);
    CHECK_EQ(result->packetizeType, Packet::PacketizeType::None);

    // check original variables
    CHECK_EQ(result->clientID, client_id);
    CHECK_EQ(result->sourceID, source_id);
    CHECK_EQ(result->conferenceID, conference_id);

    // data size should be identical
    CHECK_EQ(result->frameSize, image_size);
    CHECK_EQ(result->data.size(), image_size);

    // data should be identical
    for (std::size_t i = 0; i < result->data.size(); i++)
    {
        if (result->data[i] != static_cast<std::uint8_t>(i % 127))
        {
            all_good = false;
            break;
        }
    }
    CHECK(all_good);
}