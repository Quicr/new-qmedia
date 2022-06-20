
#pragma once

#include <stdint.h>
#include <vector>
#include <sys/types.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif
#include "transport.hh"

namespace neo_media
{
class Packet;

using PacketPointer = std::unique_ptr<Packet>;

class Packet
{
public:
    // packet type
    enum struct Type : uint16_t
    {
        Unknown = 0,
        Join = 1,
        JoinAck = 2,
        StreamContent = 3,
        StreamContentNack = 4,
        StreamContentAck = 5,
        IdrRequest = 6,
    };

    // supported media types
    enum struct MediaType : uint16_t
    {
        Bad = 0,
        Opus = 1,
        L16 = 2,
        AV1 = 3,
        F32 = 4,
        Raw = 5
    };

    enum struct PacketizeType : uint16_t
    {
        None = 0,
        Simple = 1
    };

    enum struct VideoFrameType : uint8_t
    {
        None = 0,
        Idr = 1
    };

    // IDR message payload
    struct IdrRequestData
    {
        uint64_t client_id;
        uint64_t source_id;
        uint64_t source_timestamp;
    };

public:
    Packet();
    Packet(const Packet &) = default;
    Packet(Packet &&) = default;

    static bool encode(Packet *packet, std::string &data_out);
    static bool decode(const std::string &data_in, Packet *packet_out);

    uint64_t transportSequenceNumber;        // data independent sequence number
    uint64_t conferenceID;
    uint64_t sourceID;        // unique per source (scoped with a client)
    uint64_t clientID;
    uint64_t sourceRecordTime;

    MediaType mediaType;
    VideoFrameType videoFrameType = VideoFrameType::None;
    Type packetType;
    bool is_last_fragment = false;

    float audioEnergyLevel;
    bool echo;
    bool encrypted;
    bool retransmitted;

    // payload
    std::vector<uint8_t> data;        // media bytes (may be encrypted or not)
    std::vector<uint8_t> authTag;
    std::string encoded_data;        // transport encoded

    NetTransport::PeerConnectionInfo peer_info;        // destination

    // packetization info
    PacketizeType packetizeType;
    uint64_t encodedSequenceNum;
    std::uint32_t chunkFragmentNum;
    std::uint32_t fragmentCount;
    std::uint32_t frameSize;

    // Idr info
    IdrRequestData idrRequestData;

    // priority level
    // TODO: make it an enumeration
    uint8_t priorityLevel;

    // video frame makers
    bool intra_frame = false;
    bool discardable_frame = false;
    uint8_t temporalLayerId;
    uint8_t spatialLayerId;
};

std::ostream &operator<<(std::ostream &os, const Packet::Type &pktType);
}        // namespace neo_media
