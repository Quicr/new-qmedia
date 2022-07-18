#include <doctest/doctest.h>
#include <algorithm>
#include <iterator>
#include "rtx_manager.hh"
#include "../src/transport_manager.hh"

using namespace neo_media;
class FakeClientTransportManager : public ClientTransportManager
{
public:
    virtual void send(PacketPointer packet) { packet_retransmitted = true; }

    RtxManager *rtx_manager_ptr() { return rtx_mgr.get(); }

    bool packet_retransmitted = false;
};

PacketPointer createMediaPacket(uint64_t tx_seq_no, bool rtx_enabled)
{
    auto packet = std::make_unique<Packet>();
    packet->packetType = Packet::Type::StreamContent;
    packet->audioEnergyLevel = 222;
    packet->conferenceID = 111;
    packet->mediaType = Packet::MediaType::Opus;
    packet->encodedSequenceNum = 1;
    packet->sourceRecordTime = 0x0;
    packet->retransmitted = rtx_enabled;
    packet->transportSequenceNumber = tx_seq_no;
    return packet;
}

PacketPointer createMediaAckPacket(uint64_t tx_seq_no)
{
    auto packet = std::make_unique<Packet>();
    packet->packetType = Packet::Type::StreamContentAck;
    packet->transportSequenceNumber = tx_seq_no;
    return packet;
}

TEST_CASE("rtx_enabled")
{
    auto rtx_m = RtxManager(true, nullptr, nullptr);
    CHECK(rtx_m.isRetxEnabled());
}

TEST_CASE("add_packet_for_retransmission")
{
    auto packet = createMediaPacket(1234, false);
    auto seq_num = packet->transportSequenceNumber;
    auto rtx_m = RtxManager(true, nullptr, nullptr);
    auto now = std::chrono::steady_clock::now();

    CHECK(rtx_m.packHandle(std::move(packet), now));
    CHECK(rtx_m.tx_list.size() == 1);
    CHECK(rtx_m.aux_map.count(seq_num) == 1);
}

TEST_CASE("add_packet_and_ack_loop_not_triggered")
{
    auto packet = createMediaPacket(1234, false);
    auto seq_num = packet->transportSequenceNumber;
    auto rtx_m = RtxManager(true, nullptr, nullptr);
    auto now = std::chrono::steady_clock::now();

    CHECK(rtx_m.packHandle(std::move(packet), now));
    CHECK(rtx_m.tx_list.size() == 1);
    CHECK(rtx_m.aux_map.count(seq_num) == 1);

    auto content_ack = std::make_unique<Packet>();
    content_ack->packetType = Packet::Type::StreamContentAck;
    content_ack->transportSequenceNumber = seq_num;
    // memcpy(&content_ack->peer_info.addr,
    //       &(packet->peer_info.addr),
    //       packet->peer_info.addrLen);
    // content_ack->peer_info.transport_connection_id =
    //        packet->peer_info.transport_connection_id;
    // content_ack->peer_info.addrLen = packet->peer_info.addrLen;
    // content_ack->clientID = packet->clientID;
    CHECK(rtx_m.ackHandle(std::move(content_ack), now));
    CHECK(rtx_m.aux_map[seq_num].ack_status);
}

TEST_CASE("Retransmissions and packet TTL expires")
{
    auto rtx_m = RtxManager(false, nullptr, nullptr);

    // send first time packet
    auto now = std::chrono::steady_clock::now();
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(1111, false)), now));

    // packet gets retransmitted after 5 ms
    now += std::chrono::milliseconds(5);
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(2222, true)), now));
    // ensure aux map count is 2
    CHECK(rtx_m.aux_map.size() == 2);
    // verify  if seq_no 2222 is added to 1111's aux_map
    CHECK(rtx_m.aux_map[1111].retx_seq_list.size() == 1);
    auto result = std::find(std::begin(rtx_m.aux_map[1111].retx_seq_list),
                            std::end(rtx_m.aux_map[1111].retx_seq_list),
                            2222);
    CHECK(result != std::end(rtx_m.aux_map[1111].retx_seq_list));

    // packet gets retransmitted after 5 ms again
    now += std::chrono::milliseconds(5);
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(3333, true)), now));
    CHECK(rtx_m.aux_map.size() == 3);
    CHECK(rtx_m.aux_map[1111].retx_seq_list.size() == 2);
    result = std::find(std::begin(rtx_m.aux_map[1111].retx_seq_list),
                       std::end(rtx_m.aux_map[1111].retx_seq_list),
                       3333);
    CHECK(result != std::end(rtx_m.aux_map[1111].retx_seq_list));

    // TODO: also verify packet contents in tx_list match the latest

    // run rtx timer loop to ensure proper cleanup
    now += std::chrono::milliseconds(200);
    rtx_m.reTransmitWork(rtx_m.rtx_delay, now);
    CHECK(rtx_m.aux_map.empty());
    CHECK(rtx_m.tx_list.empty());
}

