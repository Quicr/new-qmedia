#include "media_transport.hh"

namespace qmedia
{

///
/// Delegate
///

Delegate::Delegate(TransportMessageHandler* handler)
: message_handler(handler){}

void Delegate::set_logger(LoggerPointer logger_in)
{
    logger = logger_in;
}

void Delegate::on_data_arrived(const std::string &name,
                               quicr::bytes &&data,
                               std::uint64_t group_id,
                               std::uint64_t object_id)
{
    log(quicr::LogLevel::debug, "on_data_arrived: " + name);
    if(message_handler) {
        message_handler->handle(TransportMessageInfo{name, group_id, object_id, data});
    }
}

void Delegate::on_connection_close(const std::string &name)
{
    log(quicr::LogLevel::info, "[Delegate] Media Connection Closed: " + name);
    // trigger a resubscribe
}

void Delegate::on_object_published(const std::string &name,
                                   uint64_t group_id,
                                   uint64_t object_id)
{
    if (logger)
    {
        logger->debug << "[Delegate::on_object_published]" << name << ", group "
                      << group_id << ", object:" << object_id << std::flush;
    }
}

void Delegate::log(quicr::LogLevel /*level*/, const std::string &message)
{
    // todo: add support for inserting logger
    if (logger)
    {
        logger->debug << message << std::flush;
    }
    // std::clog << message << std::endl;
}

///
///  QuicRMediaTransport
///

QuicRMediaTransport::QuicRMediaTransport(const std::string &server_ip,
                               const uint16_t port,
                               LoggerPointer logger_in) :
    delegate(this),
    qr_client(delegate, server_ip, port), logger(logger_in)
{
    while (!qr_client.is_transport_ready())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    logger->info << "[MediaTransport]: Transport Created" << std::flush;
    delegate.set_logger(logger);
}

void QuicRMediaTransport::register_stream(uint64_t id,
                                     MediaConfig::MediaDirection direction)
{
    logger->info << "[MediaTransport]: register_stream " << id << std::flush;
    auto qname = quicr::QuicrName{std::to_string(id), 0};
    if (direction == MediaConfig::MediaDirection::sendonly)
    {
        qr_client.register_names({qname}, false);
    }
    else if (direction == MediaConfig::MediaDirection::recvonly)
    {
        qr_client.subscribe({qname}, false, true);
    }
    else
    {
        qr_client.register_names({qname}, false);
        qr_client.subscribe({qname}, false, true);
    }
}

void QuicRMediaTransport::send_data(uint64_t id, quicr::bytes &&data)
{
    auto qname = quicr::QuicrName{std::to_string(id), 0};
    qr_client.publish_named_data(qname.name, std::move(data), 0, 0);
}

void QuicRMediaTransport::wait_for_messages()
{
    std::unique_lock<std::mutex> ulock(recv_queue_mutex);
    recv_cv.wait(ulock, [&]() -> bool {
                     return (shutdown || !receive_queue.empty());
                 });
    ulock.unlock();
}

TransportMessageInfo QuicRMediaTransport::recv()
{
    TransportMessageInfo info;
    {
        std::lock_guard<std::mutex> lock(recv_queue_mutex);
        if (!receive_queue.empty())
        {
            info = std::move(receive_queue.front());
            receive_queue.pop();
        }
    }
    return info;
}

void QuicRMediaTransport::handle(TransportMessageInfo &&info)
{
    // add to recv q and notify
    {
        std::lock_guard<std::mutex> lock(recv_queue_mutex);
        receive_queue.push(std::move(info));
    }

    recv_cv.notify_all();
}

}        // namespace qmedia