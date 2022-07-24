#pragma once

#include <list>
#include "packet.hh"
#include "metrics.hh"

namespace qmedia
{
// cumulative metrics
class CumulativeMetrics
{
public:
    int total;               // totals audio or video frames received
    int total_popped;        // total audio or video frames popped by the client
    int lost;                // total lost frames in a stream
    int discarded;           // throw away as delay is too large for playout (or
                             // resampling not possible)
    int discarded_repeats;        // thrown away as two media packets with same
                                  // content was received
    int concealed_interpolated;        // dual side concealment
    int concealed_generated;           // single or no side concealment
    int missing;        // client asks for data - but none has been received
};

class MetaFrame
{
public:
    enum class Type
    {
        none,
        media,
        plc_gen,
        plc_dual        // Note: no dual plc is computed
    } type;

    MetaFrame(Type packet_type, std::chrono::steady_clock::time_point now) :
        type(packet_type), recvTime(now), prev_type(Type::none){};

    PacketPointer packet;
    Type prev_type;
    std::chrono::steady_clock::time_point recvTime;
};

class MetaQueue
{
public:
    std::mutex qMutex;
    bool empty();
    unsigned int size();
    std::shared_ptr<MetaFrame> front();
    void flushPackets();
    std::list<std::shared_ptr<MetaFrame>> Q;

    enum media_type
    {
        audio,
        video
    } type;

    std::map<media_type, std::string> media_name = {{audio, "audio"},
                                                    {video, "video"}};

    Metrics::Measurement::Fields getMetricFields(media_type type);
    CumulativeMetrics metrics = {};

    // APIs to push and retrieve data from jitter
    void queueVideoFrame(PacketPointer raw_packet,
                         uint64_t last_seq_popped,
                         std::chrono::steady_clock::time_point now);
    void queueAudioFrame(PacketPointer raw_packet,
                         uint64_t last_seq_popped,
                         std::chrono::steady_clock::time_point now);
    PacketPointer pop(std::chrono::steady_clock::time_point now);

    // count of media not received and replaced by PLCs
    unsigned int lostInQueue(unsigned int &numPlc, uint64_t last_seq_popped);
    unsigned int totalPacketBytes();
    uint64_t getNextSeq();
    uint64_t getNextSourceTime();
    LoggerPointer logger = nullptr;

private:
    void drainToMax();
    size_t max_size = 3000 * 30;        // RAW 1080p30 at 3000 packets per frame
                                        // for 1 sec
};

}        // namespace qmedia