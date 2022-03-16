#include <cstring>        // memeset
#include <cassert>
#include <iostream>

#include "jitter.hh"
#include "h264_decoder.hh"

using namespace neo_media;

Jitter::Jitter(unsigned int audio_sample_rate,
               unsigned int audio_channels,
               Packet::MediaType audio_decode_type,
               uint32_t video_max_width,
               uint32_t video_max_height,
               uint32_t video_decode_pixel_format,
               const LoggerPointer &parent_logger,
               Metrics::MetricsPtr metricsPtr) :
    audio(audio_sample_rate, audio_channels),
    decode_audio_as(audio_decode_type),
    video(video_max_width, video_max_height, video_decode_pixel_format),
    idle_client(true),
    metrics(metricsPtr)
{
    logger = std::make_shared<Logger>("Jitter", parent_logger);
    video.decoder = std::make_unique<H264Decoder>(video_decode_pixel_format);
    assert(video.decoder);
}

Jitter::~Jitter()
{
    shutdown = true;
    audio.mq.flushPackets();
    video.mq.flushPackets();
}

void Jitter::recordMetrics(MetaQueue &q,
                           MetaQueue::media_type type,
                           uint64_t clientID,
                           uint64_t sourceID)
{
    if (metrics != nullptr)
    {
        if (measurement == nullptr)
            measurement = metrics->createMeasurement(
                "Jitter", {{"clientID", clientID}, {"sourceID", sourceID}});

        if (measurement != nullptr)
        {
            measurement->set(std::chrono::system_clock::now(),
                             q.getMetricFields(type));
        }
    }
}

bool Jitter::push(PacketPointer raw_packet)
{
    return push(std::move(raw_packet), std::chrono::steady_clock::now());
}

bool Jitter::push(PacketPointer packet,
                  std::chrono::steady_clock::time_point now)
{
    bool new_stream = false;
    uint64_t sourceID = packet->sourceID;
    uint64_t clientID = packet->clientID;

    {
        assert(packet);
        switch (packet->mediaType)
        {
            case Packet::MediaType::L16:
            case Packet::MediaType::F32:
                audio.push(std::move(packet), sync.audio_seq_popped, now);
                audio.insertAudioPLCs();
                audio_jitter.updateJitterValues(audio.mq,
                                                audio.getMsPerAudioPacket());
                recordMetrics(
                    audio.mq, MetaQueue::media_type::audio, clientID, sourceID);
                idleClientPruneAudio(now);
                break;
            case Packet::MediaType::AV1:
            case Packet::MediaType::Raw:
            {
                if (video.assembler == nullptr)
                {
                    video.assembler = std::make_shared<SimpleVideoAssembler>();
                    new_stream = true;
                    logger->info << "New video sourceID: " << sourceID
                                 << std::flush;
                    video.sourceID = packet->sourceID;
                }

                PacketPointer raw = video.assembler->push(std::move(packet));
                if (raw != nullptr)
                {
                    // we got assembled frame, add it to jitter queue
                    video.push(std::move(raw), sync.video_seq_popped, now);
                    recordMetrics(video.mq,
                                  MetaQueue::media_type::video,
                                  clientID,
                                  sourceID);
                }
            }
            break;
            case Packet::MediaType::Opus:
            {
                if (audio.opus_assembler == nullptr)
                {
                    audio.opus_assembler = std::make_shared<OpusAssembler>(
                        packet->mediaType,
                        decode_audio_as,
                        audio.audio_channels,
                        audio.audio_sample_rate);
                    new_stream = true;
                    logger->info << "New audio sourceID: " << sourceID
                                 << std::flush;
                }

                PacketPointer raw = audio.opus_assembler->push(
                    std::move(packet));
                if (raw != nullptr)
                {
                    // we got a decoded audio frame
                    audio.push(std::move(raw), sync.audio_seq_popped, now);
                    audio.insertAudioPLCs();
                    audio_jitter.updateJitterValues(
                        audio.mq, audio.getMsPerAudioPacket());
                    recordMetrics(audio.mq,
                                  MetaQueue::media_type::audio,
                                  clientID,
                                  sourceID);
                    idleClientPruneAudio(now);
                }
            }
            break;
            default:
                break;
        }
    }

    return new_stream;
}

PacketPointer Jitter::popAudio(uint64_t sourceID, unsigned int length)
{
    return popAudio(sourceID, length, std::chrono::steady_clock::now());
}

