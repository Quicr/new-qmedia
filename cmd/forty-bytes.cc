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
            std::cout << "40B:<<<<<<<" << to_hex(packet->data) << "\n";
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
        std::cout << "40B:>>>>>>>>:" << to_hex(packet->data) << std::endl;
        transportManager->send(std::move(packet));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::shared_ptr<NetTransportQUICR> getTransportHandle(ClientTransportManager*&  transportManager) {
    auto transport = transportManager->transport();
    std::weak_ptr<NetTransportQUICR> tmp =
        std::static_pointer_cast<NetTransportQUICR>(transport.lock());
    return tmp.lock();
}

int main(int argc, char *argv[])
{
    std::string mode;
    std::string transport_type;
    std::string me;
    std::string you;
    uint64_t source_id = 0x1000;
    uint16_t server_port = 7777;
    if (argc < 4)
    {
        std::cerr << "Usage: forty <port> <mode> <self-client-id> <other-client-id>"
                  << std::endl;
        std::cerr << "port: server port for quicr origin/relay" << std::endl;
        std::cerr << "mode: sendrecv/send/recv" << std::endl;
        std::cerr << "self-client-id: some string" << std::endl;
        std::cerr << "other-client-id: some string that is not self" << std::endl;
        return -1;
    }

    std::string port_str;
    port_str.assign(argv[1]);
    if (port_str.empty())
    {
        std::cout << "Port is empty" << std::endl;
        exit(-1);
    }
    server_port = std::stoi(argv[1], nullptr);

    mode.assign(argv[2]);
    if (mode != "send" && mode != "recv" && mode != "sendrecv")
    {
        std::cout << "Bad choice for mode.. Bye" << std::endl;
        exit(-1);
    }

    // names
    me.assign(argv[3]);
    you.assign(argv[4]);

    LoggerPointer logger = std::make_shared<Logger>("FORTY_BYTES");
    logger->SetLogFacility(LogFacility::CONSOLE);

    transportManager = new ClientTransportManager(
        neo_media::NetTransport::QUICR, "127.0.0.1", server_port, nullptr, logger);
    transportManager->start();

    if (mode == "recv")
    {
        if(you.empty()) {
            std::cout << "Bad choice for other-client-id" << std::endl;
            exit(-1);
        }

        auto quicr_transport = getTransportHandle(transportManager);
        quicr_transport->subscribe(
            source_id, Packet::MediaType::Opus, you);
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
        if(me.empty()) {
            std::cout << "Bad choice for self-client-id" << std::endl;
            exit(-1);
        }
        auto quicr_transport = getTransportHandle(transportManager);
        quicr_transport->publish(
            source_id, Packet::MediaType::Opus, me);

        // start the transport
        quicr_transport->start();

        while (!transportManager->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "Transport is ready" << std::endl;

        send_loop(1111, source_id);
    }
    else
    {
        if (me.empty() || you.empty()) {
            std::cout << "Bad choice for clientId(s)" << std::endl;
            exit(-1);
        }

        auto quicr_transport = getTransportHandle(transportManager);
        quicr_transport->subscribe(
            source_id, Packet::MediaType::Opus, you);

        quicr_transport->publish(
            source_id, Packet::MediaType::Opus, me);

        quicr_transport->start();

        while (!transportManager->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "Transport is ready" << std::endl;

        std::thread reader(read_loop);
        send_loop(1111, source_id);
    }

    return 0;
}
