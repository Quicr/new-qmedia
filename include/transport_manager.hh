
#pragma once

#include <string>
#include <thread>
#include <queue>
#include <cstdint>
#include <mutex>
#include <cassert>
#include <memory>
#include <ostream>
#include <iostream>

#if defined(__linux) || defined(__APPLE__)
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "packet.hh"
#include "transport.hh"
#include "netTransportQuicR.hh"
#include "logger.hh"
#include "metrics.hh"
#include "rtx_manager.hh"

namespace neo_media
{
class NetTransportUDP;

///
/// TransportManager
///
class TransportManager
{
public:
    static NetTransport *make_transport(NetTransport::Type type,
                                        TransportManager *transportManager,
                                        const std::string &sfuName_in,
                                        int sfuPort_in,
                                        const LoggerPointer &logger)
    {
        NetTransport *transport = nullptr;
        switch (type)
        {
            case NetTransport::QUICR:{
                transport = new NetTransportQUICR(
                    transportManager, sfuName_in, sfuPort_in, logger);
                transport->setLogger("TransportQuicR", logger);
            }
            break;
            default:
                throw std::runtime_error("Invalid Transport");
        }

        return transport;
    }

    enum Type
    {
        Client,
        Server
    };

    TransportManager(NetTransport::Type type,
                     const std::string &sfuName_in,
                     int sfuPort_in,
                     Metrics::MetricsPtr metricsPtr,
                     const LoggerPointer &parent_logger = nullptr);
    virtual Type type() const = 0;
    virtual void send(PacketPointer packet) = 0;        // queues this to be
                                                        // sent thread safe
    bool empty();                // any packets to receive..queue locked
    void waitForPacket();        // blocks till a packet is ready for receive

    std::condition_variable recv_cv;
    std::condition_variable send_cv;

    PacketPointer recv();

    bool recvDataFromNet(std::string &data_in,
                         NetTransport::PeerConnectionInfo info);
    bool getDataToSendToNet(std::string &data_out,
                            NetTransport::PeerConnectionInfo *info,
                            socklen_t *addrLen);

    bool getDataToSendToNet(NetTransport::Data& data);
    size_t hasDataToSendToNet();

    virtual bool transport_ready() const = 0;

    bool shutDown = false;

    std::weak_ptr<NetTransport> transport()
    {
        return std::weak_ptr<NetTransport>(netTransport);
    }

protected:
    friend NetTransport;
    std::shared_ptr<NetTransport> netTransport;
    // Metrics reported by transport manager
    enum struct MeasurementType
    {
        PacketRate_Tx,
        PacketRate_Rx,
        FrameRate_Tx,
        FrameRate_Rx,
        QDepth_Tx,
        QDepth_Rx,
    };

    std::map<MeasurementType, Metrics::MeasurementPtr> measurements;
    void recordMetric(MeasurementType, const PacketPointer &packetPointer);
    Metrics::MetricsPtr metrics;

    // rtx handle (used by clientTxMgr today)
    std::unique_ptr<RtxManager> rtx_mgr;

protected:
    virtual ~TransportManager();

    void runNetRecv();
    std::queue<PacketPointer> recvQ;
    std::mutex recvQMutex;
    std::thread recvThread;
    static int recvThreadFunc(TransportManager *t)
    {
        assert(t);
        t->runNetRecv();
        return 0;
    }

    void runNetSend();
    std::queue<PacketPointer> sendQ;
    std::mutex sendQMutex;
    std::thread sendThread;
    static int sendThreadFunc(TransportManager *t)
    {
        assert(t);
        t->runNetSend();
        return 0;
    }

    // Encode packet to bytes based on underlying encoding
    bool netEncode(Packet *packet, std::string &data_out);
    // Decode bytes to Packet, return nullptr on decode error
    bool netDecode(const std::string &data_in, Packet *packet_out);

    LoggerPointer logger;
    bool isLoopback = false;

    uint64_t nextTransportSeq = 0;        // Next packet transport sequence
                                          // number
private:
    const int NUM_METRICS_TO_ACCUMULATE = 50;
    int num_metrics_accumulated = 0;
};

class ClientTransportManager : public TransportManager
{
public:
    ClientTransportManager(NetTransport::Type type,
                           std::string sfuName_in,
                           uint16_t sfuPort_in,
                           Metrics::MetricsPtr metricsPtr = nullptr,
                           const LoggerPointer &parent_logger = nullptr);
    // used for testing
    ClientTransportManager();
    virtual ~ClientTransportManager();
    void start();

    virtual Type type() const { return Type::Client; }

    virtual bool transport_ready() const { return netTransport->ready(); }

    // queues this to be sent thread safe
    virtual void send(PacketPointer packet);

    // Initialize sframe context with the base secret provided by MLS key
    // exchange Note: This is hard coded secret until we bring in MLS
    void setCryptoKey(uint64_t epoch, const bytes &epoch_secret);

    // add to the recvQ
    void loopback(PacketPointer packet);

    bytes protect(const bytes &plaintext);
    bytes unprotect(const bytes &ciphertext);

private:
    std::string sfuName;
    uint16_t sfuPort;
    struct sockaddr_in sfuAddr;        // struct sockaddr_storage sfuAddr;
    socklen_t sfuAddrLen;
    int64_t current_epoch = 0;
    LoggerPointer logger;
};

class ServerTransportManager : public TransportManager
{
public:
    ServerTransportManager(NetTransport::Type type,
                           int sfuPort = 5004,
                           Metrics::MetricsPtr metricsPtr = nullptr,
                           const LoggerPointer &logger = nullptr);
    virtual ~ServerTransportManager();
    virtual Type type() const { return Type::Server; }

    virtual bool transport_ready() const { return netTransport->ready(); }

    virtual void send(PacketPointer packet);
};

}        // namespace neo_media