PacketPointer Jitter::popAudio(uint64_t sourceID,
                               unsigned int length,
                               std::chrono::steady_clock::time_point now)
{
    PacketPointer packet = nullptr;
    bool unableToThrowSilence = false;

    if (idle_client) idle_client = false;

    if (sourceID != audio.sourceID)
    {
        logger->warning << "SrcID?" << sourceID
                        << " Audio source id:" << audio.sourceID << std::flush;
        return packet;
    }

    // std::clog << "Jitter QD:" << audio.getMsInQueue() << std::endl;

    QueueMonitor(now);
    int num_depth_adjustments = 1;

    // loop to ensure clients asking for variable length data
    while (audio.playout.getTotalInBuffers() < length)
    {
        if (bucket.initialFill(audio.getMsInQueue(),
                               audio_jitter.getJitterMs()))
        {
            // we don't have anything in our buffers, create PLC
            packet = audio.createPLC(audio.getFrameSize());
            packet->sourceRecordTime = 0;
            logger->debug << "F" << std::flush;
        }
        else
        {
            double src_ratio = bucket.getSrcRatio();
            if (num_depth_adjustments > 0 && src_ratio > 1.0)
            {
                // insert silence if we need to make
                if (audio.silence.isSilence())
                {
                    // only extend existing silence (avoid pushing into
                    // talk spurt)
                    packet = audio.createPLC(audio.getFrameSize());
                    packet->sourceRecordTime = 0;
                    logger->debug << "F" << std::flush;
                    --num_depth_adjustments;
                    bucket.adjustQueueDepthTrackerDiscardedPackets(1);
                }
                else
                {
                    // TODO: undersample over a longer sliding window
                    // to avoid distorting effects
                }
            }

            if (packet == nullptr)
            {
                std::lock_guard<std::mutex> lock(audio.mq.qMutex);
                packet = audio.pop(now);
                if (!packet)
                {
                    // no audio in our buffers, update trackers
                    bucket.emptyBucket(now);
                    packet = audio.createPLC(audio.getFrameSize());
                    packet->sourceRecordTime = 0;
                    logger->debug << "F" << std::flush;
                }
                else
                {
                    // we have either silence/audio data
                    // need to catch up throw silence packet
                    if (num_depth_adjustments > 0 && src_ratio < 1.0 &&
                        audio.silence.isSilence() &&
                        audio.mq.totalPacketBytes() > length)
                    {
                        packet.reset();
                        packet = nullptr;
                        --num_depth_adjustments;
                        bucket.adjustQueueDepthTrackerDiscardedPackets(-1);
                    }

                    if (packet != nullptr)
                    {
                        // got audio data, update sync data for video
                        sync.audio_popped(packet->sourceRecordTime,
                                          packet->encodedSequenceNum,
                                          now);
                        if (packet->encodedSequenceNum % 100 == 0)
                            logger->debug << "[A:" << packet->encodedSequenceNum
                                          << "]" << std::flush;
                    }
                }
            }
        }

        // got silence/audio
        if (packet != nullptr)
        {
            switch (packet->mediaType)
            {
                case Packet::MediaType::L16:
                    audio.playout.sample_divisor = audio.audio_channels *
                                                   sizeof(uint16_t);
                    break;
                case Packet::MediaType::F32:
                    audio.playout.sample_divisor = audio.audio_channels *
                                                   sizeof(float);
                    break;
                default:
                    break;
            }
            audio.playout.addBuffer(packet->data.data(),
                                    packet->data.size(),
                                    packet->sourceRecordTime);
            packet.reset();
        }
    }        // while

    packet = std::make_unique<Packet>();
    packet->data.resize(0);
    packet->data.reserve(length);
    uint64_t timestamp;
    audio.playout.fill(packet->data, length, timestamp);
    packet->encodedSequenceNum = sync.audio_seq_popped;
    packet->sourceRecordTime = timestamp;
    logger->debug << "[QA: " << audio.mq.size() << "]" << std::flush;
    return packet;
}

int Jitter::popVideo(uint64_t sourceID,
                     uint32_t &width,
                     uint32_t &height,
                     uint32_t &format,
                     uint64_t &timestamp,
                     unsigned char **buffer,
                     Packet::IdrRequestData &idr_data_out)
{
    return popVideo(sourceID,
                    width,
                    height,
                    format,
                    timestamp,
                    buffer,
                    std::chrono::steady_clock::now(),
                    idr_data_out);
}

