#include "playout_tools.hh"

using namespace neo_media;

///
/// PopFrequencyCounter
///

void PopFrequencyCounter::updatePopFrequency(
    std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock(pMutex);
    if (!first_pop)
    {
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - last_pop)
                         .count();
        last_pop = now;

        if ((interval_sum + delta) > measure_interval_ms)
        {
            interval_sum -= pop_time.front();
            pop_time.pop_front();
        }

        interval_sum += delta;
        pop_time.push_back(delta);
    }
    else
    {
        first_pop = false;
        last_pop = now;
    }
}

unsigned int PopFrequencyCounter::getAveragePopDelay()
{
    std::lock_guard<std::mutex> lock(pMutex);
    if (pop_time.empty()) return 0;
    return (interval_sum / pop_time.size());        // moving average
}

unsigned int PopFrequencyCounter::getFps()
{
    // moving average
    unsigned int delay_average = getAveragePopDelay();
    if (!delay_average) return 0;

    return (1000 / delay_average);
}

///
/// JitterCalc
///

void JitterCalc::updateJitterValues(MetaQueue &mq, unsigned int ms_per_packet)
{
    std::lock_guard<std::mutex> lock(mq.qMutex);

    // audioQ is sorted at all times
    if (ms_per_packet == 0)
    {
        return;
    }

    for (auto &meta : mq.Q)
    {
        // we need a pair of received media packets
        if (prev_jitter_seq == 0)
        {
            // use the previous type to temporarily identify and ignore
            // retransmitted packets
            if (meta->type == MetaFrame::Type::media &&
                meta->prev_type == MetaFrame::Type::none)
            {
                prev_jitter_time = meta->recvTime;
                prev_jitter_seq = meta->packet->encodedSequenceNum;
            }
            continue;
        }

        if (meta->type == MetaFrame::Type::media &&
            meta->prev_type == MetaFrame::Type::none)
        {
            uint64_t curr_seq = meta->packet->encodedSequenceNum;
            if (curr_seq <= prev_jitter_seq)
            {
                // we look at the whole buffer every time - skip if already
                // evaluated
                continue;
            }
            else if (curr_seq == (prev_jitter_seq + 1))
            {
                std::chrono::steady_clock::time_point curr_time = meta->recvTime;
                auto delta =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        curr_time - prev_jitter_time)
                        .count();
                auto jitter = abs(delta - ms_per_packet);
                jitter_values.push_back(jitter);
            }
            prev_jitter_time = meta->recvTime;
            prev_jitter_seq = curr_seq;
        }
    }

    // prune recorded jitter values
    unsigned int limit = jitter_window_size / ms_per_packet;
    if (jitter_values.size() > limit)
    {
        auto to_remove = jitter_values.size() - limit;
        for (size_t i = 0; i < to_remove; i++)
        {
            jitter_values.pop_front();
        }
    }
}

// temporary estimation until we can use RTT to do something more sensible
unsigned int JitterCalc::standard_deviation(unsigned int num_std)
{
    if (jitter_values.size()) return 0;

    float sum = 0;
    for (auto value : jitter_values)
    {
        sum += value;
    }

    float n = jitter_values.size();
    float mean = sum / n;

    float variance = 0;
    for (auto value : jitter_values)
    {
        variance += pow(value - mean, 2);
    }
    variance = variance / n;

    float standard_deviation = sqrt(variance);
    return (unsigned int) (num_std * standard_deviation) + 1;
}

unsigned int JitterCalc::getJitterMs()
{
    return standard_deviation(jitter_num_std);
}