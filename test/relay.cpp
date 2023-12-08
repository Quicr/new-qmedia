#include <doctest/doctest.h>

#include "relay.h"

#include <quicr/quicr_client.h>

#include <future>
#include <chrono>

class LocalhostServerDelegate : public quicr::ServerDelegate
{
public:
    LocalhostServerDelegate(std::shared_ptr<cantina::Logger> logger_in) : server(nullptr), logger(std::move(logger_in))
    {
    }

    void set_server(std::shared_ptr<quicr::Server> server_in) { server = std::move(server_in); }

    void onPublishIntent(const quicr::Namespace& quicr_namespace,
                         const std::string& /* origin_url */,
                         const std::string& /* auth_token */,
                         quicr::bytes&& /* e2e_token */) override
    {
        logger->info << "PublishIntent namespace=" << quicr_namespace << std::flush;

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
        logger->info << "Subscribe namespace=" << quicr_namespace << " subscriber_id=" << subscriber_id << std::flush;

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
        logger->info << "Unsubscribe namespace=" << quicr_namespace << " subscriber_id=" << subscriber_id << std::flush;

        const auto status = quicr::SubscribeResult::SubscribeStatus::Ok;
        server->subscriptionEnded(subscriber_id, quicr_namespace, status);

        auto& subs = subscriptions.at(quicr_namespace);
        auto new_end = std::remove_if(
            subs.begin(), subs.end(), [&](const auto sub) { return sub.subscriber_id == subscriber_id; });
        subs.erase(new_end, subs.end());
    }

    void onPublisherObject(const qtransport::TransportConnId& conn_id,
                           const qtransport::DataContextId& /* data_ctx_id */,
                           quicr::messages::PublishDatagram&& datagram) override
    {
        logger->info << "PublisherObject name=" << datagram.header.name << " size=" << datagram.media_data.size()
                     << std::flush;

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
                    logger->info << "  Skipping loopback to subscriber_id=" << sub.subscriber_id << std::flush;
                    continue;
                }

                logger->info << "  Forwarding to subscriber_id=" << sub.subscriber_id << std::flush;
                server->sendNamedObject(sub.subscriber_id, 1, 200, datagram);
            }
        }
    }

private:
    std::shared_ptr<quicr::Server> server;
    std::shared_ptr<cantina::Logger> logger;

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
    };

    const auto logger = std::make_shared<cantina::Logger>("LocalhostRelay");
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
    const auto logger = std::make_shared<cantina::Logger>("LocalhostRelayTestClient");
    const auto relayInfo = quicr::RelayInfo{
        .hostname = "127.0.0.1",
        .port = LocalhostRelay::port,
        .proto = quicr::RelayInfo::Protocol::QUIC,
    };

    const auto tcfg = qtransport::TransportConfig{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
    };

    using namespace std::chrono_literals;

    logger->Log("Connecting...");
    auto client_a = quicr::Client(relayInfo, tcfg, logger);
    auto client_b = quicr::Client(relayInfo, tcfg, logger);
    auto client_c = quicr::Client(relayInfo, tcfg, logger);

    client_a.connect();
    client_b.connect();
    client_c.connect();

    REQUIRE(client_a.connected());
    REQUIRE(client_b.connected());
    REQUIRE(client_c.connected());

    // All three clients subscribe to the same namespace
    logger->Log("Subscribing...");
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
    logger->Log("Publishing...");
    const auto name_a = 0x01020304000000000000000000000001_name;
    const auto data_a = quicr::bytes{0, 1, 2, 3, 4, 5, 6, 7};
    auto data = data_a;

    auto pub_del_a = std::make_shared<PubDelegate>();
    client_a.publishIntent(pub_del_a, ns, {}, {}, {}, quicr::TransportMode::ReliablePerTrack);
    pub_del_a->await_publish_intent_response();

    client_a.publishNamedObject(name_a, 0, 1000, std::move(data));

    // Verify that both other clients received on the namespace
    logger->Log("Receiving...");
    const auto [name_b, data_b] = sub_del_b->recv();
    const auto [name_c, data_c] = sub_del_c->recv();
    REQUIRE(name_b == name_a);
    REQUIRE(name_c == name_a);
    REQUIRE(data_b == data_a);
    REQUIRE(data_c == data_a);
}
