#pragma once

// Calculate drain speed from bucket to keep a steady fill level

#include <chrono>
#include <deque>
#include <qmedia/logger.hh>

namespace qmedia
{
class LeakyBucket
{
public:
    const unsigned int max_bucket_listener = 500;
    const unsigned int target_fill_listener = 150;
    const unsigned int max_bucket_active = 150;
    const unsigned int target_fill_active = 20;
    const bool use_cc = false;        // correlation coefficients are used to
                                      // learn if level is already growing or
                                      // shriking
    const bool smooth_drain_speed = false;        // smoothing of drain speed
                                                  // disabled

    LeakyBucket() :
        max_bucket_size(max_bucket_active),
        target_fill_level(target_fill_active),
        initial_fill(true),
        current_drain(DrainSpeed::normal),
        drain_time_changed(std::chrono::steady_clock::now())
    {
    }

    void emptyBucket(std::chrono::steady_clock::time_point now);
    unsigned int getRecommendedFillLevel(unsigned int audio_jitter_ms);

    typedef std::deque<
        std::pair<unsigned int, std::chrono::steady_clock::time_point>>
        Tracker;
    Tracker queue_depth_tracker;
    Tracker empty_pop_tracker;
    void pruneTracker(std::chrono::steady_clock::time_point now,
                      Tracker &tracker,
                      unsigned int interval);
    const unsigned int tracker_measurement_interval = 1000;
    void tick(std::chrono::steady_clock::time_point,
              unsigned int queue_depth,
              unsigned int lost_in_queue,
              unsigned int audio_jitter_ms,
              unsigned int ms_per_audio,
              unsigned int fps,
              LoggerPointer logger = nullptr);
    void adjustQueueDepthTrackerDiscardedPackets(int num);
    bool initialFill(unsigned int ms_in_queue, unsigned int jitter_ms, LoggerPointer logger=nullptr);
    double getSrcRatio();

private:
    unsigned int target_fill_level;
    bool initial_fill;
    unsigned int max_bucket_size;
    int fill_change;

    enum DrainSpeed
    {
        normal = 0,
        increased = 1,
        decreased = 2
    } current_drain;
    std::chrono::steady_clock::time_point drain_time_changed;

    // smoothing of drain speed disabled - not used atm
    Tracker drain_tracker;
    LeakyBucket::DrainSpeed getBestDrain();
};
}        // namespace qmedia