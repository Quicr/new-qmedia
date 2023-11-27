#include "../include/qmedia/counter.h"

#include <random>
#include <tls/tls_syntax.h>

namespace counter {

static LockID
fresh_lock_id()
{
  auto dist = std::uniform_int_distribution<uint64_t>(
    std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  auto engine = std::random_device();
  return mls::tls::marshal(dist(engine));
}

InMemoryService::InMemoryService(cantina::LoggerPointer logger_in)
  : logger(
      std::make_shared<cantina::Logger>("LockSvc", std::move(logger_in), true))
{
}

LockResponse
InMemoryService::lock(const CounterID& counter_id,
                      Counter next_value,
                      Duration duration)
{
  const auto _ = lock();
  const auto now = clean_up_expired_locks();

  // Check that the proposed next value is correct
  const auto expected_next_value =
    counters.contains(counter_id) ? (counters.at(counter_id) + 1) : 0;
  if (next_value != expected_next_value) {
    logger->info << "Lock counter_id=" << counter_id
                 << " => OutOfSync actual=" << next_value
                 << " expected=" << expected_next_value << std::flush;
    return OutOfSync{ .next_value = expected_next_value };
  }

  // Check that the lock is not already locked
  const auto lock_it =
    std::find_if(counter_locks.begin(),
                 counter_locks.end(),
                 [&counter_id](const auto& pair) {
                   return pair.second.counter_id == counter_id;
                 });
  if (lock_it != counter_locks.end()) {
    logger->info << "Lock counter_id=" << counter_id << " => Locked"
                 << std::flush;
    return Locked{ .expiry = lock_it->second.expiry };
  }

  // Mark the lock as acquired
  const auto lock_id = fresh_lock_id();
  const auto lock = Lock{
    .expiry = now + duration,
    .counter_id = counter_id,
  };
  counter_locks.insert_or_assign(lock_id, lock);
  logger->info << "Lock counter_id=" << counter_id
               << " => LockOK lock_id=" << lock_id << std::flush;
  return LockOK{ .expiry = lock.expiry, .lock_id = lock_id };
}

IncrementResponse
InMemoryService::increment(const LockID& lock_id)
{
  const auto _ = lock();
  clean_up_expired_locks();

  // Check that the lock has been acquired and the token is valid
  if (!counter_locks.contains(lock_id)) {
    logger->info << "Increment lock_id=" << lock_id
                 << " => Unauthorized (not locked)" << std::flush;
    return Unauthorized{};
  }

  // Increment the counter and clear the lock
  const auto counter_id = counter_locks.at(lock_id).counter_id;
  if (!counters.contains(counter_id)) {
    counters.insert_or_assign(counter_id, 0);
  } else {
    counters.at(counter_id) += 1;
  }

  counter_locks.erase(lock_id);
  logger->info << "Increment lock_id=" << lock_id
               << " counter_id=" << counter_id
               << " => OK counter=" << counters.at(counter_id) << std::flush;
  return IncrementOK{};
}

std::lock_guard<std::mutex>
InMemoryService::lock()
{
  return std::lock_guard(mutex);
}

TimePoint
InMemoryService::clean_up_expired_locks()
{
  const auto now = std::chrono::system_clock::now();
  std::erase_if(counter_locks,
                [&](const auto& pair) { return pair.second.expiry < now; });
  return now;
}

} // namespace lock
