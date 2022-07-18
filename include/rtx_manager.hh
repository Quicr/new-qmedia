#pragma once

#include <string>
#include <list>
#include <thread>
#include <queue>
#include <map>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <memory>
#include <chrono>

#include "../src/packet.hh"
#include "qmedia/metrics.hh"

namespace neo_media
{
// forward declaration
class ClientTransportManager;

class RtxManager
{
public:
    RtxManager(bool retx,
               ClientTransportManager *trans,
               Metrics::MetricsPtr metricsPtr);
    ~RtxManager();
    bool isRetxEnabled();
    uint64_t getSmoothRTT();
    uint64_t getMinimumRTT();
    uint64_t getLastRTT();
    bool packHandle(PacketPointer packet,
                    std::chrono::time_point<std::chrono::steady_clock> now);
    bool ackHandle(const PacketPointer &packet,
                   std::chrono::time_point<std::chrono::steady_clock> now);
    bool nackHandle(PacketPointer packet,
                    std::chrono::time_point<std::chrono::steady_clock> now);

    void updateRTT(std::uint64_t,
                   std::chrono::time_point<std::chrono::steady_clock>,
                   std::chrono::time_point<std::chrono::steady_clock>);
    void reTransmitter();
    std::chrono::milliseconds
    reTransmitWork(std::chrono::milliseconds current_timer,
                   std::chrono::time_point<std::chrono::steady_clock> now);

    ClientTransportManager *transport;
    bool retx_enabled = false;

    struct AuxPacket
    {
        bool ack_status;
        bool retx;
        std::chrono::time_point<std::chrono::steady_clock> send_time;
        std::list<std::uint64_t> retx_seq_list;
    };

    struct TransmitPacket
    {
        Packet::MediaType mediaType;
        uint64_t encodedSequenceNum;
        std::uint32_t chunkFragmentNum;
        uint64_t origTransportSeq;
        bool validPointer = false;
        PacketPointer packet;
    };

    // Currently use Quic formula for RTT estimation over the whole time of
    // session Might limit to specific number of smaples Need to reset when
    // experiencing persistent congestion
    struct RTTInfo
    {
        std::uint64_t minimum = 0;
        std::uint64_t variance = 0;
        std::uint64_t smooth = 0;
    };

    RTTInfo rtt;

    static constexpr uint64_t kWeightRTt = 8;        // RTT weight: 1/8:new -
                                                     // 7/8:history
    std::chrono::milliseconds kInitialRtxDelay{60};        // retransmission
                                                           // timer before RTT
                                                           // is calculated

    // Crude way of creating a retx timer solen from Galia metrics emitter
    std::condition_variable signal;
    std::thread timer_thread;
    std::chrono::milliseconds rtx_delay{
        kInitialRtxDelay};        // how long to
                                  // wait before
                                  // retransmission
                                  // a packet
    std::chrono::milliseconds packet_ttl{kInitialRtxDelay *
                                         2};        // how
                                                    // long
                                                    // before
                                                    // a
                                                    // packet
                                                    // expires
                                                    // from
                                                    // transmission
                                                    // list
    bool shutdown = false;

    std::list<TransmitPacket> tx_list;
    std::map<std::uint64_t, AuxPacket> aux_map;        // This is inefficient as
                                                       // it does not use
                                                       // rreference to
                                                       // AuxPacket....need to
                                                       // use managed pointers
    std::mutex retx_mutex;

    // Metrics
    uint64_t rtx_count = 0;
    uint64_t spurious_rtx_count = 0;
    uint64_t late_ack_count = 0;
    uint64_t self_client_id = 0;

private:
    // Metrics reported by transport manager
    enum struct MeasurementType
    {
        RTT_Smooth,
        PacketRate_RTX,
    };
    std::map<MeasurementType, Metrics::MeasurementPtr> measurements;
    void recordMetric(MeasurementType, const PacketPointer &packetPointer);
    Metrics::MetricsPtr metrics;
};

}        // namespace neo_media
