#pragma once

#include "channel.h"
#include "namespace_config.h"

#include <mls/messages.h>
#include <quicr/quicr_client.h>

#include <future>
#include <latch>

namespace delivery {

using mls::bytes_ns::bytes;

using UserID = uint32_t;
using JoinID = uint32_t;
using EpochID = uint64_t;

struct JoinRequest
{
  mls::KeyPackage key_package;

  TLS_SERIALIZABLE(key_package)
};

struct Welcome
{
  mls::Welcome welcome;

  TLS_SERIALIZABLE(welcome)
};

struct Commit
{
  mls::MLSMessage commit;

  TLS_SERIALIZABLE(commit)
};

struct LeaveRequest
{
  mls::MLSMessage proposal;
  TLS_SERIALIZABLE(proposal)
};

using Message = std::variant<JoinRequest, Welcome, Commit, LeaveRequest>;

struct Service
{
  Service(size_t capacity);

  virtual ~Service() = default;

  // Connect to the service
  virtual bool connect(bool as_creator) = 0;

  // Disconnect from the service
  virtual void disconnect() = 0;

  // Broadcast a Commit message to the group
  virtual void send(Message message) = 0;

  // Notify the transport that the client is now joined
  virtual void join_complete() = 0;

  // Read incoming messages
  channel::Receiver<Message> inbound_messages;

protected:
  channel::Sender<Message> make_sender()
  {
    return inbound_messages.make_sender();
  }
};

struct QuicrService : Service
{
  QuicrService(size_t queue_capacity,
               cantina::LoggerPointer logger_in,
               std::shared_ptr<quicr::Client> client,
               quicr::Namespace welcome_ns_in,
               quicr::Namespace group_ns_in,
               uint32_t endpoint_id);

  bool connect(bool as_creator) override;
  void disconnect() override;
  void send(Message message) override;
  void join_complete() override;

private:
  static const uint16_t default_ttl_ms = 1000;

  cantina::LoggerPointer logger;
  std::shared_ptr<quicr::Client> client;
  NamespaceConfig namespaces;

  bool subscribe(quicr::Namespace nspace);
  bool publish_intent(quicr::Namespace nspace);
  void deliver(Message&& message);

  struct SubDelegate;
  struct PubDelegate;
};

} // namespace delivery
