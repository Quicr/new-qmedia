
#include <iomanip>
#include <iostream>
#include <sstream>

#include "transport_manager.hh"

using namespace neo_media;

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

int main(int argc, char *argv[])
{
    std::string transport_str;
    if (argc < 2)
    {
        std::cerr << "Must provide transport" << std::endl;
        std::cerr << "Usage: echoSFU <transport>" << std::endl;
        std::cerr << "Transport: q (for quic), r (udp)" << std::endl;
        std::cerr << "(Optional: debug <true/false>)" << std::endl;
        return -1;
    }

    transport_str.assign(argv[1]);

    // Logger.
    auto logger = std::make_shared<Logger>("ECHO");
    bool debug = false;
    if (argc == 3)
    {
        bool echo = false;
        std::stringstream ss(argv[2]);
        ss >> std::boolalpha >> debug;
    }
    logger->SetLogLevel(debug ? LogLevel::DEBUG : LogLevel::INFO);

    auto transport_type = NetTransport::Type::UDP;
    if (transport_str == "q")
    {
        std::cout << "Using Quic Transport\n";
        transport_type = NetTransport::Type::PICO_QUIC;
    }

    ServerTransportManager transport(transport_type, 5004, nullptr, logger);
    while (1)
    {
        if (transport.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // transport.waitForPacket();
        PacketPointer packet = transport.recv();
        if (Packet::Type::StreamContent == packet->packetType)
        {
            // send ack immediately ( real sfu shall not be that reactive)
            PacketPointer content_ack = std::make_unique<Packet>();
            content_ack->packetType = Packet::Type::StreamContentAck;
            content_ack->transportSequenceNumber =
                packet->transportSequenceNumber;
            content_ack->peer_info = packet->peer_info;
            transport.send(std::move(content_ack));
            // echo packet
            std::clog << to_hex(packet->data) << std::endl;
            transport.send(std::move(packet));
        }
        else if (Packet::Type::StreamContentAck == packet->packetType)
        {
            // std::clog << "got content ack for: " <<
            // packet->transportSequenceNumber << std::endl;
        }
        else if (Packet::Type::IdrRequest == packet->packetType)
        {
            // echo packet
            transport.send(std::move(packet));
        }
    }

    return 0;
}
