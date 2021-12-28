#ifdef ENABLE_QUIC
#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string.h>        // memcpy
#include <thread>
#include <sstream>

#if defined(__linux) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netdb.h>
#endif
#if defined(__linux__)
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <net/if_dl.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "netTransportQuic.hh"
#include "transport_manager.hh"

#include <picoquic/picoquic.h>
#include <picoquic/picoquic_internal.h>
#include <picoquic/picoquic_logger.h>
#include <picoquic/picoquic_utils.h>
#include <picoquic/picosocks.h>
#include <picotls.h>

using namespace neo_media;

#define SERVER_CERT_FILE "cert.pem"
#define SERVER_KEY_FILE "key.pem"

///
// Utility
///

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

static void print_sock_info(const std::string &debug_string,
                            sockaddr_storage *addr)
{
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];
    socklen_t len = sizeof(struct sockaddr_storage);
    int rc = getnameinfo((struct sockaddr *) addr,
                         len,
                         hoststr,
                         sizeof(hoststr),
                         portstr,
                         sizeof(portstr),
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        std::cout << "getnameinfo error = " << gai_strerror(rc) << "\n";
        // assert(0);
    }
    else
    {
        std::cout << debug_string << " host: " << hoststr
                  << " port: " << portstr << std::endl;
    }
}

///
// alpn helpers
///

static size_t nb_alpn_list = sizeof(alpn_list) / sizeof(alpn_list_t);

picoquic_alpn_enum parse_alpn_nz(char const *alpn, size_t len)
{
    picoquic_alpn_enum code = alpn_undef;

    if (alpn != nullptr)
    {
        for (size_t i = 0; i < nb_alpn_list; i++)
        {
            if (memcmp(alpn, alpn_list[i].alpn_val, len) == 0 &&
                alpn_list[i].alpn_val[len] == 0)
            {
                code = alpn_list[i].alpn_code;
                break;
            }
        }
    }

    return code;
}

/* Callback from the TLS stack upon receiving a list of proposed ALPN in the
 * Client Hello */
size_t select_alpn(picoquic_quic_t *quic, ptls_iovec_t *list, size_t count)
{
    size_t ret = count;

    for (size_t i = 0; i < count; i++)
    {
        if (parse_alpn_nz((const char *) list[i].base, list[i].len) !=
            alpn_undef)
        {
            ret = i;
            break;
        }
    }

    return ret;
}

// quic stack report reason
int transport_close_reason(picoquic_cnx_t *cnx)
{
    uint64_t last_err = 0;
    int ret = 0;
    if ((last_err = picoquic_get_local_error(cnx)) != 0)
    {
        fprintf(stdout,
                "Connection end with local error 0x%" PRIx64 ".\n",
                last_err);
        ret = -1;
    }

    if ((last_err = picoquic_get_remote_error(cnx)) != 0)
    {
        fprintf(stdout,
                "Connection end with remote error 0x%" PRIx64 ".\n",
                last_err);
        ret = -1;
    }

    if ((last_err = picoquic_get_application_error(cnx)) != 0)
    {
        fprintf(stdout,
                "Connection end with application error 0x%" PRIx64 ".\n",
                last_err);
        ret = -1;
    }

    return ret;
}

