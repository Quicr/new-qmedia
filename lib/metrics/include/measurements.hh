#pragma once
#include <map>
#include <string>
#include <list>
#include <mutex>

namespace metrics {

enum struct MeasurementType
{
    PacketRate_Tx,
    PacketRate_Rx,
    FrameRate_Tx,
    FrameRate_Rx,
    QDepth_Tx,
    QDepth_Rx,
};

const auto measurement_names = std::map<MeasurementType, std::string>{
    {MeasurementType::PacketRate_Tx, "TxPacketCount"},
    {MeasurementType::PacketRate_Rx, "RxPacketCount"},
    {MeasurementType::FrameRate_Tx, "TxFrameCount"},
    {MeasurementType::FrameRate_Rx, "RxFrameCount"},
    {MeasurementType::QDepth_Tx, "TxQueueDepth"},
    {MeasurementType::QDepth_Rx, "RxQueueDepth"},
};

///
/// Measurement
///
///

struct Measurement {
    virtual std::string toString() = 0;
    virtual ~Measurement() = default;
};

/// Influx Measurement and Helpers
///

// handy defines
using Field =  std::pair<std::string, uint64_t>;
using Fields =  std::list<Field>;
using Tag = std::pair<std::string, uint64_t>;
using Tags = std::list<Tag>;
using TimePoint = std::pair<long long, Fields>;

class InfluxMeasurement : public  Measurement
{
public:

    static std::unique_ptr<InfluxMeasurement> createMeasurement(std::string name, Tags tags);

    InfluxMeasurement(std::string &name, Tags &tags);
    ~InfluxMeasurement()  = default;

    // Measurement - to line protocol
    std::string toString() override;

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
