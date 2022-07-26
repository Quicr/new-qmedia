#include <cstring>        // memeset
#include <cassert>
#include <iostream>
#include <iomanip>

#include "jitter.hh"
#include "h264_decoder.hh"

using namespace qmedia;

static std::string to_hex(const std::vector<uint8_t> &data)
{
    std::stringstream hex(std::ios_base::out);
    hex.flags(std::ios::hex);
    for (const auto &byte : data)
    {
        hex << std::setw(2) << std::setfill('0') << int(byte);
    }
    return hex.str();
}

///
/// Jitter
///
Jitter::Jitter(const LoggerPointer &parent_logger,
               Metrics::MetricsPtr metricsPtr) :
    idle_client(true), metrics(metricsPtr)
{
    logger = std::make_shared<Logger>("Jitter", parent_logger);
}

Jitter::~Jitter()
{
    shutdown = true;
    audio.mq.flushPackets();
    video.mq.flushPackets();
}

void Jitter::set_audio_params(unsigned int audio_sample_rate,
                              unsigned int audio_channels,
                              Packet::MediaType audio_decode_type)
{
    audio.audio_sample_rate = audio_sample_rate;
    audio.audio_channels = audio_channels;
    decode_audio_as = audio_decode_type;
}

void Jitter::set_video_params(uint32_t video_max_width,
                              uint32_t video_max_height,
                              uint32_t video_decode_pixel_format)
{
    video.last_decoded_width = video_max_width;
    video.last_decoded_height = video_max_height;
    video.last_decoded_format = video_decode_pixel_format;
    video.lastDecodedFrame.resize(video_max_width * video_max_width * 12 /
                                  8);        // YUV420 12 bits/pixel
    memset(video.lastDecodedFrame.data(),
           0x80,
           video.lastDecodedFrame.size());        // Gray

    video.decoder = std::make_unique<H264Decoder>(video_decode_pixel_format);
    logger->info << "[set_video_params]" << video.last_decoded_width << ","
                << video.last_decoded_height << ","
                 << video.last_decoded_format << std::flush;
    assert(video.decoder);
}