// Callback from quic stack on transport states and data
int NetTransportQUIC::datagram_callback(picoquic_cnx_t *cnx,
                                        uint64_t stream_id,
                                        uint8_t *bytes_in,
                                        size_t length,
                                        picoquic_call_back_event_t fin_or_event,
                                        void *callback_ctx,
                                        void *v_stream_ctx)
{
    // std::cout << "datagram_callback <<<\n";
    int ret = 0;
    auto *ctx = (neo_media::TransportContext *) callback_ctx;
    if (!ctx->initialized)
    {
        picoquic_set_callback(cnx, &NetTransportQUIC::datagram_callback, ctx);
        ctx->initialized = true;
    }

    ret = 0;

    switch (fin_or_event)
    {
        case picoquic_callback_stream_data:
        case picoquic_callback_stream_fin:
        case picoquic_callback_stream_reset: /* Client reset stream #x */
        case picoquic_callback_stop_sending: /* Client asks server to reset
                                                stream #x */
        case picoquic_callback_stream_gap:
        case picoquic_callback_prepare_to_send:
            std::cout << "Unexpected callback: "
                         "picoquic_callback_prepare_to_send"
                      << std::endl;
            ret = -1;
            break;
        case picoquic_callback_stateless_reset:
        case picoquic_callback_close: /* Received connection close */
        case picoquic_callback_application_close:
        {
            auto cnx_id = picoquic_get_client_cnxid(cnx);
            auto cnx_id_bytes = bytes(cnx_id.id, cnx_id.id + cnx_id.id_len);
            std::cout << to_hex(cnx_id_bytes)
                      << ":picoquic_callback_application_close: "
                      << transport_close_reason(cnx) << std::endl;
            ctx->transport->remove_connection(cnx_id_bytes);
        }
        break;
        case picoquic_callback_version_negotiation:
            break;
        case picoquic_callback_almost_ready:
            std::cout << "picoquic_callback_almost_ready" << std::endl;
            break;
        case picoquic_callback_ready:
        {
            std::cout << " Quic Callback: Transport Ready" << std::endl;
            if (ctx->transport)
            {
                std::lock_guard<std::mutex> lock(
                    ctx->transport->quicConnectionReadyMutex);

                ctx->transport->quicConnectionReady = true;

                // save the connection information
                auto cnx_id = picoquic_get_client_cnxid(cnx);
                auto cnx_id_bytes = bytes(cnx_id.id, cnx_id.id + cnx_id.id_len);
                ctx->transport->add_connection(cnx_id_bytes, cnx);

                if (cnx->client_mode)
                {
                    ctx->transport->local_connection_id = std::move(
                        cnx_id_bytes);
                }
            }
        }
            ret = 0;
            break;
        case picoquic_callback_datagram:
        {
            /* Process the datagram
             */
            auto data = std::string(bytes_in, bytes_in + length);
            // std::cout << "picoquic_callback_datagram " << data.size()
            //          << " bytes\n";
            struct sockaddr *peer_addr = nullptr;
            picoquic_get_peer_addr(cnx, &peer_addr);
            NetTransport::PeerConnectionInfo peer_info;
            // TODO: support IPV6
            memcpy(&peer_info.addr,
                   (sockaddr_in *) peer_addr,
                   sizeof(sockaddr_in));
            peer_info.addrLen = sizeof(struct sockaddr_storage);
            auto cnx_id = picoquic_get_client_cnxid(cnx);
            auto cnx_id_bytes = bytes(cnx_id.id, cnx_id.id + cnx_id.id_len);
            // print_sock_info("dg_callbk:", &peer_info.addr);
            peer_info.transport_connection_id = std::move(cnx_id_bytes);
            ctx->transportManager->recvDataFromNet(data, std::move(peer_info));
            ret = 0;
            break;
        }
        default:
            assert(0);
    }

    return ret;
}

// TODO: can this be made instance member
int picoquic_server_callback(picoquic_cnx_t *cnx,
                             uint64_t stream_id,
                             uint8_t *bytes,
                             size_t length,
                             picoquic_call_back_event_t fin_or_event,
                             void *callback_ctx,
                             void *v_stream_ctx)
{
    return NetTransportQUIC::datagram_callback(
        cnx, stream_id, bytes, length, fin_or_event, callback_ctx, v_stream_ctx);
}

NetTransportQUIC::~NetTransportQUIC()
{
    close();
}

void NetTransportQUIC::close()
{
    // TODO: implement graceful shutdown
    assert(0);
}

bool NetTransportQUIC::doRecvs()
{
    return false;
}

bool NetTransportQUIC::doSends()
{
    return false;
}

///
///  Private Implementation
///

