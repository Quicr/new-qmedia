
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

namespace qmedia
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
        StreamContent = 3,
     };

    // supported media types
    enum struct MediaType : uint16_t
    {
        Bad = 0,
        Opus = 1,
        L16 = 2,
        H264 = 3,
        F32 = 4,
        Raw = 5
    };


public:
    Packet();
    Packet(const Packet &) = default;
    Packet(Packet &&) = default;

    static bool encode(Packet *packet, std::vector<uint8_t>& data_out);
    static bool decode(const std::vector<uint8_t>& data_in, Packet *packet_out);

    uint64_t conferenceID;
    uint64_t sourceID;        // unique per source (scoped with a client)
    uint64_t clientID;
    uint64_t sourceRecordTime;

    MediaType mediaType;
    // payload
    std::vector<uint8_t> data;        // media bytes (may be encrypted or not)
    std::vector<uint8_t> authTag;
    std::vector<uint8_t> encoded_data;        // transport encoded
};

std::ostream &operator<<(std::ostream &os, const Packet::Type &pktType);
}