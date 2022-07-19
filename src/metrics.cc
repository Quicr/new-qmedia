
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

#include "metrics.hh"

using namespace qmedia;

const std::string MetricsConfig::URL = "";
const std::string MetricsConfig::ORG = "";
const std::string MetricsConfig::BUCKET = "";
const std::string MetricsConfig::AUTH_TOKEN = "";

Metrics::Metrics(const std::string &influx_url,
                 const std::string &org,
                 const std::string &bucket,
                 const std::string &auth_token)
{
    // don't run push thread or run curl if url not provided
    if (influx_url.empty()) return;

    // manipulate url properties for use with CURL
    std::string adjusted_url = influx_url + "/api/v2/write?org=" + org +
                               "&bucket=" + bucket + "&precision=ns";

    std::clog << "influx url:" << adjusted_url << std::endl;

    // initial curl
    curl_global_init(CURL_GLOBAL_ALL);

    // get a curl handle
    handle = curl_easy_init();

    // set the action to POST
    curl_easy_setopt(handle, CURLOPT_POST, 1L);

    // set the url
    curl_easy_setopt(handle, CURLOPT_URL, adjusted_url.c_str());

    assert(!auth_token.empty());

    curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    struct curl_slist *headerlist = NULL;
    std::string token_str = "Authorization: Token " + auth_token;
    headerlist = curl_slist_append(headerlist, token_str.c_str());

    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerlist);

    // verify certificate
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);

    // verify host
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    // do not allow unlimited redirects
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 50L);

    // enable TCP keep-alive probing
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);

    // Start the service thread
    metrics_thread = std::thread(&Metrics::pusher, this);
    metrics_thread.detach();
}

Metrics::~Metrics()
{
    if (!push_signals) push();

    shutdown = true;
    if (metrics_thread.joinable())
    {
        metrics_thread.join();
    }
    curl_global_cleanup();
}

void Metrics::pusher()
{
    while (!shutdown)
    {
        std::unique_lock<std::mutex> lk(push_mutex);
        cv.wait(lk, [&]() -> bool { return (shutdown || push_signals); });

        lk.unlock();
        if (!push_signals)
        {
            continue;
        }

        push_signals = false;
        bool print_error = true;
        for (const auto &mes : measurements)
        {
            auto mlines = mes.second->lineProtocol();
            if (mlines.empty())
            {
                continue;
            }
            std::string points;
            for (const auto &point : mlines) points += point;

            // std::clog << points << std::endl;

            curl_easy_setopt(
                handle, CURLOPT_POSTFIELDSIZE_LARGE, points.length());
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, points.c_str());
            CURLcode res = curl_easy_perform(handle);
            if (res != CURLE_OK && print_error)
            {
                std::clog << "Unable to post metrics:"
                          << curl_easy_strerror(res) << std::endl;
                print_error = false;
                continue;
            }
            long response_code;
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);

            if (response_code < 200 || response_code >= 300)
            {
                std::cerr << "Http error posting to influx: " << response_code
                          << std::endl;
                shutdown = true;
                break;
            }
            else
                mes.second->sent = true;
        }
    }
    std::cerr << "Metrics Thread is down" << std::endl;
}

void Metrics::push()
{
    push_signals = true;
    cv.notify_all();
}

Metrics::MeasurementPtr Metrics::createMeasurement(std::string name,
                                                   Measurement::Tags tags)
{
    std::lock_guard<std::mutex> lock(metrics_mutex);
    auto frame = std::make_shared<Measurement>(name, tags);
    measurements.insert(std::pair<std::string, MeasurementPtr>(name, frame));
    return frame;
}

Metrics::Measurement::Measurement(std::string &name, Measurement::Tags &tags) :
    fieldIndex(0)
{
    this->name = name;
    this->tags = tags;
}

Metrics::Measurement::~Measurement()
{
}

void Metrics::Measurement::set(std::chrono::system_clock::time_point now,
                               Field field)
{
    Fields fields;
    fields.emplace_back(field);
    set(now, fields);
}

void Metrics::Measurement::set_time_entry(
    std::chrono::system_clock::time_point now,
    TimeEntry &&entry)
{
    long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch())
                         .count();
    TimeSeriesEntry tse;
    tse.first = time;
    tse.second = entry;
    {
        std::lock_guard<std::mutex> lock(series_lock);
        series.emplace_back(tse);
    }
}

// we will probably need to set multiple fields in a single entry?
void Metrics::Measurement::set(std::chrono::system_clock::time_point now,
                               Fields fields)
{
    long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch())
                         .count();
    time_entry entry;
    entry.first = time;
    entry.second = fields;
    {
        std::lock_guard<std::mutex> lock(series_lock);
        time_series.emplace_back(entry);
    }
}

std::string Metrics::Measurement::lineProtocol_nameAndTags()
{
    return lineProtocol_nameAndTags(tags);
}

std::string Metrics::Measurement::lineProtocol_nameAndTags(Tags &tag_set)
{
    if (name.empty()) return "";

    std::string m = name;

    if (!tag_set.empty())
    {
        for (const auto &tag : tag_set)
        {
            m += ",";
            m += tag.first;
            m += "=";
            m += std::to_string(tag.second);
        }
    }
    return m;
}

std::string Metrics::Measurement::lineProtocol_fields(Fields &fields)
{
    bool first = true;
    std::string line = " ";
    for (auto &field : fields)
    {
        if (!first)
        {
            line += ",";
        }
        line += field.first;
        line += "=";
        line += std::to_string(field.second);
        first = false;
    }

    return line;
}

std::list<std::string> Metrics::Measurement::lineProtocol()
{
    std::list<std::string> lines;
    std::string name_tags = lineProtocol_nameAndTags(tags);

    {
        std::lock_guard<std::mutex> lock(series_lock);
        if (name_tags.empty() && series.empty()) return lines;

        for (auto entry : time_series)
        {
            std::string line = name_tags;
            line += lineProtocol_fields(entry.second);
            line += " ";
            line += std::to_string(entry.first);        // time
            line += "\n";
            lines.emplace_back(line);
        }

        time_series.clear();

        // add series generated via TimeSeriesEntries
        std::for_each(
            series.begin(),
            series.end(),
            [&lines, this](auto &entry)
            {
                // gen tags
                std::string line = lineProtocol_nameAndTags(entry.second.tags);
                if (line.empty()) return;
                line += lineProtocol_fields(entry.second.fields);
                line += " ";
                line += std::to_string(entry.first);        // time
                line += "\n";
                lines.emplace_back(line);
            });
        series.clear();

    }        // lock guard

    return lines;
}
