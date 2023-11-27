#pragma once

#include <bytes/bytes.h>
#include <cantina/logger.h>

#include <map>
#include <mutex>
#include <set>

namespace counter {

using CounterID = mls::bytes_ns::bytes;
using Counter = uint64_t;
using LockID = mls::bytes_ns::bytes;
using Duration = std::chrono::milliseconds;
using TimePoint = std::chrono::system_clock::time_point;

struct Locked
{
  TimePoint expiry;
};

struct OutOfSync
{
  Counter next_value;
};

struct Unauthorized
{};

struct LockOK
{
  TimePoint expiry;
  LockID lock_id;
};

struct IncrementOK
{};

using LockResponse = std::variant<Locked, OutOfSync, LockOK>;
using IncrementResponse = std::variant<Unauthorized, IncrementOK>;

// A counter service tracks a collection of counters with unique identifiers.
// In an MLS context, the counter value is equal to the MLS epoch, and the
// counter identifier is the group identifier.
//
// The counter service definition below does not actually expose the value of
// the counter, except in OutOfSync.  The assumption is that the consumers of
// the counter are communicating updates to the counter outside this service
// (e.g., by sending Commits), and only using the counter service to synchronize
// updates.
//
// Before a counter can be incremented, it must be locked.  As part of the lock
// operation, the caller states what their expected next counter value, which
// much match the service's expectation in order for the caller to acquire the
// lock.  Since the actual updates to the counter are out of band, this ensures
// that the caller has the correct current value before incrementing.
//
// There is no explicit initialization of counters.  The first call to `lock`
// for a counter must have `next_value` set to 0.
//
// There is no method provided to clean up counters.  A service may clean up a
// counter if it has some out-of-band mechanism to find out that the counter is
// no longer needed.  For example, in an MLS context, once the MLS group is no
// longer in use, its counter can be discarded.
struct Service
{
  virtual ~Service() = default;

  // Acquire the lock for a counter.
  //
  // Responses:
  // * Locked: The counter is already locked
  // * OutOfSync: The indicated next_value is not correct
  // * LockOK: The caller now owns the lock
  virtual LockResponse lock(const CounterID& counter_id,
                            Counter next_value,
                            Duration duration) = 0;

  // Increment a locked counter.
  //
  // Responses:
  // * Unauthorized: The provided token is invalid for this counter
  // * IncrementOK: The counter has been incremented
  virtual IncrementResponse increment(const LockID& lock_id) = 0;
};

struct InMemoryService : Service
{
  InMemoryService(cantina::LoggerPointer logger_in);

  LockResponse lock(const CounterID& counter_id,
                    Counter next_value,
                    Duration duration) override;

  IncrementResponse increment(const LockID& lock_id) override;

private:
  cantina::LoggerPointer logger;

  std::lock_guard<std::mutex> lock();
  std::mutex mutex;

  struct Lock
  {
    TimePoint expiry;
    CounterID counter_id;
  };

  std::map<LockID, Lock> counter_locks;
  TimePoint clean_up_expired_locks();

  std::map<CounterID, Counter> counters;
};

} // namespace lock
