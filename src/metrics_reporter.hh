#pragma once
#include <qmedia/media_client.hh>
#include <metrics/measurements.hh>

struct MetricsReporter
{
    static void Report(qmedia::MediaStreamId streamId,
                       qmedia::MediaType media_type,
                       metrics::MeasurementType measurement_type,
                       const long long& val);

    static uint64_t client_id;
};
