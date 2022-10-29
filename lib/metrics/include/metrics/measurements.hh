#pragma once
#include <map>
#include <string>
#include <list>
#include <mutex>
#include <memory>

namespace metrics {

enum struct MeasurementType
{
    FrameRate_Tx,    // frame count before transmit
    FrameRate_Rx,    // frame count from transport after re-assembly
    EncodeTime,      // time elapsed for encoding a raw sample (ms)
    EndToEndFrameDelay, // tx to rx latency (ms)
};

const auto MeasurementNames = std::map<MeasurementType, std::string>{
    {MeasurementType::FrameRate_Tx, "TxFrameCount"},
    {MeasurementType::FrameRate_Rx, "RxFrameCount"},
    {MeasurementType::EncodeTime, "EncodeTimeInMs"},
    {MeasurementType::EndToEndFrameDelay, "EndToEndFrameDelayInMs"},
};

///
/// Measurement
///
///

struct Measurement {
    virtual ~Measurement() = default;
    virtual std::string serialize() = 0;
};


// handy defines
using Field =  std::pair<std::string, uint64_t>;
using Fields =  std::list<Field>;
using Tag = std::pair<std::string, uint64_t>;
using Tags = std::list<Tag>;
using TimePoint = std::pair<long long, Fields>;

class InfluxMeasurement : public Measurement
{
public:
    static std::shared_ptr<InfluxMeasurement> create(std::string name, Tags tags);

    InfluxMeasurement(std::string &name_in, Tags &tags_in);
    ~InfluxMeasurement()  = default;

    // Measurement - to line protocol
    std::string serialize() override;

    struct TimeEntry
    {
        Tags tags;
        Fields fields;
    };

    using TimeSeriesEntry = std::pair<long long, TimeEntry>;

    // Setters for the measurement
    void set(std::chrono::system_clock::time_point now,
             std::list<Field> fields);
    void set(std::chrono::system_clock::time_point now, Field field);
    void set_time_entry(std::chrono::system_clock::time_point now,
                        TimeEntry &&entry);

    std::list<std::string> lineProtocol();

    int fieldIndex;
    std::map<std::string, int> fieldIds;
    std::map<int, std::string> fieldNames;
    std::string name;
    Tags tags;

    std::mutex series_lock;
    std::list<TimeSeriesEntry> series;
    std::list<TimePoint> time_points;

    std::string lineProtocol_nameAndTags();
    std::string lineProtocol_nameAndTags(Tags &tags);
    std::string lineProtocol_fields(Fields &fields);
};

}
