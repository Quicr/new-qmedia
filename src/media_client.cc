#include <memory>
#include <sstream>

#include <qmedia/media_client.hh>
#include "media_transport.hh"
#include "media_stream.hh"
#include "packet.hh"

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
    media_transport = std::make_shared<QuicRMediaTransport>(
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
        media_transport->wait_for_messages();
        auto message = media_transport->recv();
        if (message.data.empty())
        {
            log->info << "[MediaClient::do_work]: Message data is empty" << std::flush;
            continue;
        }

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

        if (message.data.size() > 100)
        {
            log->info << "[MediaClient::do_work]: got message for "
                      << media_stream_id << " data:" << message.data.size()
                      << std::flush;
        }

        // hand the data to appropriate media stream
        active_streams[media_stream_id]->handle_media(new_stream_callback,
                                                      message.group_id,
                                                      message.object_id,
                                                      std::move(message.data));

    }
}

int MediaClient::get_audio(MediaStreamId streamId,
                           uint64_t &timestamp,
                           unsigned char **buffer,
                           unsigned int max_len,
                           void  **to_free)
{
    uint32_t recv_length = 0;
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        // log and return
        return 0;
    }

    log->debug << "MediaClient::get_audio:" << streamId << std::flush;

    auto audio_stream = std::dynamic_pointer_cast<AudioStream>(
        active_streams[streamId]);
    MediaConfig config{};
    return audio_stream->get_media(timestamp, config, buffer, max_len, to_free);
}

std::uint32_t MediaClient::get_video(MediaStreamId streamId,
                                     uint64_t &timestamp,
                                     uint32_t &width,
                                     uint32_t &height,
                                     uint32_t &format,
                                     unsigned char **buffer,
                                     void **to_free)
{
    uint32_t recv_length = 0;
    // happens on client thread
    if (!active_streams.count(streamId))
    {
        log->warning << "[MediaClient::get_video]: media stream inactive" << std::flush;
        return 0;
    }

    auto video_stream = std::dynamic_pointer_cast<VideoStream>(
        active_streams[streamId]);

    MediaConfig config{};
    recv_length = video_stream->get_media(timestamp, config, buffer, 0, to_free);
    width = config.video_max_width;
    height = config.video_max_height;
    format = (uint32_t) config.video_decode_pixel_format;

    log->info << "MediaClient::get_video: w=" << width
              << ",h=" << height << ",format=" << format
              << std::flush;

    return recv_length;
}

void MediaClient::release_media_buffer(void *buffer)
{
    delete((Packet*) buffer);
}

}        // namespace qmedia
