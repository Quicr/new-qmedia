#include "../include/qmedia/mls_client.h"

#include <transport/transport.h>

using namespace mls;
using namespace std::chrono_literals;

static const size_t epochs_capacity = 100;
static const auto create_lock_duration = 500ms;
static const auto commit_lock_duration = 500ms;

MLSClient::MLSClient(const Config& config)
  : logger(config.logger)
  , group_id(config.group_id)
  , endpoint_id(config.endpoint_id)
  , counter_service(config.counter_service)
  , delivery_service(config.delivery_service)
  , mls_session(MLSInitInfo{ suite, endpoint_id })
  , epochs(epochs_capacity)
{
}

MLSClient::~MLSClient()
{
  disconnect();
}

counter::CounterID
make_counter_id(uint64_t group_id)
{
  return tls::marshal(group_id);
}

bool
MLSClient::maybe_create_session()
{
  // Attempt to acquire the lock for this group at epoch 0
  const auto counter_id = make_counter_id(group_id);
  const auto lock_resp =
    counter_service->lock(counter_id, 0, create_lock_duration);

  // If the counter has already advanced, someone else has already created the
  // group.
  if (std::holds_alternative<counter::OutOfSync>(lock_resp)) {
    return false;
  }

  // If the counter is locked, someone else is in the process of creating the
  // group, and we should retry once the lock releases.
  if (std::holds_alternative<counter::Locked>(lock_resp)) {
    const auto& locked = std::get<counter::Locked>(lock_resp);
    std::this_thread::sleep_until(locked.expiry);
    return maybe_create_session();
  }

  // Otherwise, the response should be OK
  if (!std::holds_alternative<counter::LockOK>(lock_resp)) {
    // Unspecified failure
    return false;
  }

  // Otherwise, create the group and report that the group has been created
  const auto init_info = std::get<MLSInitInfo>(mls_session);
  const auto session = MLSSession::create(init_info, group_id);

  // Destroy the epoch 0 lock to signal that the group has been created
  const auto& lock_id = std::get<counter::LockOK>(lock_resp).lock_id;
  const auto increment_resp = counter_service->increment(lock_id);
  if (!std::holds_alternative<counter::IncrementOK>(increment_resp)) {
    // Unspecified failure
    return false;
  }

  // Now that everything has gone well, install the session locally
  mls_session = session;
  return true;
}

bool
MLSClient::connect()
{
  // Determine whether to create the group
  auto as_creator = maybe_create_session();

  // Connect to the delivery service
  if (!delivery_service->connect(as_creator)) {
    return false;
  }

  // Start up a thread to handle incoming messages
  handler_thread = std::thread([&]() {
    while (!stop_threads) {
      auto maybe_msg =
        delivery_service->inbound_messages.receive(inbound_timeout);
      if (!maybe_msg) {
        continue;
      }

      const auto _ = lock();
      auto& msg = maybe_msg.value();
      handle_message(std::move(msg));
    }

    logger->Log("Handler thread stopping");
  });

  // Start up a thread to commit requests from other clients
  commit_thread = std::thread([&]() {
    while (!stop_threads) {
      std::this_thread::sleep_for(commit_interval);

      const auto _ = lock();
      make_commit();
    }
  });

  return true;
}

void
MLSClient::disconnect()
{
  logger->Log("Disconnecting delivery service");
  delivery_service->disconnect();

  stop_threads = true;

  if (handler_thread && handler_thread.value().joinable()) {
    logger->Log("Stopping handler thread");
    handler_thread.value().join();
    logger->Log("Handler thread stopped");
  }

  logger->Log("Stopping commit thread");
  if (commit_thread && commit_thread.value().joinable()) {
    commit_thread.value().join();
    logger->Log("Commit thread stopped");
  }
}

std::future<bool>
MLSClient::join()
{
  const auto _ = lock();
  join_promise = std::promise<bool>();

  const auto& kp = std::get<MLSInitInfo>(mls_session).key_package;
  delivery_service->send(delivery::JoinRequest{ kp });

  return join_promise->get_future();
}

void
MLSClient::leave()
{
  auto self_remove = std::get<MLSSession>(mls_session).leave();
  delivery_service->send(delivery::LeaveRequest{ self_remove });

  // XXX(richbarn) It is important to disconnect here, before the Commit shows
  // up removing this client.  If we receive that Commit, we will crash with
  // "Invalid proposal list" becase we are trying to handle a Commit that
  // removes us.
  disconnect();
}

bool
MLSClient::joined() const
{
  return std::holds_alternative<MLSSession>(mls_session);
}

const MLSSession&
MLSClient::session() const
{
  return std::get<MLSSession>(mls_session);
}

