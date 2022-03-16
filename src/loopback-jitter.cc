#include <cassert>
#include <iostream>
#include <cstring>        // memeset

#include "loopback-jitter.hh"
#include "h264_decoder.hh"

using namespace neo_media;

LoopbackJitter::LoopbackJitter(unsigned int audio_sample_rate,
                               unsigned int audio_channels,
                               Packet::MediaType audio_decode_type,
                               uint32_t video_max_width,
                               uint32_t video_max_height,
                               uint32_t video_decode_pixel_format,
                               const LoggerPointer &parent_logger) :
    audio(audio_sample_rate, audio_channels),
    decode_audio_as(audio_decode_type),
    video(video_max_width, video_max_height, video_decode_pixel_format)
{
    logger = std::make_shared<Logger>("Loopback-Jitter", parent_logger);
    video.decoder = nullptr;
    video.decoder = new H264Decoder(video_decode_pixel_format);

    if (!video.decoder)
    {
        logger->error << "H264 video decoder init failed" << std::flush;
    }
}

LoopbackJitter::~LoopbackJitter()
{
    audio.flushPackets();
    video.flushPackets();
}

bool LoopbackJitter::push(PacketPointer packet)
{
    bool new_stream = false;
    uint64_t sourceID = packet->sourceID;
    uint64_t clientID = packet->clientID;

    {
        assert(packet);
        switch (packet->mediaType)
        {
            case Packet::MediaType::AV1:
            {
                if (video.assembler == nullptr)
                {
                    video.assembler = std::make_shared<SimpleVideoAssembler>();
                    new_stream = true;
                }

                PacketPointer raw = video.assembler->push(std::move(packet));
                if (raw != nullptr)
                {
                    video.push(std::move(raw));
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
                }

                PacketPointer raw = audio.opus_assembler->push(
                    std::move(packet));
                if (raw != nullptr)
                {
                    audio.push(std::move(raw));
                }
            }
            break;
            default:
                break;
        }
    }

    return new_stream;
}

PacketPointer LoopbackJitter::popAudio(uint64_t sourceID,
                                       unsigned int /*length*/)
{
    PacketPointer packet = nullptr;

    if (sourceID != audio.sourceID)
    {
        logger->warning << "SrcID?" << sourceID
                        << " Audio source id:" << audio.sourceID << std::flush;
        return packet;
    }
    std::lock_guard<std::mutex> lock(audio.qMutex);

    packet = audio.pop();
    if (packet == nullptr)
    {
        packet = audio.createPLC(sourceID, audio.getFrameSize());
        logger->debug << "P" << std::flush;
    }
    return packet;
}

unsigned int LoopbackJitter::Audio::getFrameSize()
{
    unsigned int frames_per_sec = 0;
    /*
        if (ms_per_audio_packet != 0) {
            frames_per_sec = 1000 / ms_per_audio_packet;
        } else {
            frames_per_sec = 1000 / 10;  // default to 10ms if no packet has
       been received
        }
    */
    // THis is a hack and should get the right value from the queue
    frames_per_sec = 1000 / 10;        // default to 10 ms

    unsigned int samples_per_frame = audio_sample_rate / frames_per_sec;
    unsigned samples_per_channel = samples_per_frame * sizeof(float);
    return samples_per_channel * audio_channels;
}

LoopbackJitter::Video::Video(uint32_t max_width,
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

int LoopbackJitter::setDecodedFrame(uint64_t /*sourceID*/,
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

int LoopbackJitter::popVideo(uint64_t sourceID,
                             uint32_t &width,
                             uint32_t &height,
                             uint32_t &format,
                             uint64_t &timestamp,
                             unsigned char **buffer,
                             Packet::IdrRequestData & /*idr_data_out*/)
{
    PacketPointer packet = nullptr;
    int len = 0;

    if (sourceID == video.sourceID)
    {
        logger->debug << "[QV:" << video.Q.size() << "]" << std::flush;
        std::lock_guard<std::mutex> lock(video.qMutex);

        packet = video.pop();
        if (packet == nullptr)
        {
            len = setDecodedFrame(
                sourceID, width, height, format, timestamp, buffer);
            logger->debug << "[EQ]" << std::flush;
            return len;
        }

        int error;
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
                }
                else
                {
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
                video.lastDecodedFrame.swap(packet->data);
                packet.reset();
                break;
            default:
                logger->error
                    << "unknown video packet type: " << (int) packet->mediaType
                    << std::flush;
                assert(0);        // TODO: handle more gracefully
                break;
        }
    }
    else        // not audio or video sourceID
    {
        logger->warning << "SrcID?" << sourceID
                        << " VideoQSrcId:" << video.sourceID << std::flush;
    }
    return len;
}

LoopbackJitter::Audio::Audio(unsigned int sample_rate, unsigned int channels) :
    sourceID(0),
    audio_sample_rate(sample_rate),
    audio_channels(channels),
    opus_assembler(nullptr)
{
}

PacketPointer LoopbackJitter::Audio::createPLC(int /*sourceID*/,
                                               unsigned int size)
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

PacketPointer LoopbackJitter::Audio::createZeroPayload(unsigned int size)
{
    PacketPointer packet = std::make_unique<Packet>();
    packet->data.resize(size);
    std::fill(packet->data.begin(), packet->data.end(), 0);
    return packet;
}

void LoopbackJitter::Audio::push(PacketPointer packet)
{
    switch (packet->mediaType)
    {
        case Packet::MediaType::F32:
        case Packet::MediaType::L16:
        case Packet::MediaType::Opus:
            queueAudioFrame(std::move(packet));
            break;
        default:
            assert(0);
    }
}

void LoopbackJitter::Audio::queueAudioFrame(PacketPointer packet)
{
    sourceID = packet->sourceID;
    std::lock_guard<std::mutex> lock(qMutex);
    Q.push_back(std::move(packet));
}

PacketPointer LoopbackJitter::Audio::pop()
{
    if (Q.empty())
    {
        return nullptr;
    }

    PacketPointer packet = std::move(Q.front());
    Q.pop_front();
    return packet;
}

PacketPointer LoopbackJitter::Video::pop()
{
    if (Q.empty())
    {
        return nullptr;
    }

    PacketPointer packet = std::move(Q.front());
    Q.pop_front();
    return packet;
}

void LoopbackJitter::Video::push(PacketPointer raw_packet)
{
    switch (raw_packet->mediaType)
    {
        case Packet::MediaType::AV1:
        case Packet::MediaType::Raw:
            queueVideoFrame(std::move(raw_packet));
            break;
        default:
            assert(0);
    }
}

void LoopbackJitter::Video::queueVideoFrame(PacketPointer raw_packet)
{
    sourceID = raw_packet->sourceID;
    std::lock_guard<std::mutex> lock(qMutex);
    Q.push_back(std::move(raw_packet));
}

void LoopbackJitter::Audio::flushPackets()
{
    std::lock_guard<std::mutex> lock(qMutex);

    while (!Q.empty())
    {
        PacketPointer p = std::move(Q.front());
        assert(p);
        Q.pop_front();
        p.reset();
    }
}

void LoopbackJitter::Video::flushPackets()
{
    std::lock_guard<std::mutex> lock(qMutex);

    while (!Q.empty())
    {
        PacketPointer p = std::move(Q.front());
        assert(p);
        Q.pop_front();
        p.reset();
    }
}
