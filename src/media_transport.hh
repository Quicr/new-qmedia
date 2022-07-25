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

struct TransportMessageHandler {
    virtual ~TransportMessageHandler() = default;
    virtual void handle(TransportMessageInfo&& info) = 0;
};


struct Delegate : public quicr::QuicRClient::Delegate
{
    Delegate(TransportMessageHandler* handler);

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


    void set_logger(LoggerPointer logger_in);

private:
    LoggerPointer logger;
    TransportMessageHandler* message_handler;
};

struct MediaTransport : TransportMessageHandler {
    virtual ~MediaTransport() = default;
    virtual void register_stream(uint64_t id, MediaConfig::MediaDirection direction) = 0;
    virtual void send_data(uint64_t id, quicr::bytes &&data) = 0;
    virtual void wait_for_messages() = 0;
    virtual TransportMessageInfo recv() = 0;
};

// Wrapper around quicr::QuicRClient
struct QuicRMediaTransport : public MediaTransport
{
    explicit QuicRMediaTransport(const std::string &server_ip,
                            const uint16_t port,
                            LoggerPointer logger_in);
    ~QuicRMediaTransport() = default;

    virtual void register_stream(uint64_t id, MediaConfig::MediaDirection direction) override;

    virtual void send_data(uint64_t id, quicr::bytes &&data) override;

    virtual void wait_for_messages() override;

    virtual TransportMessageInfo recv() override;

    virtual void handle(TransportMessageInfo&& info) override;

private:
    Delegate delegate;
    quicr::QuicRClient qr_client;
    LoggerPointer logger;
    std::mutex recv_queue_mutex;
    std::queue<TransportMessageInfo> receive_queue;

    std::mutex recv_audio_queue_mutex;
    std::queue<TransportMessageInfo> receive_audio_queue;

    std::mutex recv_video_queue_mutex;
    std::queue<TransportMessageInfo> receive_video_queue;

    std::condition_variable recv_cv;
    bool shutdown = false;
};

}        // namespace qmedia