#pragma once

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <list>
#include <chrono>

namespace neo_media
{
struct MetricsConfig
{
    static const std::string URL;
    static const std::string ORG;
    static const std::string BUCKET;
    static const std::string AUTH_TOKEN;
};

class Metrics
{
public:
    // Support influx 2.0 API
    Metrics(const std::string &influx_url,
            const std::string &org,
            const std::string &bucket,
            const std::string &auth_token);
    ~Metrics();

    typedef std::shared_ptr<Metrics> MetricsPtr;

    class Measurement
    {
    public:
        typedef std::pair<std::string, uint64_t> Field;
        typedef std::list<Field> Fields;
        typedef std::pair<std::string, uint64_t> Tag;
        typedef std::pair<long long, Fields> time_entry;
        typedef std::list<Tag> Tags;
        struct TimeEntry
        {
            Tags tags;
            Fields fields;
        };
        using TimeSeriesEntry = std::pair<long long, TimeEntry>;

        void set(std::chrono::system_clock::time_point now,
                 std::list<Field> fields);
        void set(std::chrono::system_clock::time_point now, Field field);
        void set_time_entry(std::chrono::system_clock::time_point now,
                            TimeEntry &&entry);
        std::list<std::string> lineProtocol();

        Measurement(std::string &name, Tags &tags);
        ~Measurement();

        // don't want to store redundant string - so class offers a simple
        // converter to ints
        int fieldIndex;
        std::map<std::string, int> fieldIds;
        std::map<int, std::string> fieldNames;

        std::string name;
        Tags tags;

        std::mutex series_lock;
        std::list<TimeSeriesEntry> series;
        std::list<time_entry> time_series;

        std::string lineProtocol_nameAndTags();
        std::string lineProtocol_nameAndTags(Tags &tags);
        std::string lineProtocol_fields(Fields &fields);

        bool sent = false;
    };

    void pusher();
    void push();        // push(std::string & name) - specific push
    typedef std::shared_ptr<Measurement> MeasurementPtr;
    typedef std::pair<std::string, int> tag;
    MeasurementPtr createMeasurement(std::string name, Measurement::Tags tags);

    bool shutdown = false;
    bool push_signals = false;
    std::mutex metrics_mutex;
    std::condition_variable cv;
    std::mutex push_mutex;
    std::thread metrics_thread;
    std::map<std::string, Metrics::MeasurementPtr> measurements;
    CURL *handle;
};

}        // namespace neo_media
