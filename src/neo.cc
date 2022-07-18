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
               MediaDirection dir,
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
    media_dir = dir;

    log->info << "Transport Type " << (int) transport_type << std::flush;
    transport = std::make_unique<ClientTransportManager>(
        transport_type, remote_address, remote_port, metrics, log);

    transport->start();

    int64_t epoch_id = 1;
    transport->setCryptoKey(epoch_id, bytes(8, uint8_t(epoch_id)));

    workThread = std::thread(neoWorkThread, this);

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

    log->info << "MediaDirection:" << (int) media_dir << std::flush;

    if (media_dir == MediaDirection::publish_only ||
        media_dir == MediaDirection::publish_subscribe)
    {
        video_encoder = std::make_unique<H264Encoder>(
            video_max_width,
            video_max_height,
            video_max_frame_rate,
            video_max_bitrate,
            (uint32_t) video_encode_pixel_format,
            log);

        if (!video_encoder)
        {
            log->error << " video encoder init failed" << std::flush;
            return;
        }
    }

    log->info << "My ClientId: " << clientID << std::flush;
}

void Neo::publish(std::uint64_t source_id,
                  Packet::MediaType media_type,
                  std::string url)
{
    // Note: there is 1:1 mapping between source_id and object name
    auto pkt_transport = transport->transport();
    std::weak_ptr<NetTransportQUICR> tmp =
        std::static_pointer_cast<NetTransportQUICR>(pkt_transport.lock());
    auto quicr_transport = tmp.lock();

    url += "/" + std::to_string((int) media_type);
    quicr_transport->publish(source_id, media_type, url);
    log->info << "SourceID: " << source_id << ", Publish Url:" << url
              << std::flush;
}

void Neo::subscribe(uint64_t source_id,
                    Packet::MediaType media_type,
                    std::string url)
{
    auto pkt_transport = transport->transport();
    std::weak_ptr<NetTransportQUICR> tmp =
        std::static_pointer_cast<NetTransportQUICR>(pkt_transport.lock());
    auto quicr_transport = tmp.lock();

    url += "/" + std::to_string((int) media_type);
    quicr_transport->subscribe(source_id, media_type, url);

    log->info << " Subscribe Url:" << url << std::flush;
}

void Neo::start_transport(NetTransport::Type transport_type_in)
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
    if (media_dir == MediaDirection::publish_only ||
        media_dir == MediaDirection::publish_subscribe)
    {
        std::shared_ptr<AudioEncoder> audio_encoder = getAudioEncoder(sourceID);

        if (audio_encoder != nullptr)
        {
            log->debug << "sendAudio: SourceId:" << sourceID
                       << ", length:" << length << std::flush;
            audio_encoder->encodeFrame(
                buffer, length, timestamp, mutedAudioEmptyFrames);
        }
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
    if (video_encoder == nullptr)
    {
        log->debug << "Video Encoder, unavailable" << std::flush;
    }

    log->info << "Send Video Frame" << std::flush;
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

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count();

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

    auto now_2 = std::chrono::system_clock::now();
    auto now_ms_2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now_2.time_since_epoch())
                        .count();

    video_seq_no++;

    if (loopbackMode == LoopbackMode::codec)
    {
        transport->loopback(std::move(packet));
        return;
    }

    if (transport_type == NetTransport::Type::QUICR)
    {
        // quicr transport handles its own fragmentation and reassemble
        log->debug << "SendVideoFrame: Sending full object:"
                   << packet->data.size() << std::flush;
        packet->fragmentCount = 1;
        transport->send(std::move(packet));
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
    if (jitter == nullptr)
    {
        return 0;
    }

    packet = jitter->popAudio(sourceID, max_len);

    if (packet != nullptr)
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
    if (jitter_instance == nullptr)
    {
        return 0;
    }

    log->info << "Get Video Frame " << std::flush;
    Packet::IdrRequestData idr_data = {clientID, 0, 0};
    recv_length = jitter_instance->popVideo(
        sourceID, width, height, format, timestamp, buffer, idr_data);
    if (idr_data.source_timestamp > 0)
    {
        log->info << "jitter asked for keyFrame, sending IDR" << std::flush;
        PacketPointer idr = std::make_unique<Packet>();
        idr->packetType = Packet::Type::IdrRequest;
        idr->transportSequenceNumber = 0;
        idr->idrRequestData = std::move(idr_data);
        // transport->send(std::move(idr));
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
    log->debug << "Opus Encoded Audio Size:" << packet->data.size()
               << std::flush;
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