void NetTransportQUIC::add_connection(bytes &conn_id, picoquic_cnx_t *conn)
{
    if (!connections.count(conn_id))
    {
        std::cout << conn << ": Connection Saved " << to_hex(conn_id)
                  << std::endl;
        connections.emplace(conn_id, conn);
    }
}

void NetTransportQUIC::remove_connection(const bytes &conn_id)
{
    const auto &pos = connections.find(conn_id);
    if (pos != connections.end())
    {
        std::cout << "Connection Removed " << to_hex(conn_id) << std::endl;
        connections.erase(pos);
    }
}

int NetTransportQUIC::quic_start_connection()
{
    // create client connection context
    std::cout << "starting client connection to " << quic_client_ctx.sni
              << std::endl;
    client_cnx = picoquic_create_cnx(
        quicHandle,
        picoquic_null_connection_id,
        picoquic_null_connection_id,
        (struct sockaddr *) &quic_client_ctx.server_address,
        picoquic_get_quic_time(quicHandle),
        PICOQUIC_TWENTIETH_INTEROP_VERSION,
        quic_client_ctx.sni.data(),
        alpn.data(),
        1);

    if (client_cnx == nullptr)
    {
        return -1;
    }

    std::cout << "cnx proposed version " << client_cnx->proposed_version
              << std::endl;

    // context to be used on callback
    auto *xport_ctx = new TransportContext{};
    xport_ctx->transportManager = transportManager;
    xport_ctx->transport = this;
    picoquic_set_callback(client_cnx, datagram_callback, (void *) xport_ctx);

    // enable quic datagram mode
    client_cnx->local_parameters.max_datagram_frame_size = 1500;

    return picoquic_start_client_cnx(client_cnx);
}

// Client Transport
NetTransportQUIC::NetTransportQUIC(TransportManager *t,
                                   std::string sfuName,
                                   uint16_t sfuPort) :
    transportManager(t), quicConnectionReady(false), m_isServer(false)
{
    std::cout << "Quic Client Transport" << std::endl;
    udp_socket = new NetTransportUDP{nullptr, sfuName, sfuPort};
    assert(udp_socket);

    // TODO: remove the duplication
    std::string sPort = std::to_string(htons(sfuPort));
    struct addrinfo hints = {}, *address_list = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    int err = getaddrinfo(
        sfuName.c_str(), sPort.c_str(), &hints, &address_list);
    if (err)
    {
        assert(0);
    }
    struct addrinfo *item = nullptr, *found_addr = nullptr;
    for (item = address_list; item != nullptr; item = item->ai_next)
    {
        if (item->ai_family == AF_INET && item->ai_socktype == SOCK_DGRAM &&
            item->ai_protocol == IPPROTO_UDP)
        {
            found_addr = item;
            break;
        }
    }

    if (found_addr == nullptr)
    {
        assert(0);
    }

    struct sockaddr_in *ipv4_dest =
        (struct sockaddr_in *) &quic_client_ctx.server_address;
    memcpy(ipv4_dest, found_addr->ai_addr, found_addr->ai_addrlen);
    ipv4_dest->sin_port = htons(sfuPort);
    quic_client_ctx.server_address_len = sizeof(quic_client_ctx.server_address);
    quic_client_ctx.server_name = sfuName;
    quic_client_ctx.port = sfuPort;

    // create quic client context
    auto ticket_store_filename = "token-store.bin";

    auto ctx = new TransportContext{};
    ctx->transport = this;
    ctx->transportManager = transportManager;

    /* Create QUIC context */
    quicHandle = picoquic_create(1,
                                 NULL,
                                 NULL,
                                 NULL,
                                 alpn.data(),
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 picoquic_current_time(),
                                 NULL,
                                 "ticket-store.bin",
                                 NULL,
                                 0);

    assert(quicHandle != nullptr);

    picoquic_set_default_congestion_algorithm(quicHandle,
                                              picoquic_bbr_algorithm);

    if (picoquic_load_retry_tokens(quicHandle, ticket_store_filename) != 0)
    {
        fprintf(stderr,
                "No token file present. Will create one as <%s>.\n",
                ticket_store_filename);
    }

    (void) picoquic_set_default_connection_id_length(quicHandle, 4);

    // logging, TODO: configure to on/off
    // picoquic_set_textlog(quicHandle, "clientlog.txt");
    picoquic_set_log_level(quicHandle, 2);

    udp_socket = new NetTransportUDP{nullptr, sfuName, sfuPort};
    assert(udp_socket);

    // start the quic thread
    quicTransportThread = std::thread(quicTransportThreadFunc, this);
}

