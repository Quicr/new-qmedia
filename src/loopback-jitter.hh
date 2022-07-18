#pragma once

#include <mutex>
#include <list>
#include <math.h>
#include <iostream>
#include "packet.hh"
#include "frame_assembler.hh"
#include "opus_assembler.hh"
#include "simple_video_assembler.hh"
#include <queue>
#include <map>
#include "packet.hh"
#include <iostream>
#include <ostream>
#include "qmedia/logger.hh"
#include "codec.hh"
#include "jitter_interface.hh"
#include "qmedia/metrics.hh"

namespace neo_media
{
class LoopbackJitter : public JitterInterface
{
public:
    typedef std::shared_ptr<LoopbackJitter> LoopbackJitterPtr;
    explicit LoopbackJitter(unsigned int audio_sample_rate,
                            unsigned int audio_channels,
                            Packet::MediaType audio_decode_type,
                            uint32_t video_max_width,
                            uint32_t video_max_height,
                            uint32_t video_decode_pixel_format,
                            const LoggerPointer &parent_logger = nullptr);

    ~LoopbackJitter();

    bool push(PacketPointer packet);
    int popVideo(uint64_t sourceID,
                 uint32_t &width,
                 uint32_t &height,
                 uint32_t &format,
                 uint64_t &timestamp,
                 unsigned char **buffer,
                 Packet::IdrRequestData &idr_data_out);
    PacketPointer popAudio(uint64_t sourceID, unsigned int length);

    class Audio
    {
    public:
        Audio(unsigned int sample_rate, unsigned int channels);
        void push(PacketPointer raw_packet);
        PacketPointer pop();
        void flushPackets();
        PacketPointer createPLC(int sourceID, unsigned int size);
        PacketPointer createZeroPayload(unsigned int size);
        unsigned int getFrameSize();
        std::mutex qMutex;
        std::list<PacketPointer> Q;
        uint64_t sourceID;
        unsigned int audio_channels;
        unsigned int audio_sample_rate;
        std::shared_ptr<OpusAssembler> opus_assembler;

    private:
        void queueAudioFrame(PacketPointer raw_packet);
    } audio;

    class Video
    {
    public:
        Video(uint32_t max_width, uint32_t max_height, uint32_t pixel_format);
        void queueVideoFrame(PacketPointer raw_packet);
        void push(PacketPointer raw_packet);
        PacketPointer pop();
        void flushPackets();
        std::mutex qMutex;
        std::list<PacketPointer> Q;
        uint64_t sourceID;
        std::shared_ptr<SimpleVideoAssembler> assembler;
        VideoDecoder *decoder;
        std::vector<std::uint8_t> lastDecodedFrame;
        uint32_t last_decoded_width;
        uint32_t last_decoded_height;
        uint32_t last_decoded_format;
        uint64_t last_decoded_timestamp;
    } video;

    Packet::MediaType decode_audio_as;
    LoggerPointer logger;
    int setDecodedFrame(uint64_t sourceID,
                        uint32_t &width,
                        uint32_t &height,
                        uint32_t &format,
                        uint64_t &timestamp,
                        unsigned char **buffer);
};

}        // namespace neo_media
