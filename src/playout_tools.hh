#pragma once

#include <chrono>
#include <mutex>
#include <deque>

#include "jitter_queues.hh"

namespace qmedia
{
// Class to track media popped by clients
class PopFrequencyCounter
{
public:
    void updatePopFrequency(std::chrono::steady_clock::time_point now);
    unsigned int getAveragePopDelay();
    unsigned int getFps();

private:
    std::mutex pMutex;
    std::deque<unsigned int> pop_time;
    std::chrono::steady_clock::time_point last_pop;
    bool first_pop = true;
    unsigned int measure_interval_ms = 1000;
    unsigned int interval_sum = 0;
};

// Sliding window of jitter values and operations
class JitterCalc
{
public:
    const unsigned int window_size = 1000;

    JitterCalc() :
        prev_jitter_seq(0), jitter_window_size(window_size), jitter_num_std(4)
    {
    }

    void updateJitterValues(MetaQueue &mq, unsigned int ms_per_packet);
    unsigned int getJitterMs();
    unsigned int standard_deviation(unsigned int num_std);

    std::deque<unsigned int> jitter_values;
    uint64_t prev_jitter_seq;
    std::chrono::steady_clock::time_point prev_jitter_time;
    unsigned int jitter_window_size;
    unsigned int jitter_num_std;
};

}        // namespace qmedia