void Jitter::recordMetrics(MetaQueue &q,
                           MetaQueue::media_type type,
                           uint64_t clientID,
                           uint64_t sourceID)
{
    if (metrics != nullptr)
    {
        if (measurement == nullptr)
        {
            measurement = metrics->createMeasurement(
                "jitter", {{"clientID", clientID}, {"sourceID", sourceID}});
        }

        if (measurement != nullptr)
        {
            try
            {
                measurement->set(std::chrono::system_clock::now(),
                                 q.getMetricFields(type));
            }
            catch (std::exception &e)
            {
                logger->warning << "metrics exception, ignore" << std::flush;
            }
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
            case Packet::MediaType::H264:
            case Packet::MediaType::Raw:
            {
                if (!video.sourceID)
                {
                    new_stream = true;
                    logger->info << "New video sourceID: " << sourceID
                                 << std::flush;
                    video.sourceID = packet->sourceID;
                }

                // we got assembled frame, add it to jitter queue
                auto seq = packet->encodedSequenceNum;
                video.push(std::move(packet), sync.video_seq_popped, now);
                logger->debug << "[jitter-v: seq_no:" << seq
                             << ", q-size:" << video.mq.size() << std::flush;

                break;
            }
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
                }

                PacketPointer raw = audio.opus_assembler->push(
                    std::move(packet));
                if (raw != nullptr)
                {
                    auto seq = raw->encodedSequenceNum;
                    auto dsize = raw->data.size();
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

    if (idle_client)
    {
        idle_client = false;
    }

    if (sourceID != audio.sourceID)
    {
        logger->warning << "SrcID?" << sourceID
                        << " Audio source id:" << audio.sourceID << std::flush;
        return nullptr;
    }

    logger->debug << "[J-PopAudio: Q-depth:" << audio.mq.size() << "*"
                 << audio.ms_per_audio_packet << "=" << audio.getMsInQueue()
                 << "]" << std::flush;
    logger->debug << "[J-PopAudio: Jitter-ms:" << audio_jitter.getJitterMs()
                 << "]" << std::flush;
    logger->debug << "[J-PopAudio: Asking Length:" << length << "]"
                 << std::flush;
    logger->debug << "[J-PopAudio: Playing total in buffers"
                 << audio.playout.getTotalInBuffers(nullptr) << "]" << std::flush;

    QueueMonitor(now);
    int num_depth_adjustments = 1;

    // loop to ensure clients asking for variable length data
    while (audio.playout.getTotalInBuffers(logger) < length)
    {
        if (bucket.initialFill(audio.getMsInQueue(),
                               audio_jitter.getJitterMs(), logger))
        {
            // we don't have anything in our buffers, create PLC
            packet = audio.createPLC(audio.getFrameSize());
            packet->sourceRecordTime = 0;
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
    audio.playout.fill(logger, packet->data, length, timestamp);
    packet->encodedSequenceNum = sync.audio_seq_popped;
    packet->sourceRecordTime = timestamp;
    return packet;
}

int Jitter::popVideo(uint64_t sourceID,
                     uint32_t &width,
                     uint32_t &height,
                     uint32_t &format,
                     uint64_t &timestamp,
                     unsigned char **buffer)
{
    return popVideo(sourceID,
                    width,
                    height,
                    format,
                    timestamp,
                    buffer,
                    std::chrono::steady_clock::now());
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
        case Packet::MediaType::H264:
            error = video.decoder->decode((const char *) &packet->data[0],
                                          packet->data.size(),
                                          width,
                                          height,
                                          format,
                                          video.lastDecodedFrame);
            if (error)
            {
                logger->error << "[Jitter::decodeVideoPacket]" << error << "]" << std::endl;
                // reuse the last decoded frame / parameters
            } else {
                sync.video_popped(
                    packet->sourceRecordTime, packet->encodedSequenceNum, now);
                // update the last decoded frame / parameters
                video.last_decoded_format = (uint8_t) VideoConfig::PixelFormat::I420;
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
                     std::chrono::steady_clock::time_point now)
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

    logger->debug << "[Jitter:popVideo]:"<< sourceID
                 << ", queue has: " << video.mq.size() << " ]"
                 << std::flush;

    if (idle_client)
    {
        idle_client = false;
        // remove this
        sync.logger = logger;
    }

    if (video.mq.empty())
    {
        // return the last decoded frame
        len = setDecodedFrame(
            sourceID, width, height, format, timestamp, buffer);
        logger->debug << "[EQ]" << std::flush;
        return len;
    }

    std::lock_guard<std::mutex> lock(video.mq.qMutex);
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
            logger->debug << "[POP]: num_pops:" << num_pop << std::flush;
            for (unsigned int pops = 0; pops < num_pop; pops++)
            {
                packet = video.pop(now);
                if (packet != nullptr)
                {
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
                if (packet)
                {
                    packet.reset(nullptr);
                }
            }
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            logger->debug << "jitter: popDiscard: keyFrame requested"
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
            logger->debug << "jitter: popVideoOnly" << std::flush;
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
    logger->debug << "[JQM: last_seq_popped:]" << sync.audio_seq_popped << std::flush;
    unsigned int lost_in_queue = audio.mq.lostInQueue(plcs,
                                                      sync.audio_seq_popped);
    logger->debug << "[JQM: lostInQueue:" << lost_in_queue << ",plcs:" << plcs
                  << "]" << std::flush;
    // total number of audio frames in queue
    unsigned int queue_size = audio.getMsInQueue();
    logger->debug << "[JQM: getMsPerAudioPacket" << audio.getMsPerAudioPacket() << "]" << std::flush;
    logger->debug << "[JQM: Q-Size == getMsPerAudioPacket:" << queue_size << "]" << std::flush;
    // unsigned int queue_size = audio.mq.size();
    // average jitter since the last pop
    unsigned int jitter_ms = audio_jitter.getJitterMs();
    logger->debug << "[JQM: averagee audio-jitter-ms:" << jitter_ms << "]" << std::flush;
    unsigned int ms_per_audio = audio.getMsPerAudioPacket();
    logger->debug << "[JQM: ms_per_audio:" << ms_per_audio << "]" << std::flush;
    unsigned int client_fps = audio.fps.getFps();
    logger->debug << "[JQM: client_fps:" << client_fps << "]" << std::flush;
    bucket.tick(
        now, queue_size, lost_in_queue, jitter_ms, ms_per_audio, client_fps, logger);
}

///
/// Jitter::Audio
///

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

unsigned int Jitter::Audio::getMsPerPacketInQueue(LoggerPointer logger)
{
    std::lock_guard<std::mutex> lock(mq.qMutex);
    if(logger) {
        logger->debug << "Audio::getMsPerPacketInQueue:" << mq.Q.size() << std::flush;
    }
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
            if(logger) {
                logger->debug << "Audio::getMsPerPacketInQueue: num_bytes" << num_bytes << std::flush;
            }

            if (num_bytes > 0)
            {
                //
                unsigned int samples_per_channel = num_bytes / audio_channels /
                                                   media_type_size;
                if(logger) {
                    logger->debug << "Audio::getMsPerPacketInQueue: samples_per_channel" << samples_per_channel << std::flush;
                }
                unsigned int msPerPacket = 1000 / (audio_sample_rate /
                                                   samples_per_channel);

                if(logger) {
                    logger->debug << "Audio::getMsPerPacketInQueue: audio_sample_rate" << audio_sample_rate << std::flush;
                    logger->debug << "Audio::getMsPerPacketInQueue: msPerPacket" << msPerPacket << std::flush;
                }
                return msPerPacket;
            }
        }
    }

    return 10;
}

unsigned int Jitter::Audio::getMsPerAudioPacket(LoggerPointer logger)
{
    if (ms_per_audio_packet == 0)
    {
        ms_per_audio_packet = getMsPerPacketInQueue(logger);
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

    unsigned int samples_per_frame = audio_sample_rate / 100;
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
        case Packet::MediaType::H264:
        case Packet::MediaType::Raw:
            mq.queueVideoFrame(std::move(raw_packet), last_seq_popped, now);
            break;
        default:
            assert(0);
    }
}

///
/// JitterFactory
///

std::map<uint64_t, std::shared_ptr<Jitter>> JitterFactory::jitters = {};

std::shared_ptr<Jitter> JitterFactory::GetJitter(LoggerPointer logger, uint64_t client_id)
{
    if (auto it{jitters.find(client_id)}; it != std::end(jitters))
    {
        return it->second;
    }

    // create a new jitter
    auto jitter = std::make_shared<Jitter>(logger);
    jitters.emplace(client_id, jitter);
    return jitters.at(client_id);
}
