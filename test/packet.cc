#include <doctest/doctest.h>
#include <string>
#include <variant>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "../src/packet.hh"

using namespace neo_media;

static void packet_dump(Packet &pkt)
{
    std::cout << "Type: " << pkt.packetType
              << ",SeqNo: " << pkt.encodedSequenceNum
              << ",AggrSeqNo: " << pkt.transportSequenceNumber
              << ",Data Size: " << pkt.data.size()
              << ",ConferenceID: " << pkt.conferenceID
              << ",SourceID: " << pkt.sourceID
              << ",packetNumber: " << pkt.chunkFragmentNum
              << ",packetCount: " << pkt.fragmentCount
              << ",FrameSize: " << pkt.frameSize << std::endl;
}

static std::string to_hex(const std::vector<uint8_t> &data)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    for (const auto &byte : data)
    {
        hex << std::setw(2) << std::setfill('0') << int(byte);
    }
    return hex.str();
}

static std::vector<uint8_t> from_hex(const std::string &hex)
{
    if (hex.length() % 2 == 1)
    {
        throw std::invalid_argument("Odd-length hex string");
    }

    auto len = int(hex.length() / 2);
    auto out = std::vector<uint8_t>(len);
    for (int i = 0; i < len; i += 1)
    {
        auto byte = hex.substr(2 * i, 2);
        out[i] = static_cast<uint8_t>(strtol(byte.c_str(), nullptr, 16));
    }

    return out;
}

static uint64_t conference = 1;

TEST_CASE("1 Encode/Decode join packet types")
{
    auto join = std::make_unique<Packet>();
    join->packetType = neo_media::Packet::Type::Join;
    join->conferenceID = conference;
    join->transportSequenceNumber = 12345;
    packet_dump(*join);

    auto data_out = std::string{};
    data_out.reserve(1500);
    auto encoded = Packet::encode(join.get(), data_out);
    CHECK(encoded);

    auto packet_in = std::make_unique<Packet>();
    auto decoded = Packet::decode(data_out, packet_in.get());
    CHECK(decoded);
    packet_dump(*packet_in);
    CHECK_EQ(packet_in->packetType, join->packetType);
    CHECK_EQ(packet_in->encodedSequenceNum, join->encodedSequenceNum);
    CHECK_EQ(packet_in->transportSequenceNumber, join->transportSequenceNumber);
}

TEST_CASE("1 Encode/Decode join_ack packet types")
{
    auto ack = std::make_unique<Packet>();
    ack->packetType = neo_media::Packet::Type::JoinAck;
    ack->conferenceID = conference;

    auto data_out = std::string{};
    data_out.reserve(1500);
    auto encoded = Packet::encode(ack.get(), data_out);
    CHECK(encoded);
    auto packet_in = std::make_unique<Packet>();
    auto decoded = Packet::decode(data_out, packet_in.get());
    CHECK(decoded);
    CHECK_EQ(packet_in->packetType, ack->packetType);
    CHECK_EQ(packet_in->conferenceID, ack->conferenceID);
}

TEST_CASE("1 Stream Message Encode/Decode")
{
    auto audio = std::make_unique<Packet>();
    audio->packetType = Packet::Type::StreamContent;
    audio->audioEnergyLevel = 222;
    audio->conferenceID = conference;
    audio->mediaType = Packet::MediaType::Opus;
    audio->encodedSequenceNum = 1;
    audio->sourceRecordTime = 0x0;
    audio->data = from_hex("00010203");
    audio->retransmitted = true;

    auto data_out = std::string{};
    data_out.reserve(1500);
    auto encoded = Packet::encode(audio.get(), data_out);
    CHECK(encoded);
    auto packet_in = std::make_unique<Packet>();
    auto decoded = Packet::decode(data_out, packet_in.get());
    CHECK(decoded);
    CHECK_EQ(packet_in->data, audio->data);
    CHECK_EQ(packet_in->audioEnergyLevel, audio->audioEnergyLevel);
    CHECK(packet_in->retransmitted);
}

TEST_CASE("Video Header Encode/Decode")
{
    auto video = std::make_unique<Packet>();
    video->packetType = Packet::Type::StreamContent;
    video->conferenceID = conference;
    video->mediaType = Packet::MediaType::AV1;
    video->encodedSequenceNum = 1;
    video->sourceRecordTime = 0x0;
    video->data = from_hex("00010203");
    video->packetizeType = neo_media::Packet::PacketizeType::Simple;
    video->chunkFragmentNum = 1000;
    video->frameSize = 16653;
    video->fragmentCount = 123;
    video->intra_frame = true;
    video->discardable_frame = false;
    video->priorityLevel = 2;
    video->temporalLayerId = 1;
    video->spatialLayerId = 3;

    auto data_out = std::string{};
    data_out.reserve(1500);
    auto encoded = Packet::encode(video.get(), data_out);
    CHECK(encoded);
    auto packet_in = std::make_shared<Packet>();
    auto decoded = Packet::decode(data_out, packet_in.get());
    CHECK(decoded);
    CHECK_EQ(packet_in->data, video->data);
    CHECK_EQ(packet_in->packetizeType, video->packetizeType);
    CHECK_EQ(packet_in->chunkFragmentNum, video->chunkFragmentNum);
    CHECK_EQ(packet_in->frameSize, video->frameSize);
    CHECK_EQ(packet_in->spatialLayerId, video->spatialLayerId);
    CHECK_EQ(packet_in->temporalLayerId, video->temporalLayerId);
    CHECK_EQ(packet_in->intra_frame, video->intra_frame);
    CHECK_EQ(packet_in->discardable_frame, video->discardable_frame);
    CHECK_EQ(packet_in->priorityLevel, video->priorityLevel);
}

TEST_CASE("IDR Encode/Decode")
{
    auto idr = std::make_unique<Packet>();
    idr->packetType = Packet::Type::IdrRequest;
    idr->idrRequestData = Packet::IdrRequestData{0x1234, 0xabcd, 0x0000};

    auto data_out = std::string{};
    data_out.reserve(1500);
    auto encoded = Packet::encode(idr.get(), data_out);
    CHECK(encoded);
    auto packet_in = std::make_shared<Packet>();
    auto decoded = Packet::decode(data_out, packet_in.get());
    CHECK(decoded);
    CHECK_EQ(packet_in->packetType, Packet::Type::IdrRequest);
    CHECK_EQ(packet_in->idrRequestData.source_id,
             idr->idrRequestData.source_id);
    CHECK_EQ(packet_in->idrRequestData.client_id,
             idr->idrRequestData.client_id);
    CHECK_EQ(packet_in->idrRequestData.source_timestamp,
             idr->idrRequestData.source_timestamp);
}
