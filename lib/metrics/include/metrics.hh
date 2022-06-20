#pragma once

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <list>
#include <chrono>

#include "measurements.hh"

namespace metrics
{

class Metrics
{
public:
    typedef std::shared_ptr<Metrics> MetricsPtr;

    void pusher();
    void push();        // push(std::string & name) - specific push

    std::mutex metrics_mutex;
    std::condition_variable cv;
    std::mutex push_mutex;
    std::map<std::string, Metrics::MeasurementPtr> measurements;
};

}
