
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

Metrics::MeasurementPtr Metrics::createMeasurement(std::string name,
                                                   Measurement::Tags tags)
{
    std::lock_guard<std::mutex> lock(metrics_mutex);
    auto frame = std::make_shared<Measurement>(name, tags);
    measurements.insert(std::pair<std::string, MeasurementPtr>(name, frame));
    return frame;
}


