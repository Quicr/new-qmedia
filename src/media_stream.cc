#include "media_stream.hh"
#include "packet.hh"

namespace qmedia
{

///
/// Audio Stream Api
///
AudioStream::AudioStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id) :
    MediaStream(domain, conference_id, client_id)
{
}

MediaStreamId AudioStream::id()
{
    if (media_stream_id)
    {
        return media_stream_id;
    }

    auto name = QuicrName::name_for_client(domain, conference_id, client_id);
    auto quality_id = "audio/" + std::to_string((int) config.media_codec) +
                      "/" + std::to_string(config.sample_rate) + "/" +
                      std::to_string(config.channels);
    name += quality_id;
    std::hash<std::string> hasher;
    media_stream_id = hasher(name);
    return media_stream_id;
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
            setupAudioEncoder();
            break;
        case MediaConfig::MediaDirection::sendrecv:
            // setup encoder and decoder
            setupAudioEncoder();
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
                               const MediaConfig & /*media_config*/)
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
            auto encoder = setupAudioEncoder();
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
            break;
    }
}

void AudioStream::handle_media(uint64_t /*group_id*/,
                               uint64_t /*object_id*/,
                               std::vector<uint8_t> &&data)
{
    if (data.empty())
    {
        // log here
        return;
    }

    // decode
    auto packet = std::make_unique<Packet>();
    auto ret = Packet::decode(data, packet.get());
    if (!ret)
    {
        // log here
        return;
    }

    uint64_t clientID = packet->clientID;
    uint64_t sourceID = packet->sourceID;
    uint64_t sourceTS = packet->sourceRecordTime;
    Packet::MediaType sourceType = packet->mediaType;

    bool new_stream;
    auto jitter_instance = getJitter(clientID);
    if (jitter_instance == nullptr)
    {
        jitter_instance = createJitter(clientID);
    }

    if (jitter_instance != nullptr)
    {
        new_stream = jitter_instance->push(std::move(packet));
        // jitter assembles packets to frames, decodes, conceals
        // and makes frames available to client
        // if (new_stream && newSources)
        // {
        //    newSources(clientID, sourceID, sourceTS, sourceType);
        // }
    }
}
///
/// Private
///

void AudioStream::audio_encoder_callback(std::vector<uint8_t> &&bytes)
{
    logger->debug << id() << "Opus Encoded Audio Size:" << bytes.size()
                  << std::flush;

    if (media_direction == MediaConfig::MediaDirection::recvonly)
    {
        // no-op
        return;
    }

    auto packet = std::make_unique<Packet>();
    packet->data = std::move(bytes);
    packet->clientID = client_id;
    // todo : fix this to not be hardcoded
    packet->mediaType = Packet::MediaType::Opus;
    packet->sourceID = id();        // same as streamId
    auto ret = Packet::encode(packet.get(), packet->encoded_data);
    if (!ret)
    {
        // log err
        return;
    }

    logger->debug << "MediaStream : " << id() << " sending audio packet"
                  << std::flush;
    media_transport->send_data(id(), std::move(packet->encoded_data));
}

std::shared_ptr<AudioEncoder> AudioStream::setupAudioEncoder()
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

std::shared_ptr<Jitter> AudioStream::getJitter(uint64_t client_id)
{
    if (auto it{jitters.find(client_id)}; it != std::end(jitters))
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Jitter> AudioStream::createJitter(uint64_t clientID)
{
    Packet::MediaType packet_media_type = Packet::MediaType::Bad;
    switch (config.sample_type)
    {
        case AudioConfig::SampleType::Float32:
            packet_media_type = Packet::MediaType::F32;
            break;
        case AudioConfig::SampleType::PCMint16:
            packet_media_type = Packet::MediaType::L16;
            break;
        default:
            assert(0);
    }

    if (jitters.size() < maxJitters)
    {
        // todo: add metrics
        auto jitter = std::make_shared<Jitter>(logger);
        jitter->set_audio_params(
            config.sample_rate, config.channels, packet_media_type);
        auto ret = jitters.emplace(clientID, jitter);
        return ret.first->second;
    }

    return nullptr;
}

}        // namespace qmedia
