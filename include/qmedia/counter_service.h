/*
 *  counter_service.h
 *
 *  Copyright (C) 2023
 *  Cisco Systems, Inc.
 *  All Rights Reserved
 *
 *  Description:
 *      The file defines a Counter Service that is used to coordinate
 *      MLS epoch values among participants in a conference.  There are three
 *      primary classes as follows:
 *
 *      Service - a virtual base class that defines an interface for the
 *      service that may be used by objects that want a pointer to a service
 *      instance without having to know about the specific type of service.
 *
 *      InMemory - an in-memory counter service that is useful for local
 *      testing.
 *
 *      Redis - A service class that interfaces with a Redis server to provide
 *      the epoch counting service that facilitates communication among
 *      multiple conference entities.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <mls/common.h>
#include <sw/redis++/redis++.h>

namespace counter
{

// Define types associated with the Counter Service
using CounterID = std::vector<std::uint8_t>;
using Counter = mls::epoch_t;
using GroupID = std::uint64_t;

// Define an exception class for Counter
class CounterException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Define an enumeration type used to signal the result of certain functions
enum class Result
{
    Success,
    Failure,
    OutOfSync
};

// Define a service interface to allow for different counter service types
class Service
{
    public:
        Service() = default;
        virtual ~Service() = default;
        virtual Result UpdateEpoch(Counter next_epoch) = 0;
        virtual bool IsConnected() = 0;
};

// Define the in-memory version of the Counter Service
class InMemory : public Service
{
    public:
        InMemory() : Service(), epoch{0}
        {
            // Nothing more to do
        }
        virtual ~InMemory() = default;

        // Function to update the epoch value
        virtual Result UpdateEpoch(Counter next_epoch)
        {
            std::lock_guard<std::mutex> lock(counter_mutex);

            // Return an error if the expected value is incorrect
            if (epoch != next_epoch) return Result::OutOfSync;

            // Increment the epoch value
            ++epoch;

            return Result::Success;
        }

        // Since the in-memory service is local, it is always "connected"
        virtual bool IsConnected()
        {
            return true;
        }

    protected:
        Counter epoch;
        std::mutex counter_mutex;
};

// Define the counter service that interfaces with a remote Redis server
class Redis : public Service
{
    public:
        Redis(const GroupID group_id,
              const std::string& host,
              const std::uint16_t port,
              const std::string& user = "default",
              const std::string& password = "",
              const bool use_tls = true);
        virtual ~Redis() = default;

        Result UpdateEpoch(Counter next_epoch);

        bool IsConnected();

    protected:
        CounterID counter_id;
        std::string epoch_key_name;
        std::unique_ptr<sw::redis::Redis> redis;
};

} // namespace counter