TEST_CASE("Retransmissions with packet successfully acked")
{
    auto rtx_m = RtxManager(false, nullptr, nullptr);

    // send first time packet
    auto now = std::chrono::steady_clock::now();
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(1111, false)), now));

    // packet gets retransmitted after 5 ms
    now += std::chrono::milliseconds(5);
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(2222, true)), now));
    // ensure aux map count is 2
    CHECK(rtx_m.aux_map.size() == 2);
    // verify  if seq_no 2222 is added to 1111's aux_map
    CHECK(rtx_m.aux_map[1111].retx_seq_list.size() == 1);
    auto result = std::find(std::begin(rtx_m.aux_map[1111].retx_seq_list),
                            std::end(rtx_m.aux_map[1111].retx_seq_list),
                            2222);
    CHECK(result != std::end(rtx_m.aux_map[1111].retx_seq_list));

    // packet gets retransmitted after 5 ms again
    now += std::chrono::milliseconds(5);
    CHECK(rtx_m.packHandle(std::move(createMediaPacket(3333, true)), now));
    CHECK(rtx_m.aux_map.size() == 3);
    CHECK(rtx_m.aux_map[1111].retx_seq_list.size() == 2);
    result = std::find(std::begin(rtx_m.aux_map[1111].retx_seq_list),
                       std::end(rtx_m.aux_map[1111].retx_seq_list),
                       3333);
    CHECK(result != std::end(rtx_m.aux_map[1111].retx_seq_list));

    // TODO: also verify packet contents in tx_list match the latest

    // packet 3333 gets acked
    now += std::chrono::milliseconds(1);
    CHECK(rtx_m.ackHandle(createMediaAckPacket(3333), now));
    // ensure ack_status for orig seq (1111) is marked true as well
    CHECK(rtx_m.aux_map[1111].ack_status);

    // run rtx timer loop to ensure proper cleanup
    now += std::chrono::milliseconds(2);
    rtx_m.reTransmitWork(rtx_m.rtx_delay, now);
    CHECK(rtx_m.aux_map.empty());
    CHECK(rtx_m.tx_list.empty());
}

TEST_CASE("RTX timer loop trigger retransmission")
{
    auto transport = new FakeClientTransportManager();

    auto rtx_m = transport->rtx_manager_ptr();

    // send first time packet
    auto now = std::chrono::steady_clock::now();
    CHECK(rtx_m->packHandle(std::move(createMediaPacket(1111, false)), now));

    // packet gets retransmitted after 5 ms
    now += std::chrono::milliseconds(5);
    CHECK(rtx_m->packHandle(std::move(createMediaPacket(2222, true)), now));
    // ensure aux map count is 2
    CHECK(rtx_m->aux_map.size() == 2);
    // verify  if seq_no 2222 is added to 1111's aux_map
    CHECK(rtx_m->aux_map[1111].retx_seq_list.size() == 1);
    auto result = std::find(std::begin(rtx_m->aux_map[1111].retx_seq_list),
                            std::end(rtx_m->aux_map[1111].retx_seq_list),
                            2222);
    CHECK(result != std::end(rtx_m->aux_map[1111].retx_seq_list));

    // run rtx timer loop
    now += std::chrono::milliseconds(20);
    // first param is set to 10 ms, to ensure packet in  tx_listed has been
    // waited more than 10ms.
    rtx_m->rtx_delay = std::chrono::milliseconds(10);
    rtx_m->reTransmitWork(std::chrono::milliseconds(1), now);
    CHECK(transport->packet_retransmitted);
}

TEST_CASE("RTX timer triggered before any retransmission and wait timer "
          "updated")
{
    auto transport = new FakeClientTransportManager();

    auto rtx_m = transport->rtx_manager_ptr();

    // send first time packet
    auto now = std::chrono::steady_clock::now();
    CHECK(rtx_m->packHandle(std::move(createMediaPacket(1111, false)), now));

    now += std::chrono::milliseconds(5);
    auto wait_time = rtx_m->reTransmitWork(std::chrono::milliseconds(10), now);
    // rtx_delay - (now - oldest time)
    // 60 - 5 .. since packet was sent 5ms spaced
    CHECK(wait_time == std::chrono::milliseconds(55));
}
