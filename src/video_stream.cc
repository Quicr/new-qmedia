#include "media_stream.hh"
#include "h264_encoder.hh"
#include "h264_decoder.hh"
namespace qmedia
{

VideoStream::VideoStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         LoggerPointer logger_in) :
    MediaStream(domain, conference_id, client_id, logger_in)
{
}

MediaStreamId VideoStream::id()
{
    if (media_stream_id)
    {
        return media_stream_id;
    }

    auto name = QuicrName::name_for_client(domain, conference_id, client_id);
    auto quality_id = "video/" + std::to_string((int) config.media_codec) +
                      "/" + std::to_string(config.video_max_frame_rate) + "/" +
                      std::to_string(config.video_max_bitrate);
    name += quality_id;
    std::hash<std::string> hasher;
    media_stream_id = hasher(name);
    return media_stream_id;
}

void VideoStream::set_config(const MediaConfig &video_config)
{
    config = video_config;
    media_direction = video_config.media_direction;

    switch (media_direction)
    {
        case MediaConfig::MediaDirection::recvonly:
            // setup decoder
            break;
        case MediaConfig::MediaDirection::sendonly:
            // setup encoder
            encoder = std::make_unique<H264Encoder>(
                config.video_max_width,
                config.video_max_height,
                video_config.video_max_frame_rate,
                video_config.video_max_bitrate,
                (uint32_t) video_config.video_encode_pixel_format,
                logger);

            if (!encoder)
            {
                logger->error << " video encoder init failed" << std::flush;
                return;
            }
            break;
        case MediaConfig::MediaDirection::sendrecv:
            // setup encoder and decoder
            break;
        case MediaConfig::MediaDirection::unknown:
        default:
            assert("Invalid media direction");
    }
}

void VideoStream::handle_media(MediaConfig::CodecType codec_type,
                               uint8_t *buffer,
                               unsigned int length,
                               uint64_t timestamp,
                               const MediaConfig &media_config)
{
    if (!buffer || !length || !timestamp)
    {
        // malformed, throw error
        return;
    }

    switch (codec_type)
    {
        case MediaConfig::CodecType::h264:
        {
            if (encoder != nullptr)
            {
                auto encoded = encode_h264(
                    buffer, length, timestamp, media_config);

                auto packet = std::make_unique<Packet>();
                packet->data = std::move(encoded);
                packet->clientID = client_id;
                // todo : fix this to not be hardcoded
                packet->mediaType = Packet::MediaType::H264;
                packet->sourceID = id();        // same as streamId
                auto ret = Packet::encode(packet.get(), packet->encoded_data);
                if (!ret)
                {
                    // log err
                    return;
                }

                media_transport->send_data(id(),
                                           std::move(packet->encoded_data));
            }
        }
        break;
        case MediaConfig::CodecType::raw:
        {
            // decode and send to render
            // decode(codec_type, *buffer, length, timestamp)
        }
        break;
        default:
            assert("Incorrect codec type");
    }
}

void VideoStream::handle_media(uint64_t group_id,
                               uint64_t object_id,
                               std::vector<uint8_t> &&bytes)
{
}

///
/// Private
///

std::vector<uint8_t> VideoStream::encode_h264(uint8_t *buffer,
                                              unsigned int length,
                                              uint64_t timestamp,
                                              const MediaConfig &media_config)
{
    std::vector<uint8_t> output;

    if (encoder == nullptr)
    {
        logger->warning << "Video Encoder, unavailable" << std::flush;
        return output;
    }

    int sendRaw = 0;
    // bool keyFrame = reqKeyFrame;
    auto encoded_frame_type = encoder->encode(
        reinterpret_cast<const char *>(buffer),
        length,
        media_config.video_max_width,
        media_config.video_max_height,
        media_config.stride_y,
        media_config.stride_uv,
        media_config.offset_u,
        media_config.offset_v,
        (unsigned int) media_config.video_encode_pixel_format,
        timestamp,
        output,
        false);

    return output;
}

}        // namespace qmedia