int Jitter::setDecodedFrame(uint64_t /*sourceID*/,
                            uint32_t &width,
                            uint32_t &height,
                            uint32_t &format,
                            uint64_t &timestamp,
                            unsigned char **buffer)
{
    *buffer = video.lastDecodedFrame.data();
    width = video.last_decoded_width;
    height = video.last_decoded_height;
    format = video.last_decoded_format;
    timestamp = video.last_decoded_timestamp;
    return video.lastDecodedFrame.size();
}

void Jitter::decodeVideoPacket(PacketPointer packet,
                               uint64_t sourceID,
                               uint32_t &width,
                               uint32_t &height,
                               uint32_t &format,
                               uint64_t &timestamp,
                               unsigned char **buffer,
                               std::chrono::steady_clock::time_point now)
{
    int error;
    int len;

    switch (packet->mediaType)
    {
        case Packet::MediaType::AV1:
            error = video.decoder->decode((const char *) &packet->data[0],
                                          packet->data.size(),
                                          width,
                                          height,
                                          format,
                                          video.lastDecodedFrame);
            if (error)
            {
                std::cerr << "[dav1d error " << error << "]" << std::endl;
                // reuse the last decoded frame / parameters
            }
            else
            {
                sync.video_popped(
                    packet->sourceRecordTime, packet->encodedSequenceNum, now);
                // update the last decoded frame / parameters
                video.last_decoded_format = format;
                video.last_decoded_height = height;
                video.last_decoded_width = width;
                video.last_decoded_timestamp = packet->sourceRecordTime;
            }
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            packet.reset();
            break;
        case Packet::MediaType::Raw:
            sync.video_popped(
                packet->sourceRecordTime, packet->encodedSequenceNum, now);
            video.lastDecodedFrame.swap(packet->data);
            packet.reset();
            break;
        default:
            logger->error << "unknown video packet type: "
                          << (int) packet->mediaType << std::flush;
            assert(0);        // TODO: handle more gracefully
            break;
    }
}

// Client is requesting for a video frame to render
int Jitter::popVideo(uint64_t sourceID,
                     uint32_t &width,
                     uint32_t &height,
                     uint32_t &format,
                     uint64_t &timestamp,
                     unsigned char **buffer,
                     std::chrono::steady_clock::time_point now,
                     Packet::IdrRequestData &idr_data_out)
{
    int len = 0;
    PacketPointer packet = nullptr;
    unsigned int num_pop = 0;
    uint64_t sourceRecordTime = 0;

    if (sourceID != video.sourceID)
    {
        logger->warning << "SrcID?" << sourceID
                        << " VideoQSrcID:" << video.sourceID << std::flush;
        return len;
    }

    if (idle_client) idle_client = false;

    if (video.mq.empty())
    {
        // return the last decoded frame
        len = setDecodedFrame(
            sourceID, width, height, format, timestamp, buffer);
        logger->debug << "[EQ]" << std::flush;
        return len;
    }

    switch (sync.getVideoAction(audio.fps.getAveragePopDelay(),
                                video.fps.getAveragePopDelay(),
                                video.mq,
                                num_pop,
                                now))

    {
        case Sync::sync_action::hold:
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            logger->debug << "[H]" << std::flush;
            break;
        case Sync::sync_action::pop:
            for (unsigned int pops = 0; pops < num_pop; pops++)
            {
                packet = video.pop(now);
                if (packet != nullptr)
                {
                    logger->debug << "[V:" << packet->encodedSequenceNum << "]"
                                  << std::flush;
                    decodeVideoPacket(std::move(packet),
                                      sourceID,
                                      width,
                                      height,
                                      format,
                                      timestamp,
                                      buffer,
                                      now);
                }
            }
            // set the final decoded frame as output frame
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            break;
        case Sync::sync_action::pop_discard:
            for (unsigned int pops = 0; pops < num_pop; pops++)
            {
                packet = video.pop(now);
                sourceRecordTime = packet->sourceRecordTime;
                packet.reset(nullptr);
            }
            idr_data_out.source_id = sourceID;
            idr_data_out.source_timestamp = sourceRecordTime;
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            logger->debug << "[D]" << std::flush;
            logger->info << "jitter: popDiscard: keyFrame requested"
                         << std::flush;
            break;
        case Sync::sync_action::pop_video_only:
            for (unsigned int pops = 0; pops < (video.mq.size() - 2);
                 pops++)        // simple algorithm.  we keep two frames in
                                // buffer for video only (66ms)
            {
                packet = video.pop(now);
                if (packet != nullptr)
                {
                    // if (packet->encodedSequenceNum % 30 == 0)
                    logger->debug << "[V only:" << packet->encodedSequenceNum
                                  << "]" << std::flush;
                    decodeVideoPacket(std::move(packet),
                                      sourceID,
                                      width,
                                      height,
                                      format,
                                      timestamp,
                                      buffer,
                                      now);
                }
            }
            // set the final decoded frame as output frame
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            break;
    }

    return len;
}

