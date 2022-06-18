#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

#include "neo_media_client.hh"
#include "neo.hh"

#include <transport_manager.hh>
#include <transport.hh>

using namespace neo_media;
using namespace std::chrono_literals;

uint64_t conference_id = 1234;
bool done = false;
ClientTransportManager *transportManager = nullptr;

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

void read_loop()
{
    std::cout << "Client read audio loop init\n";
    while (!done)
    {
        auto packet = transportManager->recv();
        if (!packet)
        {
            continue;
        }
        if (packet->packetType == neo_media::Packet::Type::StreamContent)
        {
            std::cout << "VideoFrameType: " << (int) packet->videoFrameType
                      << "\n";
            std::cout << "40B:Received:" << to_hex(packet->data) << "\n";
        }
        else
        {
        }
    }
}

void send_loop(uint64_t client_id, uint64_t source_id)
{
    const uint8_t forty_bytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
                                   4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7,
                                   8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint32_t pkt_num = 0;

    while (!done)
    {
        auto data = bytes(forty_bytes, forty_bytes + sizeof(forty_bytes));
        PacketPointer packet = std::make_unique<Packet>();
        assert(packet);
        pkt_num++;
        packet->clientID = client_id;
        packet->data = std::move(data);
        packet->sourceID = source_id;
        packet->conferenceID = conference_id;
        packet->packetType = neo_media::Packet::Type::StreamContent;
        packet->packetizeType = neo_media::Packet::PacketizeType::None;
        packet->mediaType = neo_media::Packet::MediaType::Opus;
        packet->videoFrameType = neo_media::Packet::VideoFrameType::Idr;
        packet->encodedSequenceNum = pkt_num;
        std::cout << "40B:Sending:" << to_hex(packet->data) << std::endl;
        transportManager->send(std::move(packet));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}


int main(int argc, char *argv[])
{
    std::string mode;
    std::string transport_type;
    uint32_t client_id;
    uint64_t source_id = 0x1000;

    if (argc < 4)
    {
        std::cerr << "Must provide mode of operation" << std::endl;
        std::cerr << "Usage: forty <transport> <mode> <client-id> "
                  << std::endl;
        std::cerr << "Transport: q (for quic), r (udp),  qr(quicr)"
                  << std::endl;
        std::cerr << "Mode: sendrecv/send/recv" << std::endl;
        std::cerr << "ClientID - a sensible +ve 32 bit integer value"
                  << std::endl;
        return -1;
    }


    transport_type.assign(argv[1]);

    LoggerPointer logger = std::make_shared<Logger>("FORTY_BYTES");
    logger->SetLogFacility(LogFacility::CONSOLE);


    if (transport_type == "q")
    {
        std::cout << "Transport is Quic [ !!! Not under active developmenr "
                     "!!!]\n";
        transportManager = new ClientTransportManager(
            neo_media::NetTransport::PICO_QUIC, "localhost", 5004);
    }
    else if (transport_type == "qr")
    {
        std::cout << "Transport is QuicR\n";
        transportManager = new ClientTransportManager(
            neo_media::NetTransport::QUICR, "127.0.0.1", 7777, nullptr, logger);
        transportManager->start();
    }
    else
    {
        std::cout << "Transport is UDP\n";
        transportManager = new ClientTransportManager(
            neo_media::NetTransport::UDP, "localhost", 5004);
        transportManager->start();
    }

    mode.assign(argv[2]);
    if (mode != "send" && mode != "recv" && mode != "sendrecv")
    {
        std::cout << "Bad choice for mode.. Bye" << std::endl;
        exit(-1);
    }

    std::string client_id_str;
    client_id_str.assign(argv[3]);
    if (client_id_str.empty())
    {
        std::cout << "Bad choice for clientId .. Bye" << std::endl;
        exit(-1);
    }
    client_id = std::stoi(argv[3], nullptr);

    if (mode == "recv")
    {
        auto transport = transportManager->transport();
        std::weak_ptr<NetTransportQUICR> tmp =
            std::static_pointer_cast<NetTransportQUICR>(transport.lock());
        auto quicr_transport = tmp.lock();
        quicr_transport->subscribe(source_id,
                                   Packet::MediaType::Opus,
                                   "forty_bytes_alice");

        // start the transport
        quicr_transport->start();

        while (!transportManager->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "Transport is ready" << std::endl;
        read_loop();
    }
    else if (mode == "send")
    {
        auto transport = transportManager->transport();
        std::weak_ptr<NetTransportQUICR> tmp =
            std::static_pointer_cast<NetTransportQUICR>(transport.lock());
        auto quicr_transport = tmp.lock();
        quicr_transport->publish(
            source_id, Packet::MediaType::Opus, "forty_bytes_alice");

        // start the transport
        quicr_transport->start();

        while (!transportManager->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "Transport is ready" << std::endl;

        send_loop(client_id, source_id);
    }
    else
    {
        while (!transportManager->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "Transport is ready" << std::endl;

        std::thread reader(read_loop);
        send_loop(client_id, source_id);
    }

    if (transport_type != "qr")
    {
        // send a subscribe
        PacketPointer join = std::make_unique<Packet>();
        assert(join);

        join->clientID = client_id;
        join->packetType = neo_media::Packet::Type::Join;
        join->conferenceID = conference_id;
        std::cout << "40B:Sending Join:" << std::endl;
        transportManager->send(std::move(join));
    }

    return 0;
}
