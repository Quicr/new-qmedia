#ifdef ENABLE_QUIC
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

#include <picoquic/picoquic.h>
#include <picoquic/picoquic_internal.h>
#include <picoquic/picoquic_logger.h>
#include <picoquic/picoquic_utils.h>

namespace neo_media
{
class TransportManager;

class NetTransportQUIC;

// Context shared with th the underlying quic stack
struct TransportContext
{
    TransportManager *transportManager;
    NetTransportQUIC *transport;
    bool initialized = false;
};

// apln for quic handshake
typedef enum
{
    alpn_undef = 0,
    alpn_neo_media
} picoquic_alpn_enum;

typedef struct st_alpn_list_t
{
    picoquic_alpn_enum alpn_code;
    char const *alpn_val;
} alpn_list_t;

static alpn_list_t alpn_list[] = {{alpn_neo_media, "proto-pq-sample"}};

// QUIC transport
class NetTransportQUIC : public NetTransport
{
public:
    // Client
    NetTransportQUIC(TransportManager *manager,
                     std::string sfuName_in,
                     uint16_t sfuPort_in);

    // Server
    NetTransportQUIC(TransportManager *manager, uint16_t sfuPort_in);
    virtual ~NetTransportQUIC();

    virtual bool ready() override;
    virtual void close() override;
    virtual bool doSends() override;
    virtual bool doRecvs() override;
    virtual NetTransport::PeerConnectionInfo getConnectionInfo()
    {
        return PeerConnectionInfo{quic_client_ctx.server_address,
                                  quic_client_ctx.server_address_len,
                                  local_connection_id};
    }

    // callback registered with the quic stack on transport and data states
    static int datagram_callback(picoquic_cnx_t *cnx,
                                 uint64_t stream_id,
                                 uint8_t *bytes,
                                 size_t length,
                                 picoquic_call_back_event_t fin_or_event,
                                 void *callback_ctx,
                                 void *v_stream_ctx);

    TransportManager *transportManager;

    // Reports if the underlying quic stack is ready
    // for application messages
    std::mutex quicConnectionReadyMutex;
    bool quicConnectionReady;

    // Thread and its function managing quic context.
    std::thread quicTransportThread;
    int runQuicProcess();
    static int quicTransportThreadFunc(NetTransportQUIC *netTransportQuic)
    {
        return netTransportQuic->runQuicProcess();
    }

    // Handy storage for few quic client context
    struct QuicClientContext
    {
        std::string server_name;
        uint16_t port;
        std::string sni;
        struct sockaddr_storage server_address;
        socklen_t server_address_len;
    };

private:
    // Kick start Quic's connection context as a client
    int quic_start_connection();
    void add_connection(bytes &conn_id, picoquic_cnx_t *conn);
    void remove_connection(const bytes &conn_id);

    const bool m_isServer;
    const std::string alpn = "proto-pq-sample";
    sockaddr_storage local_address;
    uint16_t local_port = 0;
    bytes local_connection_id;
    QuicClientContext quic_client_ctx;
    picoquic_quic_t *quicHandle = nullptr;        // ref to quic stack
    picoquic_cnx_t *client_cnx;                   // ref to client connection
    NetTransportUDP *udp_socket;        // underlying transport socket.
    std::map<bytes, picoquic_cnx_t *> connections;
};

}        // namespace neo_media

#endif