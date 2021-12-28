#include "full_Fill.hh"

using namespace neo_media;

fullFill::fullFill() : read_front(0)
{
}

void fullFill::addBuffer(const uint8_t *buffer,
                         unsigned int length,
                         std::uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(buff_mutex);
    std::vector<uint8_t> data;
    data.assign(buffer, buffer + length);
    buffers.push_back(std::pair(data, timestamp));
}

unsigned int fullFill::getTotalInBuffers()
{
    unsigned int totalLength = 0;
    for (const auto &buff : buffers)
    {
        totalLength += buff.first.size();
    }
    return totalLength - read_front;
}

uint64_t fullFill::calculate_timestamp(unsigned int front,
                                       uint64_t timestamp) const
{
    if (front == 0 || timestamp == 0) return timestamp;

    uint64_t samples = front / sample_divisor;
    uint64_t microseconds_passed = samples * 1000 / 48;        // assuming 48
                                                               // Khz sample
                                                               // rate
    return timestamp + microseconds_passed;
}

bool fullFill::fill(std::vector<uint8_t> &fill_buffer,
                    unsigned int fill_length,
                    uint64_t &timestamp)
{
    std::lock_guard<std::mutex> lock(buff_mutex);
    unsigned int total_length = getTotalInBuffers();
    timestamp = UINT64_MAX;        // set timestamp from first filled sample -
                                   // using MAX to set only once

    if (total_length >= fill_length)
    {
        while (fill_buffer.size() < fill_length)
        {
            auto &dat = buffers.front();
            unsigned int length = dat.first.size();
            unsigned int available_length = length - read_front;
            unsigned int to_fill = fill_length - fill_buffer.size();

            if (timestamp == UINT64_MAX)
                timestamp = calculate_timestamp(read_front, dat.second);

            if (available_length == to_fill)
            {
                fill_buffer.insert(fill_buffer.end(),
                                   dat.first.begin() + read_front,
                                   dat.first.end());
                read_front = 0;
                buffers.pop_front();
                break;
            }
            else if (available_length < to_fill)
            {
                fill_buffer.insert(fill_buffer.end(),
                                   dat.first.begin() + read_front,
                                   dat.first.end());
                read_front = 0;
                buffers.pop_front();
            }
            else
            {
                // current buffer has more data than to_fill, copy until to_fill
                fill_buffer.insert(fill_buffer.end(),
                                   dat.first.begin(),
                                   dat.first.begin() + to_fill);
                read_front = to_fill;
                break;
            }
        }
    }

    if (timestamp == UINT64_MAX) timestamp = 0;

    return (fill_buffer.size() == fill_length);
}
