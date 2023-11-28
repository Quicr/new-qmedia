#pragma once

#include "channel.h"
#include "counter.h"
#include "delivery.h"
#include "mls_session.h"

#include <cantina/logger.h>
#include <quicr/quicr_client.h>
#include <quicr/quicr_common.h>

#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <thread>

class MLSClient
{
public:
  struct Config
  {
    uint64_t group_id;
    uint32_t endpoint_id;
    cantina::LoggerPointer logger;
    std::shared_ptr<counter::Service> counter_service;
    std::shared_ptr<delivery::Service> delivery_service;
  };

  explicit MLSClient(const Config& config);
  ~MLSClient();

  // Connect to the server and make subscriptions
  bool connect();
  void disconnect();

  // MLS operations
  std::future<bool> join();
  void leave();

  // Access internal state
  bool joined() const;
  const MLSSession& session() const;

  // Access to MLS epochs as they arrive.  This method pops a queue of epochs;
  // if there are no epoch enqueued, it will block until one shows up.
  struct Epoch
  {
    uint64_t epoch;
    size_t member_count;
    bytes epoch_authenticator;

    friend bool operator==(const Epoch& lhs, const Epoch& rhs);
  };
  Epoch next_epoch();
  Epoch latest_epoch();

private:
  // Logging
  cantina::LoggerPointer logger;

  // Pub/Sub operations
  uint64_t group_id;
  uint32_t endpoint_id;

  // MLS operations
  const mls::CipherSuite suite{
    mls::CipherSuite::ID::P256_AES128GCM_SHA256_P256
  };

  std::shared_ptr<counter::Service> counter_service;
  std::shared_ptr<delivery::Service> delivery_service;

  std::optional<std::promise<bool>> join_promise;
  std::variant<MLSInitInfo, MLSSession> mls_session;

  channel::Channel<Epoch> epochs;

  bool maybe_create_session();

  // One lock for the whole object; one stop signal for all threads
  std::recursive_mutex self_mutex;
  std::unique_lock<std::recursive_mutex> lock()
  {
    return std::unique_lock{ self_mutex };
  }

  std::atomic_bool stop_threads = false;

  // Handler thread, including out-of-order message handling
  static constexpr auto inbound_timeout = std::chrono::milliseconds(100);
  std::vector<delivery::Message> future_epoch_messages;
  std::optional<std::thread> handler_thread;

  bool current(const delivery::Message& message);

  void handle(delivery::JoinRequest&& join_request);
  void handle(delivery::Welcome&& welcome);
  void handle(delivery::Commit&& commit);
  void handle(delivery::LeaveRequest&& leave_request);
  void handle_message(delivery::Message&& obj);
  void advance(const mls::MLSMessage& commit);
  void groom_request_queues();

  // Commit thread
  static constexpr auto commit_interval = std::chrono::milliseconds(100);
  static constexpr auto commit_delay_unit = std::chrono::milliseconds(75);

  using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

  template<typename T>
  struct Deferred
  {
    TimePoint not_before;
    T request;
  };

  static TimePoint not_before(uint32_t distance);
  Deferred<ParsedJoinRequest> defer(ParsedJoinRequest&& join);
  Deferred<ParsedLeaveRequest> defer(ParsedLeaveRequest&& leave);

  std::vector<Deferred<ParsedJoinRequest>> joins_to_commit;
  std::vector<Deferred<ParsedLeaveRequest>> leaves_to_commit;
  std::atomic_bool self_update_to_commit;

  std::optional<std::thread> commit_thread;
  void make_commit();
};
