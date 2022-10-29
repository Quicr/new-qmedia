#include <algorithm>
#include <metrics/measurements.hh>

namespace metrics
{

std::shared_ptr<InfluxMeasurement> InfluxMeasurement::create(std::string name, Tags tags)
{
        return std::make_shared<InfluxMeasurement>(name, tags);
}

InfluxMeasurement::InfluxMeasurement(std::string &name_in, Tags &tags_in) :
    fieldIndex(0),
    name(name_in),
    tags(tags_in) {}


void InfluxMeasurement::set_time_entry(std::chrono::system_clock::time_point now,
    TimeEntry &&entry)
{
    long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    TimeSeriesEntry tse {time, entry};
    {
        std::lock_guard<std::mutex> lock(series_lock);
        series.emplace_back(tse);
    }
}

// we will probably need to set multiple fields in a single entry?
void InfluxMeasurement::set(std::chrono::system_clock::time_point now,
                               Fields fields)
{
    long long time = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    TimePoint entry {time, fields};
    {
        std::lock_guard<std::mutex> lock(series_lock);
        time_points.emplace_back(entry);
    }
}


std::string InfluxMeasurement::lineProtocol_nameAndTags()
{
    return lineProtocol_nameAndTags(tags);
}

std::string InfluxMeasurement::lineProtocol_nameAndTags(Tags &tag_set)
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

std::string InfluxMeasurement::lineProtocol_fields(Fields &fields)
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

std::list<std::string> InfluxMeasurement::lineProtocol()
{
    std::list<std::string> lines;
    std::string name_tags = lineProtocol_nameAndTags(tags);

    {
        std::lock_guard<std::mutex> lock(series_lock);
        if (name_tags.empty() && series.empty()) return lines;

        // add series generated via TimeSeriesEntries
        std::for_each(series.begin(), series.end(), [&lines, this](auto &entry)
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

std::string InfluxMeasurement::serialize() {
    auto entries = lineProtocol();
    std::string serialized = "";
    for(const auto& entry: entries) {
        serialized += entry;
    }
    return serialized;
}

}
