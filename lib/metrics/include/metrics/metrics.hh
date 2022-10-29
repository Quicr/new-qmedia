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

struct InfluxConfig
{
    static const std::string url;
    static const std::string org;
    static const std::string bucket;
    static const std::string auth_token;
};

class Metrics
{
public:
    void add_measurement(const std::string& name, std::shared_ptr<Measurement> measurement);
    Metrics(CURL* handle);
    ~Metrics();
private:

    void emitMetrics();
    void sendMetrics(const std::vector<std::string>& collected_metrics);
    void push_loop();

    bool shutdown = false;

    std::mutex metrics_mutex;
    std::condition_variable cv;
    std::thread metrics_thread;
    std::map<std::string, std::shared_ptr<Measurement>> measurements;
    // TODO: make it RAII
    CURL *handle;
};

///
/// Factory
///


enum class MetricProvider {
    influx = 0
};

struct MetricsFactory
{
    static std::shared_ptr<Metrics> GetInfluxProvider();
    static std::map<MetricProvider, std::shared_ptr<Metrics>> metric_providers;
};

}

