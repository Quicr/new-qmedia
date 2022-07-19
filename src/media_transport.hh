#pragma once
#include <queue>

#include <qmedia/media_client.hh>
#include "quicr/quicr_client.h"

namespace qmedia
{

// Data exported by the underling quicr transport
struct TransportMessageInfo
{
    std::string name;
    std::uint64_t group_id;
    std::uint64_t object_id;
    quicr::bytes data;
};

struct Delegate : public quicr::QuicRClient::Delegate
{
    virtual void on_data_arrived(const std::string &name,
                                 quicr::bytes &&data,
                                 std::uint64_t group_id,
                                 std::uint64_t object_id) override;
    virtual void on_connection_close(const std::string &name) override;
    virtual void on_object_published(const std::string &name,
                                     uint64_t group_id,
                                     uint64_t object_id) override;
    virtual void log(quicr::LogLevel level,
                     const std::string &message) override;
    void get_queued_messages(std::vector<TransportMessageInfo> &messages_out);

    void set_logger(LoggerPointer logger_in);

private:
    std::mutex queue_mutex;
    std::queue<TransportMessageInfo> receive_queue;
    LoggerPointer logger;
};

// Wrapper around quicr::QuicRClient
struct MediaTransport
{
    explicit MediaTransport(const std::string &server_ip,
                            const uint16_t port,
                            LoggerPointer logger_in);
    ~MediaTransport() = default;

    void register_stream(uint64_t id);

    void send_data(uint64_t id, quicr::bytes &&data);

    // special function
    void check_network_messages(std::vector<TransportMessageInfo> &messages_out);

private:
    Delegate delegate;
    quicr::QuicRClient qr_client;
    LoggerPointer logger;
};

}        // namespace qmedia