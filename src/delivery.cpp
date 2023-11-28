#include "qmedia/delivery.h"
#include "null_delegate.h"

#include <quicr/quicr_client_delegate.h>

namespace tls = mls::tls;

namespace delivery {

Service::Service(size_t capacity)
  : inbound_messages(capacity)
{
}

// Encoding / decoding for Quicr transport
enum struct MessageType : uint8_t
{
  invalid = 0,
  join_request = 1,
  welcome = 2,
  commit = 3,
  leave_request = 4,
};

struct QuicrMessage
{
  Message message;

  TLS_SERIALIZABLE(message)
  TLS_TRAITS(tls::variant<MessageType>)
};

// QuicrService::SubDelegate
struct QuicrService::SubDelegate : NullSubscribeDelegate
{
public:
  SubDelegate(cantina::LoggerPointer logger_in,
              channel::Sender<Message> queue_in)
    : logger(logger_in)
    , queue(queue_in)
  {
  }

  bool await_response() const
  {
    response_latch.wait();
    return successfully_connected;
  }

  void onSubscribeResponse(const quicr::Namespace& quicr_namespace,
                           const quicr::SubscribeResult& result) override
  {
    logger->info << "onSubscriptionResponse: ns: " << quicr_namespace
                 << " status: " << static_cast<int>(result.status)
                 << std::flush;

    successfully_connected =
      result.status == quicr::SubscribeResult::SubscribeStatus::Ok;
    response_latch.count_down();
  }

  void onSubscribedObject(const quicr::Name& quicr_name,
                          uint8_t /* priority */,
                          uint16_t /* expiry_age_ms */,
                          bool /* use_reliable_transport */,
                          quicr::bytes&& data) override
  {
    logger->info << "recv object: name: " << quicr_name
                 << " data sz: " << data.size();

    if (!data.empty()) {
      logger->info << " data: " << data.data();
    } else {
      logger->info << " (no data)";
    }

    logger->info << std::flush;

    auto decoded = tls::get<QuicrMessage>(data).message;
    queue.send(std::move(decoded));
  }

private:
  cantina::LoggerPointer logger;
  channel::Sender<Message> queue;
  std::latch response_latch{ 1 };
  std::atomic_bool successfully_connected = false;
};

// QuicrService::PubDelegate
struct QuicrService::PubDelegate : quicr::PublisherDelegate
{
public:
  PubDelegate(cantina::LoggerPointer logger_in)
    : logger(std::move(logger_in))
  {
  }

  bool await_response() const
  {
    response_latch.wait();
    return successfully_connected;
  }

  void onPublishIntentResponse(
    const quicr::Namespace& quicr_namespace,
    const quicr::PublishIntentResult& result) override
  {
    std::stringstream log_msg;
    log_msg << "onPublishIntentResponse: name: " << quicr_namespace
            << " status: " << static_cast<int>(result.status);

    logger->Log(log_msg.str());

    successfully_connected = result.status == quicr::messages::Response::Ok;
    response_latch.count_down();
  }

private:
  cantina::LoggerPointer logger;
  std::latch response_latch{ 1 };
  std::atomic_bool successfully_connected = false;
};

// QuicrService
QuicrService::QuicrService(size_t queue_capacity,
                           cantina::LoggerPointer logger_in,
                           std::shared_ptr<quicr::Client> client_in,
                           quicr::Namespace welcome_ns_in,
                           quicr::Namespace group_ns_in,
                           uint32_t endpoint_id_in)
  : Service(queue_capacity)
  , logger(logger_in)
  , client(std::move(client_in))
  , namespaces(welcome_ns_in, group_ns_in, endpoint_id_in)
{
}

bool
QuicrService::connect(bool as_creator)
{
  // XXX(richbarn) These subscriptions / publishes are done serially; we await a
  // response for each one before doing the next.  They could be done in
  // parallel by having subscribe/publish_intent return std::future<bool> and
  // awaiting all of these futures together.
  return client->connect() &&
         (as_creator || subscribe(namespaces.welcome_sub())) &&
         subscribe(namespaces.group_sub()) &&
         publish_intent(namespaces.welcome_pub()) &&
         publish_intent(namespaces.group_pub());
}

void
QuicrService::disconnect()
{
  client->disconnect();
}

void
QuicrService::send(Message message)
{
  const auto is_welcome = std::holds_alternative<Welcome>(message);
  const auto name =
    (is_welcome) ? namespaces.for_welcome() : namespaces.for_group();
  auto data = tls::marshal(QuicrMessage{ message });

  logger->Log("Publish, name=" + std::string(name) +
              " size=" + std::to_string(data.size()));
  client->publishNamedObject(name, 0, default_ttl_ms, false, std::move(data));
}

void
QuicrService::join_complete()
{
  client->unsubscribe(
    namespaces.welcome_sub(), "bogus_origin_url", "bogus_auth_token");
}

bool
QuicrService::subscribe(quicr::Namespace ns)
{
  const auto delegate = std::make_shared<SubDelegate>(logger, make_sender());

  logger->Log("Subscribe to " + std::string(ns));
  quicr::bytes empty;
  client->subscribe(delegate,
                    ns,
                    quicr::SubscribeIntent::immediate,
                    "bogus_origin_url",
                    false,
                    "bogus_auth_token",
                    std::move(empty));

  return delegate->await_response();
}

bool
QuicrService::publish_intent(quicr::Namespace ns)
{
  logger->Log("Publish Intent for namespace: " + std::string(ns));
  const auto delegate = std::make_shared<PubDelegate>(logger);
  client->publishIntent(delegate, ns, {}, {}, {});
  return delegate->await_response();
}

} // namespace delivery

namespace mls::tls {
TLS_VARIANT_MAP(delivery::MessageType, delivery::JoinRequest, join_request)
TLS_VARIANT_MAP(delivery::MessageType, delivery::Welcome, welcome)
TLS_VARIANT_MAP(delivery::MessageType, delivery::Commit, commit)
TLS_VARIANT_MAP(delivery::MessageType, delivery::LeaveRequest, leave_request)
} // namespace mls::tls
