#include <iostream>
#include "metrics_reporter.hh"
#include <metrics/metrics.hh>
#include <metrics/measurements.hh>
#include "media_stream.hh"

using namespace metrics;

uint64_t MetricsReporter::client_id = {0};

//template <typename T>
void MetricsReporter::Report(qmedia::MediaStreamId stream_id,
                             qmedia::MediaType media_type,
                             metrics::MeasurementType measurement_type,
                             const long long& val)
{
    auto metrics = MetricsFactory::GetInfluxProvider();
    auto msmt = InfluxMeasurement::create(MeasurementNames.at(measurement_type), {});
    auto entry = InfluxMeasurement::TimeEntry{};
    entry.tags = {{"clientID", client_id},
                   {"sourceID", stream_id},
                   {"mediaType", media_type}};

    // common fields
    entry.fields = {{"count", 1}};
    switch (measurement_type)
    {
        case MeasurementType::EncodeTime:
            entry.fields.push_back(std::make_pair("duration", val));
            break;
        default:
            break;

    }

    msmt->set_time_entry(std::chrono::system_clock::now(), std::move(entry));
    metrics->add_measurement(MeasurementNames.at(MeasurementType::EncodeTime), msmt);
}

