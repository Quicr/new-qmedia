#include <doctest/doctest.h>

#include "relay.h"

#include <quicr/quicr_client.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <future>
#include <chrono>

class LocalhostServerDelegate : public quicr::ServerDelegate
{
public:
    LocalhostServerDelegate(std::shared_ptr<spdlog::logger> logger_in) : server(nullptr), logger(std::move(logger_in))
    {
    }

    void set_server(std::shared_ptr<quicr::Server> server_in) { server = std::move(server_in); }

    void onPublishIntent(const quicr::Namespace& quicr_namespace,
                         const std::string& /* origin_url */,
                         const std::string& /* auth_token */,
                         quicr::bytes&& /* e2e_token */) override
    {
        SPDLOG_LOGGER_INFO(logger, "PublishIntent namespace={0}", std::string(quicr_namespace));

        quicr::PublishIntentResult result{quicr::messages::Response::Ok, {}, {}};
        server->publishIntentResponse(quicr_namespace, result);
    };

    void onPublishIntentEnd(const quicr::Namespace& /* quicr_namespace */,
                            const std::string& /* auth_token */,
                            quicr::bytes&& /* e2e_token */) override
    {
    }

    void onSubscribe(const quicr::Namespace& quicr_namespace,
                     const uint64_t& subscriber_id,
                     const qtransport::TransportConnId& conn_id,
                     const qtransport::DataContextId& /* data_ctx_id */,
                     const quicr::SubscribeIntent /* subscribe_intent */,
                     const std::string& /* origin_url */,
                     const std::string& /* auth_token */,
                     quicr::bytes&& /* data */) override
    {
        SPDLOG_LOGGER_INFO(logger, "Subscribe namespace={0} subscriber_id={1}", std::string(quicr_namespace), subscriber_id);

        subscriptions.try_emplace(quicr_namespace);
        subscriptions.at(quicr_namespace).push_back({subscriber_id, conn_id});

        const auto status = quicr::SubscribeResult::SubscribeStatus::Ok;
        const auto result = quicr::SubscribeResult{status, "", {}, {}};
        server->subscribeResponse(subscriber_id, quicr_namespace, result);
    }

    void onUnsubscribe(const quicr::Namespace& quicr_namespace,
                       const uint64_t& subscriber_id,
                       const std::string& /* auth_token */) override
    {
        SPDLOG_LOGGER_INFO(logger, "Unsubscribe namespace={0} subscriber_id={1}", std::string(quicr_namespace), subscriber_id);

        const auto status = quicr::SubscribeResult::SubscribeStatus::Ok;
        server->subscriptionEnded(subscriber_id, quicr_namespace, status);

        auto& subs = subscriptions.at(quicr_namespace);
        auto new_end = std::remove_if(
            subs.begin(), subs.end(), [&](const auto sub) { return sub.subscriber_id == subscriber_id; });
        subs.erase(new_end, subs.end());
    }

    void onPublisherObject(const qtransport::TransportConnId& conn_id,
                           const qtransport::DataContextId& /* data_ctx_id */,
                           [[maybe_unused]] bool reliable,
                           quicr::messages::PublishDatagram&& datagram) override
    {
        SPDLOG_LOGGER_INFO(logger, "PublisherObject name={0} size={1}", std::string(datagram.header.name), datagram.media_data.size());

        const auto name = datagram.header.name;
        for (const auto& [ns, subs] : subscriptions)
        {
            if (!ns.contains(name))
            {
                continue;
            }

            for (const auto& sub : subs)
            {
                if (sub.conn_id == conn_id)
                {
                    // No loopback
                    SPDLOG_LOGGER_INFO(logger, "  Skipping loopback to subscriber_id={0}", sub.subscriber_id);
                    continue;
                }

                SPDLOG_LOGGER_INFO(logger, "  Forwarding to subscriber_id={0}", sub.subscriber_id);
                server->sendNamedObject(sub.subscriber_id, 1, 500, datagram);
            }
        }
    }

    void onSubscribePause([[maybe_unused]] const quicr::Namespace& quicr_namespace,
                          [[maybe_unused]] const uint64_t subscriber_id,
                          [[maybe_unused]] const qtransport::TransportConnId conn_id,
                          [[maybe_unused]] const qtransport::DataContextId data_ctx_id,
                          [[maybe_unused]] const bool pause) override
    {
    }

private:
    std::shared_ptr<quicr::Server> server;
    std::shared_ptr<spdlog::logger> logger;

    struct Subscriber
    {
        uint64_t subscriber_id;
        qtransport::TransportConnId conn_id;
    };
    std::map<quicr::Namespace, std::vector<Subscriber>> subscriptions;
};

LocalhostRelay::LocalhostRelay()
{
    const auto relayInfo = quicr::RelayInfo{
        .hostname = "127.0.0.1",
        .port = port,
        .proto = quicr::RelayInfo::Protocol::QUIC,
    };

    const auto tcfg = qtransport::TransportConfig{
        .tls_cert_filename = cert_file,
        .tls_key_filename = key_file,
        .time_queue_rx_size = 2000
    };

    static const auto logger = spdlog::stderr_color_mt("LocalhostRelay");
    const auto delegate = std::make_shared<LocalhostServerDelegate>(logger);

    server = std::make_shared<quicr::Server>(relayInfo, tcfg, delegate, logger);
    delegate->set_server(server);
}

void LocalhostRelay::run() const
{
    server->run();
}

