#include <memory>
#include <sstream>

#include <qmedia/media_client.hh>
#include "media_transport.hh"
#include "media_stream.hh"

namespace qmedia
{

MediaClient::MediaClient(NewSourceCallback stream_callback,
                         const LoggerPointer &parent_logger) :
    new_stream_callback(stream_callback),
    log(std::make_shared<Logger>("qmedia", parent_logger))
{
}

void MediaClient::init_transport(TransportType /*transport_type*/,
                                 const std::string &remote_address,
                                 unsigned int remote_port)
{
    media_transport = std::make_shared<MediaTransport>(
        remote_address, remote_port, log);

    work_thread = std::thread(start_work_thread, this);
}

MediaStreamId MediaClient::add_audio_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const MediaConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_audio_stream(
        domain, conference_id, client_id, media_config, log);

    log->info << "[MediaClient::add_audio_stream]: created: "
              << media_stream->id() << std::flush;

    media_transport->register_stream(media_stream->id(),
                                     media_config.media_direction);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
    return media_stream->id();
}

MediaStreamId MediaClient::add_video_stream(uint64_t domain,
                                            uint64_t conference_id,
                                            uint64_t client_id,
                                            const MediaConfig &media_config)
{
    // create a new media stream and associate the transport
    auto media_stream = MediaStreamFactory::create_video_stream(
        domain, conference_id, client_id, media_config, log);

    log->info << "[MediaClient::add_video_stream]: created: "
              << media_stream->id() << std::flush;
    media_transport->register_stream(media_stream->id(),
                                     media_config.media_direction);
    media_stream->set_transport(media_transport);
    active_streams[media_stream->id()] = media_stream;
    return media_stream->id();
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
        MediaConfig config{};
        config.video_max_width = width;
        config.video_max_height = height;
        config.offset_u = offset_u;
        config.offset_v = offset_v;
        config.stride_y = stride_y;
        config.stride_uv = stride_uv;
        // revisit this
        config.video_encode_pixel_format = VideoConfig::PixelFormat::I420;
        video_stream->handle_media(
            MediaConfig::CodecType::h264, buffer, length, timestamp, config);
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
            MediaStreamId media_stream_id{0};
            std::istringstream iss(message.name);
            iss >> media_stream_id;
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
            active_streams[media_stream_id]->handle_media(new_stream_callback,
                                                          message.group_id,
                                                          message.object_id,
                                                          std::move(message.data));
        }
    }
}

int MediaClient::get_audio(MediaStreamId streamId,
                           uint64_t &timestamp,
                           unsigned char **buffer,
                           unsigned int max_len)
{
    uint32_t recv_length = 0;
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return 0;
    }

    auto audio_stream = std::dynamic_pointer_cast<AudioStream>(
        active_streams[streamId]);
    MediaConfig config{};
    recv_length = audio_stream->get_media(timestamp, config, buffer, max_len);

}

std::uint32_t MediaClient::get_video(MediaStreamId streamId,
                                     uint64_t &timestamp,
                                     uint32_t &width,
                                     uint32_t &height,
                                     uint32_t &format,
                                     unsigned char **buffer)
{
    uint32_t recv_length = 0;
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return 0;
    }

    auto video_stream = std::dynamic_pointer_cast<VideoStream>(
        active_streams[streamId]);

    MediaConfig config{};
    recv_length = video_stream->get_media(timestamp, config, buffer, 0);
    width = config.video_max_width;
    height = config.video_max_height;
    format = (uint32_t) config.video_decode_pixel_format;
    return recv_length;
}

}        // namespace qmedia
