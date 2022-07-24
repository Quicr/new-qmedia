#include "media_stream.hh"
#include "packet.hh"

namespace qmedia
{

///
/// MediaStream
///

void MediaStream::handle_media(MediaClient::NewSourceCallback  stream_callback,
                               uint64_t /*group_id*/,
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

    uint64_t client_id = packet->clientID;
    uint64_t source_id= packet->sourceID;
    uint64_t source_ts = packet->sourceRecordTime;
    MediaType media_type = MediaType::invalid;
    switch(packet->mediaType) {
        case Packet::MediaType::H264:
            media_type = MediaType::video;
            break;
        case Packet::MediaType::Opus:
        case Packet::MediaType::F32:
        case Packet::MediaType::L16:
            media_type = MediaType::audio;
            break;
        default:
            media_type = MediaType::invalid;
    }

    logger->debug <<"[MediaStream::handle_media]: id:"
                 << packet->sourceID << ", media_type:" << media_type
                 << ", seqNo:" << packet->encodedSequenceNum
                 << ", is_intra:" << packet->is_intra_frame
                 << ", size:" << packet->data.size()
                 << ", timestamp:" << packet->sourceRecordTime
                 << std::flush;

    bool new_stream = false;
    auto jitter_instance = getJitter(client_id);
    if (jitter_instance == nullptr)
    {
        jitter_instance = createJitter(client_id, config);
    }

    if (jitter_instance != nullptr)
    {
        new_stream = jitter_instance->push(std::move(packet));
        // jitter assembles packets to frames, decodes, conceals
        // and makes frames available to client
        if (new_stream && stream_callback)
        {
            stream_callback(client_id, source_id, source_ts, media_type);
        }
    }
}

///
/// Audio Stream Api
///
AudioStream::AudioStream(uint64_t domain,
                         uint64_t conference_id,
                         uint64_t client_id,
                         const MediaConfig &media_config,
                         LoggerPointer logger_in) :
    MediaStream(domain, conference_id, client_id, media_config, logger_in)
{
}

void AudioStream::configure()
{
    media_direction = config.media_direction;
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
                encoder->encodeFrame(
                    buffer, length, timestamp, mutedAudioEmptyFrames);
            }
        }
        break;
        case MediaConfig::CodecType::opus:
            break;
    }
}

size_t AudioStream::get_media(uint64_t &timestamp,
                              MediaConfig &/*config*/,
                              unsigned char **buffer,
                              unsigned int max_len,
                              void** to_free)
{
    int recv_length = 0;
    auto jitter = getJitter(client_id);
    if (jitter == nullptr) {
        logger->error << "[AudioStream::get_media] Jitter not found" << std::flush;
        return 0;
    }

    auto packet = jitter->popAudio(id(), max_len);

    if (packet != nullptr)
    {
        timestamp = packet->sourceRecordTime;
        *buffer = &packet->data[0];
        recv_length = packet->data.size();
        logger->debug << "[AudioStream::get_media] recv_length:" << recv_length << std::flush;
        *to_free = packet.release();
    }

    return recv_length;
}

///
/// Private
///

void AudioStream::audio_encoder_callback(std::vector<uint8_t> &&bytes, uint64_t timestamp)
{
    if (media_direction == MediaConfig::MediaDirection::recvonly)
    {
        // no-op
        return;
    }

    auto packet = std::make_unique<Packet>();
    packet->data = std::move(bytes);
    packet->clientID = client_id;
    packet->sourceRecordTime = timestamp;
    packet->mediaType = Packet::MediaType::Opus;
    packet->sourceID = id();        // same as streamId
    packet->encodedSequenceNum = encode_sequence_num;
    encode_sequence_num += 1;
    auto ret = Packet::encode(packet.get(), packet->encoded_data);
    if (!ret)
    {
        // log err
        return;
    }

    logger->debug << "MediaStream: " << id()
                 << " sending audio packet: " << packet->encodedSequenceNum
                 << ", timestamp " << packet->sourceRecordTime
                 << std::flush;

    media_transport->send_data(id(), std::move(packet->encoded_data));
}

std::shared_ptr<AudioEncoder> AudioStream::setupAudioEncoder()
{
    if (encoder == nullptr)
    {
        auto callback = std::bind(
            &AudioStream::audio_encoder_callback, this, std::placeholders::_1, std::placeholders::_2);
        encoder = std::make_shared<AudioEncoder>(config.sample_rate,
                                                 config.channels,
                                                 config.sample_type,
                                                 callback,
                                                 id(),
                                                 logger);
    }

    return encoder;
}

std::shared_ptr<Jitter> MediaStream::getJitter(uint64_t client_id)
{
    if (auto it{jitters.find(client_id)}; it != std::end(jitters))
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Jitter> MediaStream::createJitter(uint64_t clientID,
                                                  const MediaConfig &config)
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
