#include <iostream>

#include "neo.hh"
#include "h264_encoder.hh"

namespace neo_media
{
Neo::Neo(const LoggerPointer &parent_logger) :
    log(std::make_shared<Logger>("NEO", parent_logger)),
    metrics(std::make_shared<Metrics>(MetricsConfig::URL,
                                      MetricsConfig::ORG,
                                      MetricsConfig::BUCKET,
                                      MetricsConfig::AUTH_TOKEN))
{
}

void Neo::init(const std::string &remote_address,
               const unsigned int remote_port,
               unsigned int audio_sample_rate_,
               unsigned int audio_channels_,
               audio_sample_type audio_type,
               unsigned int video_max_width_,
               unsigned int video_max_height_,
               unsigned int video_max_frame_rate_,
               unsigned int video_max_bitrate_,
               video_pixel_format video_encode_pixel_format_,
               video_pixel_format video_decode_pixel_format_,
               uint64_t clientID,
               uint64_t conferenceID,
               callbackSourceId callback,
               NetTransport::Type xport_type,
               bool echo)
{
    myClientID = clientID;
    myConferenceID = conferenceID;
    audio_sample_rate = audio_sample_rate_;
    audio_channels = audio_channels_;
    type = audio_type;
    video_max_width = video_max_width_;
    video_max_height = video_max_height_;
    video_max_frame_rate = video_max_frame_rate_;
    video_max_bitrate = video_max_bitrate_;
    video_encode_pixel_format = video_encode_pixel_format_;
    video_decode_pixel_format = video_decode_pixel_format_;
    newSources = callback;
    transport_type = xport_type;

    if (transport_type == NetTransport::Type::PICO_QUIC)
    {
        transport = std::make_unique<ClientTransportManager>(
            NetTransport::Type::PICO_QUIC,
            remote_address,
            remote_port,
            metrics,
            log);
        while (!transport->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    else if (transport_type == NetTransport::Type::QUICR)
    {
        transport = std::make_unique<ClientTransportManager>(
            NetTransport::Type::QUICR,
            remote_address,
            remote_port,
            metrics,
            log);

        // setup sources
    }
    else
    {
        // UDP
        transport = std::make_unique<ClientTransportManager>(
            NetTransport::Type::UDP, remote_address, remote_port, metrics, log);
        transport->start();
    }

    int64_t epoch_id = 1;
    transport->setCryptoKey(epoch_id, bytes(8, uint8_t(epoch_id)));

    // Construct and send a Join Packet
    if (transport_type != NetTransport::Type::QUICR)
    {
        PacketPointer joinPacket = std::make_unique<Packet>();
        assert(joinPacket);
        joinPacket->packetType = neo_media::Packet::Join;
        joinPacket->clientID = myClientID;
        joinPacket->conferenceID = myConferenceID;
        joinPacket->echo = echo;
        transport->send(std::move(joinPacket));
    }

    // TODO: add audio_encoder

    workThread = std::thread(neoWorkThread, this);

    // Init video pipeline unless disabled with zero width or height or bad
    // format

    if (!video_max_width || !video_max_height)
    {
        log->error << "video disabled" << std::flush;
        return;
    }

    if (video_encode_pixel_format != video_pixel_format::NV12 &&
        video_encode_pixel_format != video_pixel_format::I420)
    {
        std::cerr << "unsupported encode pixel format: "
                  << int(video_encode_pixel_format) << std::endl;
        return;
    }

    if (video_decode_pixel_format != video_pixel_format::NV12 &&
        video_decode_pixel_format != video_pixel_format::I420)
    {
        std::cerr << "unsupported decode pixel format: "
                  << int(video_decode_pixel_format) << std::endl;
        return;
    }

    video_encoder = std::make_unique<H264Encoder>(
        video_max_width,
        video_max_height,
        video_max_frame_rate,
        video_max_bitrate,
        (uint32_t) video_encode_pixel_format);

    if (!video_encoder)
    {
        log->error << "AV1 video encoder init failed" << std::flush;
        return;
    }
}

void Neo::publish(std::uint64_t source_id,
                  Packet::MediaType media_type,
                  std::string url)
{
    auto pkt_transport = transport->transport();
    std::weak_ptr<NetTransportQUICR> tmp =
        std::static_pointer_cast<NetTransportQUICR>(pkt_transport.lock());
    auto quicr_transport = tmp.lock();
    quicr_transport->publish(source_id, media_type, url);
}

void Neo::subscribe(Packet::MediaType mediaType, std::string url)
{
    auto pkt_transport = transport->transport();
    std::weak_ptr<NetTransportQUICR> tmp =
        std::static_pointer_cast<NetTransportQUICR>(pkt_transport.lock());
    auto quicr_transport = tmp.lock();
    quicr_transport->subscribe(mediaType, url);
}

void Neo::start_transport(NetTransport::Type transport_type)
{
    if (transport_type == NetTransport::Type::QUICR)
    {
        auto pkt_transport = transport->transport();
        std::weak_ptr<NetTransportQUICR> tmp =
            std::static_pointer_cast<NetTransportQUICR>(pkt_transport.lock());
        auto quicr_transport = tmp.lock();
        quicr_transport->start();

        while (!transport->transport_ready())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

/** Receive all packets in this thread, push to jitter buffer, and notify client
 * if new stream via callback. */
void Neo::doWork()
{
    // Wait on a condition variable
    while (!shutdown)
    {
        transport->waitForPacket();
        PacketPointer packet = transport->recv();
        if (packet == nullptr)
        {
            continue;
        }

        if (Packet::Type::StreamContent == packet->packetType)
        {
            uint64_t clientID = packet->clientID;
            uint64_t sourceID = packet->sourceID;
            uint64_t sourceTS = packet->sourceRecordTime;
            // TODO: This is pre-decode media type, which is
            // not really right but close enough. Hold off changing a lot
            // to get at that data before Geir's changes to Jitter/Playout.
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
                if (new_stream && newSources)
                {
                    newSources(clientID, sourceID, sourceTS, sourceType);
                }
            }
        }
        else if (Packet::Type::IdrRequest == packet->packetType)
        {
            log->info << "Received IDR request" << std::flush;
            reqKeyFrame = true;
        }
    }
}

void Neo::setMicrophoneMute(bool muted)
{
    mutedAudioEmptyFrames = muted;
}

void Neo::sendAudio(const char *buffer,
                    unsigned int length,
                    uint64_t timestamp,
                    uint64_t sourceID)
{
    std::shared_ptr<AudioEncoder> audio_encoder = getAudioEncoder(sourceID);

    if (audio_encoder != nullptr)
    {
        audio_encoder->encodeFrame(
            buffer, length, timestamp, mutedAudioEmptyFrames);
    }
}

void Neo::sendVideoFrame(const char *buffer,
                         uint32_t length,
                         uint32_t width,
                         uint32_t height,
                         uint32_t stride_y,
                         uint32_t stride_uv,
                         uint32_t offset_u,
                         uint32_t offset_v,
                         uint32_t format,
                         uint64_t timestamp,
                         uint64_t sourceID)
{
    // TODO:implement clone()
    // TODO: remove assert
    int sendRaw = 0;        // 1 will send Raw YUV video instead of AV1
    PacketPointer packet = std::make_unique<Packet>();
    assert(packet);
    packet->packetType = neo_media::Packet::Type::StreamContent;
    packet->clientID = myClientID;
    packet->conferenceID = myConferenceID;
    packet->sourceID = sourceID;
    packet->encodedSequenceNum = video_seq_no;
    packet->sourceRecordTime = timestamp;
    packet->mediaType = sendRaw ? Packet::MediaType::Raw :
                                  Packet::MediaType::AV1;
    // encode and packetize
    encodeVideoFrame(buffer,
                     length,
                     width,
                     height,
                     stride_y,
                     stride_uv,
                     offset_u,
                     offset_v,
                     format,
                     timestamp,
                     packet.get());
    if (packet->data.empty())
    {
        // encoder skipped this frame, nothing to send
        return;
    }
    video_seq_no++;

    if (loopbackMode == LoopbackMode::codec)
    {
        transport->loopback(std::move(packet));
        return;
    }

    // create object for managing packetization
    auto packets = std::make_shared<SimplePacketize>(std::move(packet),
                                                     1200 /* MTU */);
    for (unsigned int i = 0; i < packets->GetPacketCount(); i++)
    {
        auto frame_packet = packets->GetPacket(i);
        frame_packet->transportSequenceNumber = 0;        // this will be set by
                                                          // the transport layer
        // sending lots of Raw packets at the same time is unreliable.
        if (frame_packet->mediaType == Packet::MediaType::Raw)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }

        if (loopbackMode == LoopbackMode::full_media)
        {
            // don't transmit but return an recv from Net
            transport->loopback(std::move(frame_packet));
        }
        else
        {
            // send over network
            transport->send(std::move(frame_packet));
        }
    }
}

JitterInterface::JitterIntPtr Neo::getJitter(uint64_t clientID)
{
    auto playout_key = clientID;
    if (auto it{jitters.find(playout_key)}; it != std::end(jitters))
    {
        return it->second;
    }
    return nullptr;
}

JitterInterface::JitterIntPtr Neo::createJitter(uint64_t clientID)
{
    Packet::MediaType packet_media_type = Packet::MediaType::Bad;
    switch (type)
    {
        case audio_sample_type::Float32:
            packet_media_type = Packet::MediaType::F32;
            break;
        case audio_sample_type::PCMint16:
            packet_media_type = Packet::MediaType::L16;
            break;
        default:
            assert(0);
    }

    if (jitters.size() < maxJitters)
    {
        if (loopbackMode == LoopbackMode::codec)
        {
            LoopbackJitter::LoopbackJitterPtr loopbackPtr =
                std::make_shared<LoopbackJitter>(
                    audio_sample_rate,
                    audio_channels,
                    packet_media_type,
                    video_max_width,
                    video_max_height,
                    (uint32_t) video_decode_pixel_format,
                    log);
            auto ret = jitters.emplace(
                clientID,
                JitterInterface::JitterIntPtr(
                    std::static_pointer_cast<JitterInterface>(loopbackPtr)));
            return ret.first->second;
        }
        else
        {
            Jitter::JitterPtr jitterPtr = std::make_shared<Jitter>(
                audio_sample_rate,
                audio_channels,
                packet_media_type,
                video_max_width,
                video_max_height,
                (uint32_t) video_decode_pixel_format,
                log,
                metrics);
            auto ret = jitters.emplace(
                clientID,
                JitterInterface::JitterIntPtr(
                    std::static_pointer_cast<JitterInterface>(jitterPtr)));
            return ret.first->second;
        }
    }
    return nullptr;
}

void Neo::encodeVideoFrame(const char *buffer,
                           uint32_t length,
                           uint32_t width,
                           uint32_t height,
                           uint32_t stride_y,
                           uint32_t stride_uv,
                           uint32_t offset_u,
                           uint32_t offset_v,
                           uint32_t format,
                           uint64_t timestamp,
                           Packet *packet)
{
    switch (packet->mediaType)
    {
        case Packet::MediaType::AV1:
        {
            // TODO : add logic to determine to send keyframe or not
            // TODO : encoder api needs to return frame type and its
            // relationships
            bool keyFrame = reqKeyFrame;
            packet->videoFrameType = (Packet::VideoFrameType)
                                         video_encoder->encode(buffer,
                                                               length,
                                                               width,
                                                               height,
                                                               stride_y,
                                                               stride_uv,
                                                               offset_u,
                                                               offset_v,
                                                               format,
                                                               timestamp,
                                                               packet->data,
                                                               keyFrame);
            if (keyFrame &&
                packet->videoFrameType == Packet::VideoFrameType::Idr)
            {
                log->info << "after encode, resetting keyFrame\n" << std::flush;
                reqKeyFrame = false;
            }
        }
        break;
        case Packet::MediaType::Raw:
            packet->data.resize(length);
            memcpy(packet->data.data(), buffer, length);
            break;
        default:
            log->error << "unknown video packet type: "
                       << (int) packet->mediaType << std::flush;
            assert(0);        // TODO: handle more gracefully
    }
}

int Neo::getAudio(uint64_t clientID,
                  uint64_t sourceID,
                  uint64_t &timestamp,
                  unsigned char **buffer,
                  unsigned int max_len,
                  Packet **packetToFree)
{
    int recv_length = 0;

    PacketPointer packet;
    JitterInterface::JitterIntPtr jitter = getJitter(clientID);
    if (jitter == nullptr) return 0;

    packet = jitter->popAudio(sourceID, max_len);

    if (packet != NULL)
    {
        timestamp = packet->sourceRecordTime;
        *buffer = &packet->data[0];
        recv_length = packet->data.size();
        *packetToFree = packet.release();
    }
    return recv_length;
}

std::uint32_t Neo::getVideoFrame(uint64_t clientID,
                                 uint64_t sourceID,
                                 uint64_t &timestamp,
                                 uint32_t &width,
                                 uint32_t &height,
                                 uint32_t &format,
                                 unsigned char **buffer)
{
    int recv_length = 0;

    JitterInterface::JitterIntPtr jitter_instance = getJitter(clientID);
    if (jitter_instance == nullptr) return 0;

    Packet::IdrRequestData idr_data = {clientID, 0, 0};
    recv_length = jitter_instance->popVideo(
        sourceID, width, height, format, timestamp, buffer, idr_data);
    if (idr_data.source_timestamp > 0)
    {
        log->debug << "jitter asked for keyFrame, sending IDR\n" << std::flush;
        PacketPointer idr = std::make_unique<Packet>();
        idr->packetType = Packet::Type::IdrRequest;
        idr->transportSequenceNumber = 0;
        idr->idrRequestData = std::move(idr_data);
        transport->send(std::move(idr));
    }
    return recv_length;
}

void Neo::audioEncoderCallback(PacketPointer packet)
{
    assert(packet);

    // TODO: temporary copy until we decouple data from packet
    packet->packetType = neo_media::Packet::Type::StreamContent;
    packet->clientID = myClientID;
    packet->conferenceID = myConferenceID;
    packet->encodedSequenceNum = seq_no;
    packet->transportSequenceNumber = 0;        // this will be set by the
                                                // transport layer

    // This assumes that the callback is coming from an opus encoder
    // Once we encode other media we need to change where this is set
    packet->mediaType = Packet::MediaType::Opus;

    // this seems to be incomplete, we need media timeline driven
    // seq_no and may be also the media packet sequence number
    seq_no++;
    if (loopbackMode == LoopbackMode::full_media ||
        loopbackMode == LoopbackMode::codec)
    {
        // don't transmit but return an recv from Net
        transport->loopback(std::move(packet));
        return;
    }

    // send it over the network

    transport->send(std::move(packet));
}

std::shared_ptr<AudioEncoder> Neo::getAudioEncoder(uint64_t sourceID)
{
    std::shared_ptr<AudioEncoder> audio_enc = nullptr;
    if (auto it{audio_encoders.find(sourceID)}; it != std::end(audio_encoders))
    {
        audio_enc = it->second;
    }

    if (audio_enc == nullptr)
    {
        Packet::MediaType m_type;

        switch (type)
        {
            case Neo::audio_sample_type::PCMint16:
                m_type = Packet::MediaType::L16;
                break;
            case Neo::audio_sample_type::Float32:
                m_type = Packet::MediaType::F32;
                break;
            default:
                break;
        }
        auto callback = std::bind(
            &Neo::audioEncoderCallback, this, std::placeholders::_1);
        auto ret = audio_encoders.emplace(
            sourceID,
            std::make_shared<AudioEncoder>(audio_sample_rate,
                                           audio_channels,
                                           m_type,
                                           callback,
                                           sourceID,
                                           log));
        audio_enc = ret.first->second;
    }

    return audio_enc;
}

}        // namespace neo_media