void Jitter::idleClientPruneAudio(std::chrono::steady_clock::time_point now)
{
    // client not popping yet - prune if too much
    if (idle_client)
    {
        if (!audio.mq.empty())
        {
            unsigned int prune_target = bucket.getRecommendedFillLevel(
                audio_jitter.getJitterMs());
            audio.pruneAudioQueue(now, prune_target);
        }
        return;
    }
}

void Jitter::QueueMonitor(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock(audio.mq.qMutex);
    unsigned int plcs = 0;
    unsigned int lost_in_queue = audio.mq.lostInQueue(plcs,
                                                      sync.audio_seq_popped);
    // total number of audio frames in queue
    unsigned int queue_size = audio.getMsInQueue();
    // unsigned int queue_size = audio.mq.size();
    // average jitter since the last pop
    unsigned int jitter_ms = audio_jitter.getJitterMs();
    unsigned int ms_per_audio = audio.getMsPerAudioPacket();
    unsigned int client_fps = audio.fps.getFps();
    bucket.tick(
        now, queue_size, lost_in_queue, jitter_ms, ms_per_audio, client_fps);
}

///
/// Jitter::Audio
///

Jitter::Audio::Audio(unsigned int sample_rate, unsigned int channels) :
    sourceID(0),
    audio_sample_rate(sample_rate),
    audio_channels(channels),
    ms_per_audio_packet(0),
    opus_assembler(nullptr)
{
}

PacketPointer Jitter::Audio::createPLC(unsigned int size)
{
    PacketPointer packet = nullptr;

    if (opus_assembler != nullptr)
    {
        packet = opus_assembler->opusCreatePLC(size);
    }
    else
    {
        packet = createZeroPayload(size);
    }
    return packet;
}

PacketPointer Jitter::Audio::createZeroPayload(unsigned int size)
{
    PacketPointer packet = std::make_unique<Packet>();
    packet->data.resize(size);
    std::fill(packet->data.begin(), packet->data.end(), 0);
    return packet;
}

void Jitter::Audio::insertAudioPLCs()
{
    // audioQ push ensures sorted queue
    uint64_t prev_seq = 0;
    bool plc_first_frame = true;
    std::lock_guard<std::mutex> lock(mq.qMutex);

    std::list<std::shared_ptr<MetaFrame>>::iterator it;
    for (it = mq.Q.begin(); it != mq.Q.end(); ++it)
    {
        uint64_t curr_seq = (*it)->packet->encodedSequenceNum;

        if (plc_first_frame)
        {
            // skip first
            prev_seq = curr_seq;
            plc_first_frame = false;
            continue;
        }

        uint64_t seq_diff = curr_seq - prev_seq;
        if (seq_diff > 1)
        {
            uint64_t missing_seq = prev_seq + 1;
            uint64_t plc_count = seq_diff - 1;
            // add plcs for all the missing audio frames
            for (size_t i = 0; i < plc_count; i++)
            {
                // TODO: add check for before and after.  opus plc uses before
                // from inside codec
                PacketPointer plc_packet = createPLC(getFrameSize());
                plc_packet->encodedSequenceNum = missing_seq;
                auto frame = std::make_shared<MetaFrame>(
                    MetaFrame::Type::plc_gen, std::chrono::steady_clock::now());
                frame->packet = std::move(plc_packet);
                mq.Q.insert(it, frame);
                ++missing_seq;
            }
        }

        // update to the latest seq number
        prev_seq = curr_seq;
    }
}
void Jitter::Audio::push(PacketPointer raw_packet,
                         uint64_t last_seq_popped,
                         std::chrono::steady_clock::time_point now)
{
    switch (raw_packet->mediaType)
    {
        case Packet::MediaType::F32:
            if (!silence.isInited())
            {
                silence.init((int) (raw_packet->data.size() /
                                    (sizeof(float) * audio_channels)),
                             (int) audio_sample_rate,
                             (int) audio_channels);
            }
            silence.updateSilenceDetection(
                (const float *) raw_packet->data.data());
            // fall thru to include L16 not supported by silence adjustment
        case Packet::MediaType::L16:
            if (mq.empty())
            {
                sourceID = raw_packet->sourceID;
            }
            mq.queueAudioFrame(std::move(raw_packet), last_seq_popped, now);
            break;
        default:
            assert(0);
    }
}

