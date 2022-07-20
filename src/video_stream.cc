#include "media_stream.hh"
#include "h264_encoder.hh"
#include "h264_decoder.hh"
namespace qmedia
{

VideoStream::VideoStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         const MediaConfig &media_config,
                         LoggerPointer logger_in) :
    MediaStream(domain, conference_id, client_id, media_config, logger_in)
{
}

void VideoStream::configure()
{
    media_direction = config.media_direction;

    switch (config.media_direction)
    {
        case MediaConfig::MediaDirection::recvonly:
            // setup decoder
            break;
        case MediaConfig::MediaDirection::sendonly:
            // setup encoder
            encoder = std::make_unique<H264Encoder>(
                config.video_max_width,
                config.video_max_height,
                config.video_max_frame_rate,
                config.video_max_bitrate,
                (uint32_t) config.video_encode_pixel_format,
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

                auto ret = Packet::encode(encoded.get(), encoded->encoded_data);
                if (!ret)
                {
                    // log err
                    return;
                }

                media_transport->send_data(id(),
                                           std::move(encoded->encoded_data));
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

size_t VideoStream::get_media(uint64_t &timestamp,
                              MediaConfig &config,
                              unsigned char **buffer,
                              unsigned int /*max_len*/)
{
    size_t recv_length = 0;
    logger->debug << "GetvideoFrame called" << std::flush;

    auto jitter = getJitter(client_id);
    if (jitter == nullptr)
    {
        logger->warning << "[VideoStream::get_media]: jitter is nullptr"
                        << std::flush;
        return 0;
    }

    uint32_t pixel_format;
    recv_length = jitter->popVideo(id(),
                                   config.video_max_width,
                                   config.video_max_height,
                                   pixel_format,
                                   timestamp,
                                   buffer);

    config.video_decode_pixel_format = (VideoConfig::PixelFormat) pixel_format;

    return recv_length;
}

///
/// Private
///

PacketPointer VideoStream::encode_h264(uint8_t *buffer,
                                       unsigned int length,
                                       uint64_t timestamp,
                                       const MediaConfig &media_config)
{
    std::vector<uint8_t> output;

    if (encoder == nullptr)
    {
        logger->warning << "Video Encoder, unavailable" << std::flush;
        return nullptr;
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

    if (encoded_frame_type == VideoEncoder::EncodedFrameType::Skip ||
        encoded_frame_type == VideoEncoder::EncodedFrameType::Invalid)
    {
        logger->info << "Encoded Frame Type ignored due "
                     << (int) encoded_frame_type << std::flush;
        return nullptr;
    }

    logger->info << "Encoded Frame Type is  " << (int) encoded_frame_type
                 << std::flush;

    auto packet = std::make_unique<Packet>();

    packet->is_intra_frame = encoded_frame_type ==
                             VideoEncoder::EncodedFrameType::IDR;

    packet->data = std::move(output);
    packet->clientID = client_id;
    // todo : fix this to not be hardcoded
    packet->mediaType = Packet::MediaType::H264;
    packet->sourceID = id();        // same as streamId
    packet->encodedSequenceNum = encode_sequence_num;
    encode_sequence_num += 1;
    return packet;
}

}        // namespace qmedia