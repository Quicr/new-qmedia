#include <cassert>
#include <iostream>

#include "jitter_queues.hh"

using namespace qmedia;

bool MetaQueue::empty()
{
    return Q.empty();
}

unsigned int MetaQueue::size()
{
    return Q.size();
}

std::shared_ptr<MetaFrame> MetaQueue::front()
{
    return Q.front();
}

unsigned int MetaQueue::totalPacketBytes()
{
    unsigned int total = 0;
    for (auto &meta_frame : Q)
    {
        total += meta_frame->packet->data.size();
    }

    return total;
}

void MetaQueue::flushPackets()
{
    while (!Q.empty())
    {
        auto p = std::move(Q.front()->packet);
        assert(p);
        Q.pop_front();
        p.reset();
    }
}

// utility function assuming existing lock
void MetaQueue::drainToMax()
{
    while (Q.size() > max_size)
    {
        // drain if too full
        // TODO - track metric here
        PacketPointer p = std::move(Q.front()->packet);
        assert(p);
        Q.pop_front();
        p.reset();
    }
}

uint64_t MetaQueue::getNextSeq()
{
    uint64_t next_seq = 0;
    if (!Q.empty())
    {
        next_seq = Q.front()->packet->encodedSequenceNum;
    }

    return next_seq;
}

uint64_t MetaQueue::getNextSourceTime()
{
    uint64_t source_time = 0;
    if (!Q.empty()) source_time = Q.front()->packet->sourceRecordTime;

    return source_time;
}

Metrics::Measurement::Fields MetaQueue::getMetricFields(media_type media_type)
{
    return {};
    if (!media_name.count(media_type))
    {
        std::clog << "media_type entries not found:" << type << std::endl;
        return {};
    }

    return {
        {media_name.at(type) + "Total", metrics.total},
        {media_name.at(type) + "TotalPopped", metrics.total_popped},
        {media_name.at(type) + "Lost", metrics.lost},
        {media_name.at(type) + "Discarded", metrics.discarded},
        {media_name.at(type) + "DiscardedRepeats", metrics.discarded_repeats},
        {media_name.at(type) + "ConcealedInterpolated",
         metrics.concealed_interpolated},
        {media_name.at(type) + "ConcealedGenerated",
         metrics.concealed_generated},
        {media_name.at(type) + "Missing", metrics.missing},
        {media_name.at(type) + "QueueDepth", Q.size()}};
}

///
///  Jitter Push/Pop API
///

void MetaQueue::queueAudioFrame(PacketPointer raw_packet,
                                uint64_t last_seq_popped,
                                std::chrono::steady_clock::time_point now)
{
    uint64_t new_seq = raw_packet->encodedSequenceNum;
    // check if it is not too old play - throw it
    if (last_seq_popped != 0 && new_seq <= last_seq_popped)
    {
        raw_packet.reset();
        ++metrics.discarded;
        return;
    }

    std::lock_guard<std::mutex> lock(qMutex);
    uint64_t inc_sourceID = raw_packet->sourceID;
    // setup meta frame for new incoming media
    auto frame = std::make_shared<MetaFrame>(MetaFrame::Type::media, now);
    frame->packet = std::move(raw_packet);

    // normal case is to push new packets at the back of the queue
    if (Q.empty() || (new_seq > Q.back()->packet->encodedSequenceNum))
    {
        Q.push_back(frame);
        ++metrics.total;
        return;
    }

    // out-of-order, retransmissions replacing plc etc.
    for (auto it = Q.begin(); it != Q.end(); ++it)
    {
        uint64_t curr_seq = (*it)->packet->encodedSequenceNum;
        if (new_seq < curr_seq)
        {
            // new frame is older than the one in queue
            Q.insert(it, frame);
            ++metrics.total;
            break;
        }
        else if (new_seq == curr_seq)
        {
            switch ((*it)->type)
            {
                case MetaFrame::Type::media:
                    // retransmitted frame has been previously retransmitted -
                    // skip
                    ++metrics.discarded_repeats;
                    return;
                case MetaFrame::Type::plc_dual:
                    if (frame->type == MetaFrame::Type::media)
                    {
                        // retransmitted media is better than plc
                        frame->prev_type = (*it)->type;
                        --metrics.concealed_interpolated;
                        auto pos = Q.erase(it);
                        Q.insert(pos, frame);
                        ++metrics.total;
                        return;
                    }
                    break;
                case MetaFrame::Type::plc_gen:
                    if (frame->type == MetaFrame::Type::media ||
                        frame->type == MetaFrame::Type::plc_dual)
                    {
                        // plc_gen considered lowest quality frame - replace
                        // with a better frame
                        frame->prev_type = (*it)->type;
                        --metrics.concealed_generated;
                        auto pos = Q.erase(it);
                        Q.insert(pos, frame);

                        if (frame->type == MetaFrame::Type::media)
                            ++metrics.total;
                        else
                            ++metrics.concealed_interpolated;
                        return;
                    }
                    break;
                default:
                    break;
            }
        }
    }

}

PacketPointer MetaQueue::pop(std::chrono::steady_clock::time_point now)
{
    // Lock should occur by the calling client to lock for the appropriate
    // context
    //    std::lock_guard<std::mutex> lock(qMutex);

    if (Q.empty())
    {
        ++metrics.missing;
        return nullptr;
    }

    PacketPointer packet = std::move(Q.front()->packet);
    Q.pop_front();
    ++metrics.total_popped;
    return packet;
}

// TODO: retransmit replacements, interpolated frames and super sampling
void MetaQueue::queueVideoFrame(PacketPointer raw_packet,
                                uint64_t last_seq_popped,
                                std::chrono::steady_clock::time_point now)
{
    uint64_t new_seq = raw_packet->encodedSequenceNum;

    // check if it is not too old play - throw it
    if (last_seq_popped != UINT64_MAX && new_seq <= last_seq_popped)
    {
        raw_packet.reset();
        ++metrics.discarded;
        return;
    }

    std::lock_guard<std::mutex> lock(qMutex);
    auto frame = std::make_shared<MetaFrame>(MetaFrame::Type::media, now);
    frame->packet = std::move(raw_packet);

    // normal case is to push new packets at the back of the queue
    if (Q.empty() || (new_seq > Q.back()->packet->encodedSequenceNum))
    {
        Q.push_back(frame);
        ++metrics.total;
        drainToMax();
        return;
    }

    // Frame is out of order
    for (auto it = Q.begin(); it != Q.end(); ++it)
    {
        uint64_t curr_seq = (*it)->packet->encodedSequenceNum;
        if (new_seq < curr_seq)
        {
            // new frame is older than the one in queue
            Q.insert(it, frame);
            ++metrics.total;
            drainToMax();
            return;
        }
        else
        {
            if (new_seq == curr_seq)
            {
                // retransmitted frame has been previously retransmitted - skip
                ++metrics.discarded_repeats;
                return;
            }
        }
    }
}

unsigned int MetaQueue::lostInQueue(unsigned int &numPlc,
                                    uint64_t last_seq_popped)
{
    unsigned int lost = 0;
    numPlc = 0;
    uint64_t prev_seq = last_seq_popped;

    for (auto &meta : Q)
    {
        if (meta->packet->encodedSequenceNum != (prev_seq + 1))
        {
            ++lost;
        }
        prev_seq = meta->packet->encodedSequenceNum;

        if (meta->type == MetaFrame::Type::plc_gen ||
            meta->type == MetaFrame::Type::plc_dual)
        {
            ++numPlc;
        }
    }
    return lost;
}
