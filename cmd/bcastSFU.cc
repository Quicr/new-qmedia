
#include <iostream>
#include <iomanip>
#include <sstream>

#include "transport_manager.hh"

using namespace neo_media;

bool use_quic = true;

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

struct SFU
{
    struct Association
    {
        uint64_t clientID;
        neo_media::NetTransport::PeerConnectionInfo peer_info;
    };

    std::map<uint64_t, Association> subscribers;
};

void print(const Packet *packet)
{
    std::cout << "Type " << packet->packetType << std::endl;
    std::cout << "Client " << packet->clientID << std::endl;
    std::cout << "ConfID " << packet->conferenceID << std::endl;
    std::cout << "XportID " << packet->peer_info.transport_connection_id
              << std::endl;
    std::cout << "Size " << packet->data.size() << std::endl;
}

int main(int argc, char *argv[])
{
    auto sfu = SFU{};
    std::string transport_str;
    if (argc < 2)
    {
        std::cerr << "Must provide transport" << std::endl;
        std::cerr << "Usage: bcastSFU <transport>" << std::endl;
        std::cerr << "Transport: q (for quic), r (udp)" << std::endl;
        return -1;
    }

    transport_str.assign(argv[1]);

    auto transport_type = NetTransport::Type::UDP;
    if (transport_str == "q")
    {
        std::cout << "Using Quic Transport\n";
        transport_type = NetTransport::Type::PICO_QUIC;
    }

    ServerTransportManager transport(transport_type);

    while (1)
    {
        if (transport.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        PacketPointer packet = transport.recv();
        if (!sfu.subscribers.count(packet->clientID))
        {
            std::cout << packet->packetType
                      << ": adding subscriber: " << packet->clientID
                      << std::endl;
            sfu.subscribers.insert(
                {packet->clientID,
                 SFU::Association{packet->clientID, packet->peer_info}});
        }

        // std::cout << "bsfu: cnxId  "
        //          << to_hex(packet->peer_info.transport_connection_id);
        // std::cout <<", size " << packet->data.size() << std::endl;

        // broadcast
        for (auto const &[client, assoc] : sfu.subscribers)
        {
            if (packet->clientID == client)
            {
                // std::cout << "not forwarding; reason(sender)\n";
                continue;
            }

            if (packet->packetType != neo_media::Packet::Type::StreamContent)
            {
                // std::cout << "ignoring non content packet: " <<
                // packet->packetType <<  std::endl;
                continue;
            }

            std::cout << "forwarding to cnx "
                      << to_hex(assoc.peer_info.transport_connection_id)
                      << std::endl;
            auto copy = std::make_unique<Packet>(*packet);
            copy->peer_info = assoc.peer_info;
            // std::clog << packet->clientID <<".[" << assoc.clientID <<"]";
            transport.send(std::move(copy));
        }
        // std::clog << ".["<< packet->sequenceNumber<<"]";
    }

    return 0;
}
