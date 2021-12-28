#include "playout_leakybucket.hh"
#include <math.h>

using namespace neo_media;

void LeakyBucket::emptyBucket(std::chrono::steady_clock::time_point now)
{
    empty_pop_tracker.emplace_back(1, now);
}

unsigned int LeakyBucket::getRecommendedFillLevel(unsigned int audio_jitter_ms)
{
    unsigned int new_target = target_fill_level;
    unsigned int min_jitter_ms = audio_jitter_ms;

    if (min_jitter_ms > new_target) new_target = min_jitter_ms;

    // add calculations about RTT here - and adjust for the need for n x
    // retransmissions

    if (new_target > max_bucket_size) new_target = max_bucket_size;

    return new_target;
}

void LeakyBucket::pruneTracker(std::chrono::steady_clock::time_point now,
                               Tracker &tracker,
                               unsigned int interval)
{
    std::chrono::steady_clock::time_point interval_cut_off =
        now - std::chrono::milliseconds(interval);

    for (auto pair : tracker)
    {
        if (pair.second < interval_cut_off)
            tracker.pop_front();
        else
            break;
    }
}

LeakyBucket::DrainSpeed LeakyBucket::getBestDrain()
{
    unsigned int normal_count = 0;
    unsigned int increased_count = 0;
    unsigned int decreased_count = 0;

    for (auto dr : drain_tracker)
    {
        switch (dr.first)
        {
            case normal:
                ++normal_count;
                break;
            case increased:
                ++increased_count;
                break;
            case decreased:
                ++decreased_count;
                break;
        }
    }
    unsigned int num = drain_tracker.size();
    unsigned int limit = num / 2;

    if (increased_count > limit)
        return DrainSpeed::increased;
    else if (decreased_count > limit)
        return DrainSpeed::decreased;
    return DrainSpeed::normal;
}

void LeakyBucket::tick(std::chrono::steady_clock::time_point now,
                       unsigned int queue_depth,
                       unsigned int lost_in_queue,
                       unsigned int audio_jitter_ms,
                       unsigned int ms_per_audio,
                       unsigned int fps)
{
    if (initialFill(queue_depth, audio_jitter_ms)) return;

    queue_depth_tracker.emplace_back(queue_depth, now);

    pruneTracker(now, queue_depth_tracker, tracker_measurement_interval);
    pruneTracker(now, empty_pop_tracker, tracker_measurement_interval);

    // unsigned int splash_size_ms = getSplashSize(ms_per_audio);
    unsigned int minimum_fill_level_ms = audio_jitter_ms;
    unsigned int new_target_fill_ms = (target_fill_level >
                                       minimum_fill_level_ms) ?
                                          target_fill_level :
                                          minimum_fill_level_ms;
    if (new_target_fill_ms > max_bucket_size)
        new_target_fill_ms = max_bucket_size;

    int current_fill_level_ms = queue_depth * ms_per_audio;

    fill_change = current_fill_level_ms - new_target_fill_ms;

    if (fill_change > 10)
        current_drain = increased;
    else if (fill_change < -10)
        current_drain = decreased;
    else
        current_drain = normal;
}

bool LeakyBucket::initialFill(unsigned int ms_in_queue, unsigned int jitter_ms)
{
    if (initial_fill)
    {
        // jitter vs target vs max
        unsigned int start_ms = getRecommendedFillLevel(jitter_ms);

        if (ms_in_queue >= start_ms)
        {
            initial_fill = false;
        }
    }
    return initial_fill;
}

// do something smarter than always 10% resampling
double LeakyBucket::getSrcRatio()
{
    switch (current_drain)
    {
        case normal:
            return 1.0;
        case increased:
            return 0.9;
        case decreased:
            return 1.1;
    }
    return 1.0;
}

void LeakyBucket::adjustQueueDepthTrackerDiscardedPackets(int num)
{
    for (auto &depth : queue_depth_tracker)
    {
        depth.first += num;
    }
}