PacketPointer Jitter::Audio::pop(std::chrono::steady_clock::time_point now)
{
    PacketPointer packet = mq.pop(now);

    if (packet)
    {
        fps.updatePopFrequency(now);
    }

    return packet;
}

unsigned int Jitter::Audio::getMsPerPacketInQueue()
{
    std::lock_guard<std::mutex> lock(mq.qMutex);
    for (auto &mf : mq.Q)
    {
        if (mf->type == MetaFrame::Type::media)
        {
            int media_type_size = 1;
            switch (mf->packet->mediaType)
            {
                case Packet::MediaType::L16:
                    media_type_size = sizeof(uint16_t);
                    break;
                case Packet::MediaType::F32:
                    media_type_size = sizeof(float);
                    break;
                default:
                    break;
            }
            size_t num_bytes = mf->packet->data.size();
            if (num_bytes > 0)
            {
                //
                unsigned int samples_per_channel = num_bytes / audio_channels /
                                                   media_type_size;
                unsigned int msPerPacket = 1000 / (audio_sample_rate /
                                                   samples_per_channel);
                return msPerPacket;
            }
        }
    }

    return 10;
}

unsigned int Jitter::Audio::getMsPerAudioPacket()
{
    if (ms_per_audio_packet == 0)
    {
        ms_per_audio_packet = getMsPerPacketInQueue();
    }

    return ms_per_audio_packet;
}

unsigned int Jitter::Audio::getFrameSize()
{
    unsigned int frames_per_sec = 0;

    if (ms_per_audio_packet != 0)
    {
        frames_per_sec = 1000 / ms_per_audio_packet;
    }
    else
    {
        frames_per_sec = 1000 / 10;        // default to 10ms if no packet has
                                           // been received
    }

    unsigned int samples_per_frame = audio_sample_rate / frames_per_sec;
    unsigned samples_per_channel = samples_per_frame * sizeof(float);
    return samples_per_channel * audio_channels;
}

unsigned int Jitter::Audio::getMsInQueue()
{
    return mq.size() * ms_per_audio_packet;
}

void Jitter::Audio::pruneAudioQueue(std::chrono::steady_clock::time_point now,
                                    unsigned int prune_target)
{
    unsigned int total_frames_in_queue = mq.size();
    unsigned int total_frames_target = prune_target / getMsPerAudioPacket();

    if (total_frames_in_queue > total_frames_target)
    {
        unsigned int prune_count = total_frames_in_queue - total_frames_target;

        for (size_t i = 0; i < prune_count; i++)
        {
            pop(now);
        }
    }
}

///
/// Jitter::Video
///

Jitter::Video::Video(uint32_t max_width,
                     uint32_t max_height,
                     uint32_t pixel_format) :
    assembler(nullptr),
    last_decoded_width(max_width),
    last_decoded_height(max_height),
    last_decoded_format(pixel_format),
    last_decoded_timestamp(0)
{
    lastDecodedFrame.resize(max_width * max_height * 12 /
                            8);        // YUV420 12 bits/pixel
    memset(
        lastDecodedFrame.data(), 0x80, lastDecodedFrame.size());        // Gray
}

PacketPointer Jitter::Video::pop(std::chrono::steady_clock::time_point now)
{
    PacketPointer packet = mq.pop(now);

    if (packet)
    {
        fps.updatePopFrequency(now);
    }

    return packet;
}

void Jitter::Video::push(PacketPointer raw_packet,
                         uint64_t last_seq_popped,
                         std::chrono::steady_clock::time_point now)
{
    switch (raw_packet->mediaType)
    {
        case Packet::MediaType::AV1:
        case Packet::MediaType::Raw:
            mq.queueVideoFrame(std::move(raw_packet), last_seq_popped, now);
            break;
        default:
            assert(0);
    }
}