bool
operator==(const MLSClient::Epoch& lhs, const MLSClient::Epoch& rhs)
{
  return lhs.epoch == rhs.epoch && lhs.member_count == rhs.member_count &&
         lhs.epoch_authenticator == rhs.epoch_authenticator;
}

MLSClient::Epoch
MLSClient::next_epoch()
{
  return epochs.receive().value();
}

MLSClient::Epoch
MLSClient::latest_epoch()
{
  auto epoch = epochs.receive().value();
  while (!epochs.is_empty()) {
    epoch = epochs.receive().value();
  }
  return epoch;
}

bool
MLSClient::current(const delivery::Message& message)
{
  if (!joined()) {
    return true;
  }

  auto& session = std::get<MLSSession>(mls_session);
  const auto is_current = mls::overloaded{
    [&](const delivery::Commit& commit) {
      return session.current(commit.commit);
    },
    [&](const delivery::LeaveRequest& leave) {
      return session.current(leave.proposal);
    },
    [](const auto& /* unused */) { return false; },
  };

  return var::visit(is_current, message);
}

void
MLSClient::handle(delivery::JoinRequest&& join_request)
{
  logger->Log("Received JoinRequest");
  auto parsed = MLSSession::parse_join(std::move(join_request));
  joins_to_commit.push_back(defer(std::move(parsed)));
  return;
}

void
MLSClient::handle(delivery::Welcome&& welcome)
{
  logger->Log("Received Welcome");

  if (joined()) {
    logger->Log("Ignoring Welcome; already joined to the group");
    return;
  }

  const auto& init_info = std::get<MLSInitInfo>(mls_session);
  if (!welcome.welcome.find(init_info.key_package)) {
    logger->Log("Ignoring Welcome; not for me");
    return;
  }

  // Join the group
  mls_session = MLSSession::join(init_info, welcome.welcome);
  if (join_promise) {
    join_promise->set_value(true);
  }

  auto& session = std::get<MLSSession>(mls_session);
  epochs.send(
    { session.epoch(), session.member_count(), session.epoch_authenticator() });

  delivery_service->join_complete();

  // Request an empty commit to populate my path in the tree
  self_update_to_commit = true;
}

void
MLSClient::handle(delivery::Commit&& commit)
{
  logger->Log("Received Commit");

  if (!joined()) {
    logger->Log("Ignoring Commit; not joined to the group");
    return;
  }

  advance(commit.commit);
}

void
MLSClient::handle(delivery::LeaveRequest&& leave_request)
{
  logger->Log("Received LeaveRequest");

  if (!joined()) {
    logger->Log("Ignoring leave request; not joined to the group");
    return;
  }

  auto& session = std::get<MLSSession>(mls_session);
  auto maybe_parsed = session.parse_leave(std::move(leave_request));
  if (!maybe_parsed) {
    logger->Log("Ignoring leave request; unable to process");
    return;
  }

  auto& parsed = maybe_parsed.value();
  leaves_to_commit.push_back(defer(std::move(parsed)));
}

void
MLSClient::handle_message(delivery::Message&& msg)
{
  // Any MLSMesssage-formatted messages that are for a future epoch get
  // enqueued for later processing.
  const auto* maybe_session = std::get_if<MLSSession>(&mls_session);
  const auto is_future = mls::overloaded{
    [&](const delivery::Commit& commit) {
      return !maybe_session || maybe_session->future(commit.commit);
    },
    [&](const delivery::LeaveRequest& leave) {
      return !maybe_session || maybe_session->future(leave.proposal);
    },
    [](const auto& /* other */) { return false; },
  };

  if (var::visit(is_future, msg)) {
    future_epoch_messages.push_back(std::move(msg));
    return;
  }

  // Handle messages according to type
  auto handle_dispatch = [this](auto&& msg) { handle(std::move(msg)); };
  var::visit(handle_dispatch, std::move(msg));
}

MLSClient::TimePoint
MLSClient::not_before(uint32_t distance)
{
  auto now = std::chrono::system_clock::now();
  return now + distance * commit_delay_unit;
}

MLSClient::Deferred<ParsedJoinRequest>
MLSClient::defer(ParsedJoinRequest&& join)
{
  auto distance = uint32_t(0);
  if (joined()) {
    distance = session().distance_from(joins_to_commit.size(), {});
  }
  return { not_before(distance), std::move(join) };
}

MLSClient::Deferred<ParsedLeaveRequest>
MLSClient::defer(ParsedLeaveRequest&& leave)
{
  auto distance = uint32_t(0);
  if (joined()) {
    distance = session().distance_from(0, { leave });
  }
  return { not_before(distance), std::move(leave) };
}