void LocalhostRelay::stop()
{
    // The only way to stop a quicr::Server is to destroy it
    server = nullptr;
}

////////// Test case and supporting types below this line //////////

namespace
{

// Test case to verify that the relay works properly
struct SubDelegate : quicr::SubscriberDelegate
{
    SubDelegate() :
        resp_promise(), resp_future(resp_promise.get_future()), recv_promise(), recv_future(recv_promise.get_future())
    {
    }

    void await_subscribe_response() { resp_future.wait(); }
    std::tuple<quicr::Name, quicr::bytes> recv() { return recv_future.get(); }

    void onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                             const quicr::SubscribeResult& /* result */) override
    {
        resp_promise.set_value();
    }

    void onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                             const quicr::SubscribeResult::SubscribeStatus& /* reason */) override
    {
    }

    void onSubscribedObject(const quicr::Name& quicr_name,
                            uint8_t /* priority */,
                            quicr::bytes&& data) override
    {
        recv_promise.set_value({quicr_name, std::move(data)});
    }

    void onSubscribedObjectFragment(const quicr::Name& /* quicr_name */,
                                    uint8_t /* priority */,
                                    const uint64_t& /* offset */,
                                    bool /* is_last_fragment */,
                                    quicr::bytes&& /* data */) override
    {
    }

private:
    std::promise<void> resp_promise;
    std::future<void> resp_future;

    std::promise<std::tuple<quicr::Name, quicr::bytes>> recv_promise;
    std::future<std::tuple<quicr::Name, quicr::bytes>> recv_future;
};

struct PubDelegate : quicr::PublisherDelegate
{
    PubDelegate() : promise(), future(promise.get_future()) {}

    void onPublishIntentResponse(const quicr::Namespace& /* quicr_namespace */,
                                 const quicr::PublishIntentResult& /* result */) override
    {
        promise.set_value();
    }

    void await_publish_intent_response() { future.wait(); }

private:
    std::promise<void> promise;
    std::future<void> future;
};

}        // namespace

TEST_CASE("Localhost relay")
{
    const auto relay = LocalhostRelay();
    relay.run();

    // Construct three clients
    const auto logger = spdlog::stderr_color_mt("LocalhostRelayTestClient");
    const auto relayInfo = quicr::RelayInfo{
        .hostname = "127.0.0.1",
        .port = LocalhostRelay::port,
        .proto = quicr::RelayInfo::Protocol::QUIC,
    };

    const auto tcfg = qtransport::TransportConfig{
        .tls_cert_filename = "",
        .tls_key_filename = "",
    };

    using namespace std::chrono_literals;

    SPDLOG_LOGGER_INFO(logger, "Connecting...");
    auto client_a = quicr::Client(relayInfo, "test-client-1", 0, tcfg, logger);
    auto client_b = quicr::Client(relayInfo, "test-client-2", 0, tcfg, logger);
    auto client_c = quicr::Client(relayInfo, "test-client-3", 0, tcfg, logger);

    client_a.connect();
    client_b.connect();
    client_c.connect();

    REQUIRE(client_a.connected());
    REQUIRE(client_b.connected());
    REQUIRE(client_c.connected());

    // All three clients subscribe to the same namespace
    SPDLOG_LOGGER_INFO(logger, "Subscribing...");
    const auto ns = quicr::Namespace(0x01020304000000000000000000000000_name, 32);
    const auto intent = quicr::SubscribeIntent::immediate;

    const auto sub_del_a = std::make_shared<SubDelegate>();
    client_a.subscribe(sub_del_a, ns, intent,
                       quicr::TransportMode::ReliablePerTrack,
                       "origin_url", "auth_token", {});
    sub_del_a->await_subscribe_response();

    const auto sub_del_b = std::make_shared<SubDelegate>();
    client_b.subscribe(sub_del_b, ns, intent,
                       quicr::TransportMode::ReliablePerTrack,
                       "origin_url", "auth_token", {});
    sub_del_b->await_subscribe_response();

    const auto sub_del_c = std::make_shared<SubDelegate>();
    client_c.subscribe(sub_del_c, ns, intent,
                       quicr::TransportMode::ReliablePerTrack,
                       "origin_url", "auth_token", {});
    sub_del_c->await_subscribe_response();

    // One client publishes on the namespace
    SPDLOG_LOGGER_INFO(logger, "Publishing...");
    const auto name_a = 0x01020304000000000000000000000001_name;
    const auto data_a = quicr::bytes{0, 1, 2, 3, 4, 5, 6, 7};
    auto data = data_a;

    auto pub_del_a = std::make_shared<PubDelegate>();
    client_a.publishIntent(pub_del_a, ns, {}, {}, {}, quicr::TransportMode::ReliablePerTrack);
    pub_del_a->await_publish_intent_response();

    std::vector<qtransport::MethodTraceItem> trace;
    const auto start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());

    trace.push_back({"client:publish", start_time});

    client_a.publishNamedObject(name_a, 0, 1000, std::move(data), std::move(trace));

    // Verify that both other clients received on the namespace
    SPDLOG_LOGGER_INFO(logger, "Receiving...");
    const auto [name_b, data_b] = sub_del_b->recv();
    const auto [name_c, data_c] = sub_del_c->recv();
    REQUIRE(name_b == name_a);
    REQUIRE(name_c == name_a);
    REQUIRE(data_b == data_a);
    REQUIRE(data_c == data_a);
}
