#include <memory>

#include <qmedia/media_client.hh>
#include "media_transport.hh"
#include "media_stream.hh"

namespace qmedia
{

MediaClient::MediaClient(NewSourceCallback stream_callback,
                         const LoggerPointer &parent_logger) :
    new_stream_callback(stream_callback), log(parent_logger)
{
}

void MediaClient::init_transport(TransportType /*transport_type*/,
                                 const std::string &remote_address,
                                 unsigned int remote_port)
{
    media_transport = std::make_shared<MediaTransport>(remote_address,
                                                       remote_port);
}

MediaStreamId MediaClient::add_audio_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const MediaConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_audio_stream(
        domain, conference_id, client_id, media_config);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
}

MediaStreamId MediaClient::add_video_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const MediaConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_video_stream(
        domain, conference_id, client_id, media_config);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
}

// media apis
void MediaClient::send_audio(MediaStreamId streamId,
                             uint8_t *buffer,
                             unsigned int length,
                             uint64_t timestamp)
{
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return;
    }

    auto audio_stream = std::dynamic_pointer_cast<AudioStream>(
        active_streams[streamId]);
    if (audio_stream)
    {
        // revisit this
        MediaConfig config;
        audio_stream->handle_media(
            MediaConfig::CodecType::raw, buffer, length, timestamp, config);
    }
}

void MediaClient::send_video(MediaStreamId streamId,
                             uint8_t *buffer,
                             uint32_t length,
                             uint32_t width,
                             uint32_t height,
                             uint32_t stride_y,
                             uint32_t stride_uv,
                             uint32_t offset_u,
                             uint32_t offset_v,
                             uint32_t format,
                             uint64_t timestamp)
{
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return;
    }

    auto video_stream = std::dynamic_pointer_cast<VideoStream>(
        active_streams[streamId]);
    if (video_stream)
    {
        // revisit this
        MediaConfig config;
        video_stream->handle_media(
            MediaConfig::CodecType::raw, buffer, length, timestamp, config);
    }
}

void MediaClient::do_work()
{
    // Wait on a condition variable
    while (!shutdown)
    {
        auto messages = std::vector<TransportMessageInfo>{};
        media_transport->check_network_messages(messages);

        if (messages.empty())
        {
            continue;
        }

        for (auto &message : messages)
        {
            auto media_stream_id = std::stoi(message.name);
            //  Note: since a subscribe should preceed before
            // data arrive, we should find an entry when
            // there is data for a given stream
            if (!active_streams.count(media_stream_id))
            {
                log->warning << media_stream_id << " not found, ignoring data"
                             << std::flush;
                continue;
            }

            // hand the data to appropriate media stream
            active_streams[media_stream_id]->handle_media(
                message.group_id, message.object_id, std::move(message.data));
        }
    }
}

int MediaClient::get_audio(MediaStreamId streamId,
                           uint64_t &timestamp,
                           unsigned char **buffer,
                           unsigned int max_len)
{
    return -1;
}

std::uint32_t MediaClient::get_video(MediaStreamId streamId,
                                     uint64_t &timestamp,
                                     uint32_t &width,
                                     uint32_t &height,
                                     uint32_t &format,
                                     unsigned char **buffer)
{
    return -1;
}

}        // namespace qmedia