// server
NetTransportQUIC::NetTransportQUIC(TransportManager *t, uint16_t sfuPort) :
    transportManager(t), quicConnectionReady(false), m_isServer(true)

{
    std::cout << "Quic Server Transport" << std::endl;
    char default_server_cert_file[512];
    char default_server_key_file[512];
    const char *server_cert_file = nullptr;
    const char *server_key_file = nullptr;

    picoquic_get_input_path(default_server_cert_file,
                            sizeof(default_server_cert_file),
                            "/tmp",
                            SERVER_CERT_FILE);
    server_cert_file = default_server_cert_file;

    picoquic_get_input_path(default_server_key_file,
                            sizeof(default_server_key_file),
                            "/tmp",
                            SERVER_KEY_FILE);
    server_key_file = default_server_key_file;

    auto ctx = new TransportContext{};
    ctx->transport = this;
    ctx->transportManager = transportManager;

    quicHandle = picoquic_create(1,
                                 server_cert_file,
                                 server_key_file,
                                 NULL,
                                 alpn.data(),
                                 picoquic_server_callback,
                                 ctx,
                                 NULL,
                                 NULL,
                                 NULL,
                                 picoquic_current_time(),
                                 NULL,
                                 NULL,
                                 NULL,
                                 0);

    assert(quicHandle != nullptr);

    picoquic_set_alpn_select_fn(quicHandle, select_alpn);
    picoquic_set_default_congestion_algorithm(quicHandle,
                                              picoquic_bbr_algorithm);
    // logging, TODO: configure to on/off
    // picoquic_set_textlog(quicHandle, "serverlog.txt");
    picoquic_set_log_level(quicHandle, 2);

    udp_socket = new NetTransportUDP{nullptr, sfuPort};

    // kickoff quic process
    quicTransportThread = std::thread(quicTransportThreadFunc, this);
}

bool NetTransportQUIC::ready()
{
    bool ret;
    {
        std::lock_guard<std::mutex> lock(quicConnectionReadyMutex);
        ret = quicConnectionReady;
    }
    if (ret)
    {
        std::cout << "NetTransportQUIC::ready()" << std::endl;
    }
    return ret;
}

