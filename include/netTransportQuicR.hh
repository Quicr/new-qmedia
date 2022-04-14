#pragma once

#include <cassert>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

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

#include "netTransportUDP.hh"
#include "transport.hh"
#include "logger.hh"

#include "picoquic.h"
#include "picoquic_internal.h"
#include "picoquic_logger.h"
#include "picoquic_utils.h"
#include "picoquic_config.h"

#include "quicrq_relay.h"
#include "quicrq_reassembly.h"

namespace neo_media
{
class TransportManager;
class NetTransportQUICR;

// Context shared with th the underlying quicr stack
struct TransportContext
{
    uint16_t port;
    quicrq_ctx_t *qr_ctx;
    TransportManager *transportManager;
    NetTransportQUICR *transport;
    bytes local_connection_id;
    bool initialized = false;
};

// Handy storage for few quic client context
struct QuicRClientContext
{
    std::string server_name;
    uint16_t port;
    struct sockaddr_storage server_address;
    socklen_t server_address_len;
    quicrq_ctx_t *qr_ctx;
};

struct PublisherContext
{
    uint64_t source_id;
    Packet::MediaType media_type;
    std::string url;
    quicrq_media_source_ctx_t *source_ctx;
    TransportManager *transportManager;
    NetTransportQUICR *transport;
};

struct ConsumerContext
{
    Packet::MediaType media_type;
    std::string url;
    quicrq_reassembly_context_t reassembly_ctx;
    quicrq_cnx_ctx_t *cnx_ctx;
    TransportManager *transportManager;
    NetTransportQUICR *transport;
};

// QUIC transport
class NetTransportQUICR : public NetTransport
{
public:
    // Client
    NetTransportQUICR(TransportManager *manager,
                      std::string sfuName_in,
                      uint16_t sfuPort_in,
                      const LoggerPointer& logger_in);

    virtual ~NetTransportQUICR();

    virtual bool ready() override;
    virtual void close() override;
    virtual bool doSends() override;
    virtual bool doRecvs() override;
    virtual void shutdown() override {}
    virtual NetTransport::PeerConnectionInfo getConnectionInfo()
    {
        return PeerConnectionInfo{quicr_client_ctx.server_address,
                                  quicr_client_ctx.server_address_len,
                                  local_connection_id};
    }

    // utils for managing quicr sources
    void publish(uint64_t source_id,
                 Packet::MediaType media_type,
                 const std::string &url);
    void remove_source(uint64_t source_id);
    void subscribe(Packet::MediaType media_type, const std::string &url);

    void start();

    // callback registered with the underlying quicr stack
    static int quicr_callback(picoquic_cnx_t *cnx,
                              uint64_t stream_id,
                              uint8_t *bytes,
                              size_t length,
                              picoquic_call_back_event_t fin_or_event,
                              void *callback_ctx,
                              void *v_stream_ctx);

    // todo add a setter api
    TransportManager *transportManager;

    // Reports if the underlying quic stack is ready
    // for application messages
    std::mutex quicConnectionReadyMutex;
    bool quicConnectionReady;

    // Thread and its function managing quic context.
    std::thread quicTransportThread;
    int runQuicProcess();
    static int quicTransportThreadFunc(NetTransportQUICR *netTransportQuic)
    {
        return netTransportQuic->runQuicProcess();
    }

    bytes local_connection_id;

    const PublisherContext &get_publisher_context(uint64_t source_id) const
    {
        return publishers.at(source_id);
    }

    void wake_up_all_sources();

    LoggerPointer logger;
private:
    const std::string alpn = "quicr-h00";
    TransportContext xport_ctx;
    QuicRClientContext quicr_client_ctx;
    picoquic_quic_config_t config;
    quicrq_ctx_t *quicr_ctx;
    // Quicr Connection Context
    quicrq_cnx_ctx_t *cnx_ctx = nullptr;
    picoquic_quic_t *quic = nullptr;
    // todo: media_type as key is not a great choice
    // esp when creating multiple sources of same type.
    std::map<uint64_t, PublisherContext> publishers = {};
};

}        // namespace neo_media
