#include "media_transport.hh"

namespace qmedia
{

///
/// Delegate
///

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
    std::lock_guard<std::mutex> lock(queue_mutex);
    receive_queue.push(TransportMessageInfo{name, group_id, object_id, data});
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

void Delegate::get_queued_messages(
    std::vector<TransportMessageInfo> &messages_out)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (receive_queue.empty())
    {
        return;
    }

    while (!receive_queue.empty())
    {
        auto data = receive_queue.front();
        messages_out.push_back(data);
        receive_queue.pop();
    }
}

///
///  MediaTransport
///

MediaTransport::MediaTransport(const std::string &server_ip,
                               const uint16_t port,
                               LoggerPointer logger_in) :
    qr_client(delegate, server_ip, port), logger(logger_in)
{
    while (!qr_client.is_transport_ready())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    logger->info << "[MediaTransport]: Transport Created" << std::flush;
    delegate.set_logger(logger);
}

void MediaTransport::register_stream(uint64_t id,
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

void MediaTransport::send_data(uint64_t id, quicr::bytes &&data)
{
    auto qname = quicr::QuicrName{std::to_string(id), 0};
    qr_client.publish_named_data(qname.name, std::move(data), 0, 0);
}

void MediaTransport::check_network_messages(
    std::vector<TransportMessageInfo> &messages_out)
{
    delegate.get_queued_messages(messages_out);
}

}        // namespace qmedia