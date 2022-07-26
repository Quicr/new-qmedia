#include "full_Fill.hh"
#include <qmedia/logger.hh>
namespace qmedia
{

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

unsigned int fullFill::getTotalInBuffers(LoggerPointer logger)
{
    unsigned int totalLength = 0;
    for (const auto &buff : buffers)
    {
        totalLength += buff.first.size();
    }
    if (logger)
    {
        logger->debug << "getTotalInBuffers: totalLength: " << totalLength
                     << ", read_front " << read_front << std::flush;
    }
    return totalLength - read_front;
}

uint64_t fullFill::calculate_timestamp(LoggerPointer logger, unsigned int front,
                                       uint64_t timestamp) const
{
    logger->debug << "calculate_timestamp: front " << front << ", timestamp " << timestamp << std::endl;
    if (front == 0 || timestamp == 0) {
        return timestamp;
    }

    uint64_t samples = front / sample_divisor;
    uint64_t microseconds_passed = samples * 1000 / 48;        // assuming 48
                                                               // Khz sample
                                                               // rate
    return timestamp + microseconds_passed;
}

bool fullFill::fill(LoggerPointer logger, std::vector<uint8_t> &fill_buffer,
                    unsigned int fill_length,
                    uint64_t &timestamp)
{
    std::lock_guard<std::mutex> lock(buff_mutex);
    unsigned int total_length = getTotalInBuffers(logger);
    timestamp = UINT64_MAX;        // set timestamp from first filled sample -
                                   // using MAX to set only once

    logger->debug << "Fill: total_length: " << total_length
                 << ", fill_length " << fill_length
                 << ", num elems " << buffers.size() << std::flush;
    if (total_length >= fill_length)
    {
        while (fill_buffer.size() < fill_length)
        {
            auto &dat = buffers.front();
            unsigned int length = dat.first.size();
            unsigned int available_length = length - read_front;
            unsigned int to_fill = fill_length - fill_buffer.size();
            logger->debug << "Fill: to_fill: " << to_fill << std::flush;

            if (timestamp == UINT64_MAX)
            {
                timestamp = calculate_timestamp(logger, read_front, dat.second);
                logger->debug << "Fill: calculate_timestamp: " << timestamp << std::flush;

            }

            if (available_length == to_fill)
            {
                fill_buffer.insert(fill_buffer.end(),
                                   dat.first.begin() + read_front,
                                   dat.first.end());
                read_front = 0;
                buffers.pop_front();
                break;
            } else if (available_length < to_fill)
            {
                fill_buffer.insert(fill_buffer.end(),
                                   dat.first.begin() + read_front,
                                   dat.first.end());
                read_front = 0;
                buffers.pop_front();
            } else
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

    if (timestamp == UINT64_MAX) {
        logger->debug << "Fill: timestamp == UINT64_MAX" << std::flush;
        timestamp = 0;
    }

    logger->debug << "Fill: fill_buffer size " << fill_buffer.size() << std::flush;
    return (fill_buffer.size() == fill_length);
}

}        // namespace qmedia
