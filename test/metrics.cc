#include <doctest/doctest.h>
#include <cstring>
#include <string>
#include <iostream>
#include "qmedia/metrics.hh"

using namespace neo_media;

TEST_CASE("createSetAndVerify")
{
    Metrics metrics("", "", "", "");
    Metrics::MeasurementPtr measurement = metrics.createMeasurement(
        "jitter", {{"streamId", 1}, {"clientId", 10}});
    CHECK_EQ(measurement->tags.size(), 2);

    auto clock = std::chrono::system_clock::now();

    for (int i = 0; i < 100; i++)
    {
        measurement->set(clock, {"queue_depth", i});
        clock += std::chrono::milliseconds(10);
    }
    CHECK_EQ(measurement->time_series.size(), 100);

    int index = 0;
    for (auto entry : measurement->time_series)
    {
        for (auto field : entry.second)
        {
            CHECK_EQ("queue_depth", field.first);
            CHECK_EQ(field.second, index);
            ++index;
        }
    }
}

TEST_CASE("nameAndTags")
{
    Metrics metrics("", "", "", "");
    Metrics::MeasurementPtr measurement = metrics.createMeasurement(
        "jitter", {{"streamId", 1}, {"clientId", 10}, {"sw_version", 1456}});
    CHECK_FALSE(measurement->name.empty());
    CHECK_EQ(measurement->tags.size(), 3);
    CHECK_EQ("jitter,streamId=1,clientId=10,sw_version=1456",
             measurement->lineProtocol_nameAndTags());
}

TEST_CASE("lineProtocol-single-field")
{
    Metrics metrics("", "", "", "");
    Metrics::MeasurementPtr measurement = metrics.createMeasurement(
        "jitter", {{"streamId", 1}, {"clientId", 10}, {"sw_version", 1456}});

    auto clock = std::chrono::system_clock::now();

    std::list<std::chrono::system_clock::time_point> clocks;

    for (int i = 0; i < 10; i++)
    {
        clocks.push_back(clock);
        measurement->set(clock, {"queue_depth", i});
        clock += std::chrono::milliseconds(10);
    }

    auto lines = measurement->lineProtocol();
    int qd = 0;
    for (auto line : lines)
    {
        long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             clocks.front().time_since_epoch())
                             .count();
        std::string base_line = "jitter,streamId=1,clientId=10,sw_version=1456 "
                                "queue_depth=";
        std::string verify_line = base_line + std::to_string(qd) + " " +
                                  std::to_string(time) + "\n";
        CHECK_EQ(verify_line, line);
        ++qd;
        clocks.pop_front();
    }
}

TEST_CASE("lineProtocol-multi-fields")
{
    Metrics metrics("", "", "", "");
    Metrics::MeasurementPtr measurement = metrics.createMeasurement(
        "jitter", {{"streamId", 1}, {"clientId", 10}, {"sw_version", 1456}});

    auto clock = std::chrono::system_clock::now();
    std::list<std::chrono::system_clock::time_point> clocks;

    for (int i = 0; i < 10; i++)
    {
        clocks.push_back(clock);
        measurement->set(
            clock,
            {{"queue_depth", i}, {"jitter", i + 100}, {"total", i + 1000}});
        clock += std::chrono::milliseconds(10);
    }

    auto lines = measurement->lineProtocol();
    int qd = 0;
    for (auto line : lines)
    {
        long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             clocks.front().time_since_epoch())
                             .count();
        std::string base_line = "jitter,streamId=1,clientId=10,sw_version="
                                "1456 ";
        std::string q_string = "queue_depth=" + std::to_string(qd);
        std::string jit_string = "jitter=" + std::to_string(qd + 100);
        std::string total_string = "total=" + std::to_string(qd + 1000);
        std::string verify_line = base_line + q_string + "," + jit_string +
                                  "," + total_string + " " +
                                  std::to_string(time) + "\n";
        CHECK_EQ(verify_line, line);
        ++qd;
        clocks.pop_front();
    }
}

// not really a unit test - but nice to test against influx instance
#if 0        // don't want this for jenkins .. enable for local testing
TEST_CASE("salt-n-peppa-push-it")
{
    std::clog << "test start" << std::endl;
    Metrics metrics(MetricsConfig::URL,
                    MetricsConfig::ORG,
                    MetricsConfig::BUCKET,
                    MetricsConfig::AUTH_TOKEN);

    Metrics::MeasurementPtr measurement = metrics.createMeasurement("jitter", {
            { "streamId", 1},
            { "clientId" ,10},
            { "sw_version", 1456}});

    auto clock = std::chrono::system_clock::now();
    for (int i=0; i < 25; i++)
    {
        measurement->set(clock, {{"queue_depth", i}, { "jitter_val", i+100} , { "total", i+1000}});
        clock += std::chrono::milliseconds(10);
    }
    std::clog << "push start" << std::endl;
    metrics.push();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::clog << "push stop" << std::endl;
}
#endif
