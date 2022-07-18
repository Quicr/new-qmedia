#include "media_stream.hh"
#include "packet.hh"
namespace qmedia
{

///
/// Audio Stream Api
///

MediaStreamId AudioStream::id()
{
    if (media_stream_id) {
        return  media_stream_id;
    }

    auto name = QuicrName::name_for_client(domain, conference_id, client_id);
    auto quality_id = "audio/"
                      + std::to_string((int)config.media_codec) + "/"
                      + std::to_string(config.sample_rate) + "/"
                      + std::to_string(config.channels);
    name += quality_id;
    std::hash<std::string> hasher;
    media_stream_id = hasher(name);
    return media_stream_id;
}

AudioStream::AudioStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id) :
    MediaStream(domain, conference_id, client_id)
{
}

void AudioStream::set_config(const MediaConfig &audio_config)
{
    config = audio_config;
    media_direction = audio_config.media_direction;
    switch (media_direction)
    {
        case MediaConfig::MediaDirection::recvonly:
            // setup decoder
            break;
        case MediaConfig::MediaDirection::sendonly:
            getAudioEncoder();
            break;
        case MediaConfig::MediaDirection::sendrecv:
            // setup encoder and decoder
            break;
        case MediaConfig::MediaDirection::unknown:
        default:
            assert("Invalid media direction");
    }
}

void AudioStream::handle_media(MediaConfig::CodecType codec_type,
                               uint8_t *buffer,
                               unsigned int length,
                               uint64_t timestamp,
                               const MediaConfig& /*media_config*/)
{
    if (!buffer || !length || !timestamp)
    {
        // malformed, throw error
        return;
    }

    switch (codec_type)
    {
        case MediaConfig::CodecType::raw:
        {
            // encode and send to transport
            auto encoder = getAudioEncoder();
            if (encoder != nullptr)
            {
                logger->debug << "sendAudio: SourceId:" << id()
                              << ", length:" << length << std::flush;
                encoder->encodeFrame(
                    buffer, length, timestamp, mutedAudioEmptyFrames);
            }
        }
        break;
        case MediaConfig::CodecType::opus:
        {
            // decode and send to render
            // decode(codec_type, *buffer, length, timestamp)
        }
        break;
        default:
            assert("Incorrect codec type");
    }
}

///
/// Private
///

void AudioStream::audio_encoder_callback(std::vector<uint8_t> &&bytes)
{
    logger->debug << id() << "Opus Encoded Audio Size:" << bytes.size()
                  << std::flush;
    auto packet = std::make_unique<Packet>();
    packet->data = std::move(bytes);
    packet->clientID = client_id;
    // todo : fix this to not be hardcoded
    packet->mediaType = Packet::MediaType::Opus;
    packet->sourceID = id(); // same as streamId
    auto ret = Packet::encode(packet.get(), packet->encoded_data);
    if(!ret) {
        // log err
        return;
    }

    media_transport->send_data(id(), std::move(packet->encoded_data));
}

std::shared_ptr<AudioEncoder> AudioStream::getAudioEncoder()
{
    if (encoder == nullptr)
    {
        auto callback = std::bind(
            &AudioStream::audio_encoder_callback, this, std::placeholders::_1);
        encoder = std::make_shared<AudioEncoder>(config.sample_rate,
                                                 config.channels,
                                                 config.sample_type,
                                                 callback,
                                                 id(),
                                                 logger);
    }

    return encoder;
}

}        // namespace qmedia