void
MLSClient::make_commit()
{
  // Can't commit if we're not joined
  if (!joined()) {
    return;
  }

  auto& session = std::get<MLSSession>(mls_session);

  // Import the requests
  groom_request_queues();

  // Select the requests for which a commit is timely
  const auto self_update = self_update_to_commit.load();

  const auto now = std::chrono::system_clock::now();
  auto joins = std::vector<ParsedJoinRequest>{};
  for (const auto& deferred : joins_to_commit) {
    if (deferred.not_before < now) {
      joins.push_back(deferred.request);
    }
  }

  auto leaves = std::vector<ParsedLeaveRequest>{};
  for (const auto& deferred : leaves_to_commit) {
    if (deferred.not_before < now) {
      leaves.push_back(deferred.request);
    }
  }

  // Abort if nothing to commit
  if (!self_update && joins.empty() && leaves.empty()) {
    logger->Log("Not committing; nothing to commit");
    return;
  }

  // Construct the commit
  logger->info << "Committing Join=[";
  for (const auto& join : joins) {
    logger->info << join.endpoint_id << ",";
  }
  logger->info << "] SelfUpdate=" << (self_update ? "Y" : "N") << " Leave=[";
  for (const auto& leave : leaves) {
    logger->info << leave.endpoint_id << ",";
  }
  logger->info << "]" << std::flush;
  auto [commit, welcome] = session.commit(self_update, joins, leaves);

  // Get permission to send a commit
  const auto next_epoch = session.epoch() + 1;
  const auto counter_id = make_counter_id(group_id);
  const auto lock_resp =
    counter_service->lock(counter_id, next_epoch, commit_lock_duration);

  if (std::holds_alternative<counter::OutOfSync>(lock_resp)) {
    logger->info << "Failed to commit: MLS state is behind" << std::flush;
  }

  if (std::holds_alternative<counter::Locked>(lock_resp)) {
    logger->info << "Failed to commit: Conflict" << std::flush;
    return;
  }

  // Otherwise, the response should be OK
  if (!std::holds_alternative<counter::LockOK>(lock_resp)) {
    logger->info << "Failed to commit: Failed to acquire lock" << std::flush;
    return;
  }

  // Publish the commit
  delivery_service->send(delivery::Commit{ commit });

  // Report that the commit has been sent
  const auto& lock_id = std::get<counter::LockOK>(lock_resp).lock_id;
  const auto increment_resp = counter_service->increment(lock_id);
  if (!std::holds_alternative<counter::IncrementOK>(increment_resp)) {
    logger->info << "Failed to commit: Failed to destroy lock" << std::flush;
    return;
  }

  // Publish the Welcome and update our own state now that everything is OK
  advance(commit);
  self_update_to_commit = false;

  // Note: While it may seem harmless to send a Welcome if there are no joins,
  // omitting this `if` guard causes one of the tests to hang.
  if (!joins.empty()) {
    delivery_service->send(delivery::Welcome{ welcome });
  }
}

void
MLSClient::advance(const mls::MLSMessage& commit)
{
  logger->Log("Attempting to advance the MLS state...");

  // Apply the commit
  auto& session = std::get<MLSSession>(mls_session);
  switch (session.handle(commit)) {
    case MLSSession::HandleResult::ok:
      epochs.send({ session.epoch(),
                    session.member_count(),
                    session.epoch_authenticator() });
      logger->Log("Updated to epoch " + std::to_string(session.epoch()));
      break;

    case MLSSession::HandleResult::fail:
      logger->Log("Failed to advance; unspecified failure");
      break;

    case MLSSession::HandleResult::removes_me:
      logger->Log("Failed to advance; MLS commit would remove me");
      break;

    default:
      logger->Log("Failed to advance; reason unknown");
      return;
  }

  // Groom the request queues, removing any requests that are obsolete
  groom_request_queues();

  // Handle any out-of-order messages that have been enqueued
  for (auto& msg : future_epoch_messages) {
    if (!current(msg)) {
      continue;
    }

    // Copy here so that erase_if works properly below
    auto msg_copy = msg;
    handle_message(std::move(msg_copy));
  }

  std::erase_if(future_epoch_messages,
                [&](const auto& msg) { return current(msg); });
}

void
MLSClient::groom_request_queues()
{
  const auto& session = std::get<MLSSession>(mls_session);
  const auto obsolete = [session](const auto& deferred) {
    return session.obsolete(deferred.request);
  };

  std::erase_if(joins_to_commit, obsolete);
  std::erase_if(leaves_to_commit, obsolete);
}
