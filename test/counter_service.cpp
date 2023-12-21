/*
 *  counter_service.cpp
 *
 *  Copyright (C) 2023
 *  Cisco Systems, Inc.
 *  All Rights Reserved
 *
 *  Description:
 *      This module implements tests for the in-memory counter service.  The
 *      in-memory counter service would generally be employed when running
 *      build tests.
 *
 *  Portability Issues:
 *      None.
 */

#include <qmedia/counter_service.h>
#include <cstdint>
#include <string>
#include <vector>
#include <ostream>

std::ostream &operator<<(std::ostream &o, const counter::Result result);

#include <doctest/doctest.h>

std::ostream &operator<<(std::ostream &o, const counter::Result result)
{
    switch(result)
    {
        case counter::Result::Success:
            o << "Success";
            break;

        case counter::Result::Failure:
            o << "Failure";
            break;

        case counter::Result::OutOfSync:
            o << "OutOfSync";
            break;

        default:
            o << "Unknown";
            break;
    }

    return o;
}

namespace
{

class CounterService_ : public counter::InMemory
{
    using counter::InMemory::InMemory;
};

// The fixture for testing class CounterService
class CounterServiceTest
{
    public:
        CounterServiceTest() : counter_service()
        {
            // Nothing to do
        }
        ~CounterServiceTest() = default;

    protected:
        counter::InMemory counter_service;
};

TEST_CASE_FIXTURE(CounterServiceTest, "TestOutOfSync")
{
    // Verify the the lock fails with the wrong epoch value
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(1));
}

TEST_CASE_FIXTURE(CounterServiceTest, "TestUpdate")
{
    // Verify the update is successful
    REQUIRE(counter::Result::Success == counter_service.UpdateEpoch(0));
}

TEST_CASE_FIXTURE(CounterServiceTest, "TestUpdateMultiple")
{
    // Attempt to update the epoch multiple times
    for (counter::Counter i = 0; i < 5; i++)
    {
        // Verify the update request is successful
        REQUIRE(counter::Result::Success == counter_service.UpdateEpoch(i));
    }

    // Verify the expected OutOfSync errors for old epoch values
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(0));
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(1));
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(2));
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(3));
    REQUIRE(counter::Result::OutOfSync == counter_service.UpdateEpoch(4));

    // This update should succeed since it would be the next epoch value
    REQUIRE(counter::Result::Success == counter_service.UpdateEpoch(5));
}

} // namespace