// Main quic process thread
// 1. check for incoming packets
// 2. check for outgoing packets
int NetTransportQUIC::runQuicProcess()
{
    // create the quic client connection context
    if (!m_isServer)
    {
        auto ret = quic_start_connection();
        assert(ret == 0);
        std::cout << "Quic client connection started ...\n";
    }

    picoquic_quic_t *quic = quicHandle;
    int if_index = 0;
    picoquic_connection_id_t log_cid;
    picoquic_cnx_t *last_cnx = nullptr;

    while (!transportManager->shutDown)
    {
        Data packet;
        //  call to next wake delay, pass it to select()
        auto got = udp_socket->read(packet);
        if (got)
        {
            if (local_port == 0)
            {
                if (picoquic_get_local_address(udp_socket->fd,
                                               &local_address) != 0)
                {
                    memset(&local_address, 0, sizeof(struct sockaddr_storage));
                    fprintf(stderr, "Could not read local address.\n");
                }
                // todo: support AF_INET6
                local_port = ((struct sockaddr_in *) &local_address)->sin_port;
                std::cout << "Found local port  " << local_port << std::endl;
            }

            // std::cout << "Recvd data from net:" << packet.data.size()
            //          << " bytes\n";
            // let the quic stack know of the incoming packet
            uint64_t curr_time = picoquic_get_quic_time(quicHandle);

            // print_sock_info("incoming: peer: ", &packet.peer.addr);
            // print_sock_info("incoming: local: ", &local_address);

            int ret = picoquic_incoming_packet(
                quic,
                reinterpret_cast<uint8_t *>(packet.data.data()),
                packet.data.size(),
                (struct sockaddr *) &packet.peer.addr,
                (struct sockaddr *) &local_address,
                -1,
                0,
                curr_time);
            assert(ret == 0);
        }

        // dequeue from the application
        Data send_packet;
        std::string data;
        NetTransport::PeerConnectionInfo peer_info;
        data.resize(1500);
        got = transportManager->getDataToSendToNet(
            data, &peer_info, &send_packet.peer.addrLen);
        uint64_t curr_time_send = picoquic_current_time();
        if (got)
        {
            picoquic_cnx_t *peer_cnx = nullptr;
            if (!peer_info.transport_connection_id.empty())
            {
                auto &cnx_id = peer_info.transport_connection_id;
                if (!connections.count(cnx_id))
                {
                    std::cerr << "ConnectionId not found in map "
                              << to_hex(cnx_id) << std::endl;
                    if (!m_isServer)
                    {
                        // need to know why connection id gets changed, is it
                        // migration ?
                        auto *cnx = connections.at(local_connection_id);
                        remove_connection(local_connection_id);
                        add_connection(cnx_id, cnx);
                    }
                    else
                    {
                        assert(0);
                    }
                }        // cnxId miss

                peer_cnx = connections.at(cnx_id);
                /*
                auto peer_cnx_id = picoquic_get_client_cnxid(peer_cnx);
                auto cnx_id_bytes = bytes(peer_cnx_id.id,
                                          peer_cnx_id.id + peer_cnx_id.id_len);
                std::cout << "peer_cnx: " << peer_cnx
                          << ", cnxId:" << to_hex(cnx_id)
                          << ", size: " << data.size() << std::endl;
                */
            }
            else
            {
                peer_cnx = connections.at(local_connection_id);
                // std::cout << "local:enqueueing datagram: using peer_info cnx:
                // cnxId: " << to_hex(local_connection_id)
                //          << ", size: " << data.size() << std::endl;
            }

            int ret = picoquic_queue_datagram_frame(
                peer_cnx,
                data.size(),
                reinterpret_cast<const uint8_t *>(data.data()));
            assert(ret == 0);
        }

        // verify if there are any packets from the underlying quic context
        size_t send_length = 0;
        std::string send_buffer;
        send_buffer.resize(1500);
        struct sockaddr_storage peer_addr;
        struct sockaddr_storage local_addr;

        int ret = picoquic_prepare_next_packet(
            quicHandle,
            curr_time_send,
            reinterpret_cast<uint8_t *>(send_buffer.data()),
            send_buffer.size(),
            &send_length,
            &peer_addr,
            &local_addr,
            &if_index,
            &log_cid,
            &last_cnx);

        assert(ret == 0);

        if (send_length > 0)
        {
            // std::cout << "Sending data returned  by quic: " << send_length
            //          << " bytes\n";
            send_buffer.resize(send_length);
            send_packet.data = std::move(send_buffer);
            if (!m_isServer)
            {
                // std::cout << " sending to sfu\n";
                // client sending first packet
                send_packet.peer.addrLen = udp_socket->sfuAddrLen;
                memcpy(&send_packet.peer.addr,
                       &udp_socket->sfuAddr,
                       udp_socket->sfuAddrLen);
            }
            else
            {
                // print_sock_info("sending to: ", &send_packet.peer.addr);
                send_packet.peer.addrLen = sizeof(peer_addr);
                memcpy(&send_packet.peer.addr, &peer_addr, sizeof(peer_addr));
            }

            udp_socket->write(send_packet);
        }
    }        // !transport_shutdown

    std::cout << "DONE" << std::endl;
    assert(0);
    // return true;
}

#endif