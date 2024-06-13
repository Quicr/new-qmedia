#include <doctest/doctest.h>

#include <cantina/logger.h>
#include <qmedia/QController.hpp>
#include <qmedia/QDelegates.hpp>

#include "relay.h"

#include <set>
#include <future>

using namespace std::string_literals;
using namespace std::chrono_literals;

struct SubscriptionCollector
{

    SubscriptionCollector() : object_future(object_promise.get_future()) {}

    void add_object(quicr::bytes&& data)
    {
        const auto _ = std::lock_guard(object_mutex);
        _objects.insert(std::move(data));

        if (_objects.size() >= expected_object_count)
        {
            object_promise.set_value();
        }
    }

    std::set<quicr::bytes> await(size_t object_count)
    {
        {
            const auto _ = std::lock_guard(object_mutex);
            expected_object_count = object_count;

            if (_objects.size() >= object_count)
            {
                return _objects;
            }
        }

        // The mutex must be unlocked here so that the transport thread can
        // add objects to the set.
        const auto status = object_future.wait_for(1500ms);
        if (status != std::future_status::ready)
        {
            throw std::runtime_error("Object collection timed out "
                                     + std::to_string(expected_object_count)
                                     + " != " + std::to_string(_objects.size()));
        }

        return _objects;
    }

    void clear()
    {
        const auto _ = std::lock_guard(object_mutex);
        _objects.clear();
        object_promise = {};
        object_future = object_promise.get_future();
    }

    // Thread-safe, unwrapping accessors
#define STRING_ACCESSOR(field_name) \
    void field_name(std::string field_name) { \
      const auto _ = std::lock_guard(object_mutex); \
      _##field_name = std::move(field_name); \
    } \
    \
    std::string field_name() { \
      const auto _ = std::lock_guard(object_mutex); \
      return _##field_name.value(); \
    }

    STRING_ACCESSOR(sourceId)
    STRING_ACCESSOR(label)
    STRING_ACCESSOR(qualityProfile)
#undef STRING_ACCESSOR

private:
    std::optional<std::string> _sourceId;
    std::optional<std::string> _label;
    std::optional<std::string> _qualityProfile;

    // XXX(richbarn): We currently collect only the payloads of the subcribed
    // objects.  In principle, we should also capture and verify the
    // group/object IDs.  However, it appears that the sender has no idea what
    // the transmitted IDs are, so we can't compare them.  So for now we only
    // capture the objects themselves.
    std::set<quicr::bytes> _objects;

    size_t expected_object_count = std::numeric_limits<size_t>::max();
    std::mutex object_mutex;
    std::promise<void> object_promise;
    std::future<void> object_future;
};

class QSubscriptionTestDelegate : public qmedia::QSubscriptionDelegate
{
public:
    QSubscriptionTestDelegate(std::shared_ptr<SubscriptionCollector> collector_in) : collector(std::move(collector_in))
    {
    }

    virtual ~QSubscriptionTestDelegate() = default;

    int prepare(const std::string& sourceId,
                const std::string& label,
                const qmedia::manifest::ProfileSet& /* profileSet */,
                quicr::TransportMode& transportMode) override
    {
        collector->sourceId(sourceId);
        collector->label(label);
        transportMode = quicr::TransportMode::ReliablePerTrack; // Testing microbursts data, which often results in drops. Use reliable for tests.
        // collector->qualityProfile(profileSet);
        return 0;
    }

    int update(const std::string& /* sourceId */,
               const std::string& /* label */,
               const qmedia::manifest::ProfileSet& /* profileSet */) override
    {
        // Always error on update to force a prepare call
        return 1;
    }

    int subscribedObject(const quicr::Namespace& /* namespace */, quicr::bytes&& data, std::uint32_t /* groupId */, std::uint16_t /* objectId */) override
    {
        collector->add_object(std::move(data));
        return 0;
    }

private:
    std::shared_ptr<SubscriptionCollector> collector;
};

class QSubscriberTestDelegate : public qmedia::QSubscriberDelegate
{
public:
    QSubscriberTestDelegate(std::shared_ptr<SubscriptionCollector> collector_in) : collector(std::move(collector_in)) {}

    virtual ~QSubscriberTestDelegate() = default;

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubBySourceId(const std::string& /* sourceId */,
                                                                         const qmedia::manifest::ProfileSet& /* profileSet */)
    {
        return std::make_shared<QSubscriptionTestDelegate>(collector);
    }

    int removeSubBySourceId(const std::string& /* sourceId */) { return 0; }

private:
    std::shared_ptr<SubscriptionCollector> collector;
};

class QPublicationTestDelegate : public qmedia::QPublicationDelegate
{
public:
    virtual ~QPublicationTestDelegate() = default;

    int prepare(const std::string& /* sourceId */, const std::string& /* qualityProfile */, quicr::TransportMode& transportMode)
    {
        transportMode = quicr::TransportMode::ReliablePerGroup; // Testing microbursts data, which often results in drops. Use reliable for tests.
        return 0;
    }

    int update(const std::string& /* sourceId */, const std::string& /* qualityProfile */) { return 0; }

    void publish(bool /* pubFlag */) {}
};

class QPublisherTestDelegate : public qmedia::QPublisherDelegate
{
public:
    virtual ~QPublisherTestDelegate() = default;

    std::shared_ptr<qmedia::QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& /* quicrNamespace */,
                                                                         const std::string& /* sourceID */,
                                                                         const std::string& /* qualityProfile */,
                                                                         const std::string& /* appTag */)
    {
        return std::make_shared<QPublicationTestDelegate>();
    }

    int removePubByNamespace(const quicr::Namespace& /* quicrNamespace */) { return 0; }
};

static qmedia::QController make_controller(std::shared_ptr<SubscriptionCollector> collector, bool encrypt = true)
{
    const auto sub = std::make_shared<QSubscriberTestDelegate>(std::move(collector));
    const auto pub = std::make_shared<QPublisherTestDelegate>();
    const auto logger = std::make_shared<cantina::Logger>("QTest", "QTEST");
    logger->SetLogLevel("DEBUG");
    const auto suite = encrypt ? std::optional<sframe::CipherSuite>(qmedia::Default_Cipher_Suite) : std::nullopt;
    return qmedia::QController(sub, pub, logger, true, suite);
}

static qmedia::manifest::MediaStream make_media_stream(uint32_t endpoint_id)
{
    const auto source_base = "source "s;
    const auto label_base = "Participant "s;

    const auto endpoint_id_string = std::to_string(endpoint_id);
    return {
        .mediaType = "audio",
        .sourceName = source_base + endpoint_id_string,
        .sourceId = endpoint_id_string,
        .label = label_base + endpoint_id_string,
        .profileSet =
            {
                .type = "singleordered",
                .profiles =
                    {
                        {
                            .qualityProfile = "opus,br=6",
                            .quicrNamespace = std::string_view("0x000001010003EBA30001000000000000/80"),
                            .priorities = {1},
                            .expiry = {500,500},
                            .appTag = "primaryV"
                        },
                    },
            },
    };
}

static std::set<quicr::bytes> test_data(uint8_t label)
{
    auto out = std::set<quicr::bytes>{};
    for (auto i = size_t(0); i < 256; i++)
    {
        const auto data = quicr::bytes{0, 0, label, static_cast<uint8_t>(i)};
        out.insert(data);
    }
    return out;
}

static void two_party_session(bool encrypt)
{
    // Start up a local relay
    const auto relay = LocalhostRelay();
    relay.run();

    // Instantiate two QControllers
    auto collector_a = std::make_shared<SubscriptionCollector>();
    auto controller_a = make_controller(collector_a, encrypt);

    auto collector_b = std::make_shared<SubscriptionCollector>();
    auto controller_b = make_controller(collector_b, encrypt);

    // Connect to the relay
    qtransport::TransportConfig config{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
    };
    controller_a.connect("a@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);
    controller_b.connect("a@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);

    // Create and configure manifests
    const auto media_a = make_media_stream(1);
    const auto media_b = make_media_stream(2);

    const auto manifest_a = qmedia::manifest::Manifest{.subscriptions = {media_b}, .publications = {media_a}};
    controller_a.updateManifest(manifest_a);

    const auto manifest_b = qmedia::manifest::Manifest{.subscriptions = {media_a}, .publications = {media_b}};
    controller_b.updateManifest(manifest_b);

    const auto ns_a = media_a.profileSet.profiles[0].quicrNamespace;
    const auto ns_b = media_b.profileSet.profiles[0].quicrNamespace;

    /* TODO: Add status to QController to return state of controller state with relay based on manifest config
     * Pub/Sub clients are not synchronized, instead they are async between each other and wtih control messages
     * This test counts messages sent to messages received. We need to wait till both clients have had a
     * chance to establish (publish intent and subscribe) state with the relay.
     * Both clients should finish publish intents and subscriptions within 1 second.
     * Sleep for 1 sec before publishing.
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Send media from participant 1 and verify that it arrived at the other participants
    const auto sent_a = test_data(1);
    for (const auto& obj : sent_a)
    {
        controller_a.publishNamedObject(ns_a, obj.data(), obj.size(), false);
    }

    const auto received_b = collector_b->await(sent_a.size());

    REQUIRE(sent_a == received_b);
    REQUIRE(collector_b->sourceId() == "1");
    REQUIRE(collector_b->label() == "Participant 1");
    // REQUIRE(collector_b->qualityProfile() == "opus,br=6");

    // Send media from participant 2 and verify that it arrived at the other participants
    const auto sent_b = test_data(2);
    for (const auto& obj : sent_b)
    {
        controller_b.publishNamedObject(ns_b, obj.data(), obj.size(), false);
    }

    const auto received_a = collector_a->await(sent_b.size());
    REQUIRE(sent_b == received_a);
    REQUIRE(collector_a->sourceId() == "2");
    REQUIRE(collector_a->label() == "Participant 2");
    // REQUIRE(collector_a->qualityProfile() == "opus,br=6");
}

TEST_CASE("Two-party session")
{
    two_party_session(true);
    two_party_session(false);
}

TEST_CASE("Fetch Switching Sets & Subscriptions")
{
    // Setup.
    const SourceId expectedSourceId = "1";
    auto collector = std::make_shared<SubscriptionCollector>();
    auto controller = make_controller(collector);

    // No manifest, no result.
    const std::vector<SourceId>& empty = controller.getSwitchingSets();
    REQUIRE(empty.empty());
    const std::vector<quicr::Namespace>& emptySubs = controller.getSubscriptions(expectedSourceId);
    REQUIRE(emptySubs.empty());

    // Manifest with sets, expect the sets.
    const auto media = make_media_stream(stoi(expectedSourceId));
    const auto& expectedNamespace = media.profileSet.profiles[0].quicrNamespace;
    const auto manifest = qmedia::manifest::Manifest{.subscriptions = {media}, .publications = {}};
    controller.updateManifest(manifest);

    // Expect the manifest's data to be present.
    const std::vector<SourceId>& sets = controller.getSwitchingSets();
    REQUIRE(sets.size() == 1);
    const SourceId retrievedSourceId = sets[0];
    REQUIRE(retrievedSourceId == expectedSourceId);

    const std::vector<quicr::Namespace>& subs = controller.getSubscriptions(expectedSourceId);
    REQUIRE(subs.size() == 1);
    const quicr::Namespace retrievedNamespace = subs[0];
    REQUIRE(retrievedNamespace == expectedNamespace);
}

TEST_CASE("Fetch Publications")
{
    // Setup.
    auto collector = std::make_shared<SubscriptionCollector>();
    auto controller = make_controller(collector);
    const auto relay = LocalhostRelay();
    relay.run();
    qtransport::TransportConfig config{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
    };
    controller.connect("a@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);

    // No manifest, no result.
    const std::vector<qmedia::QController::PublicationReport>& empty = controller.getPublications();
    REQUIRE(empty.empty());

    // Manifest with publications, expect the publications.
    const auto media = make_media_stream(1);
    const auto& expectedNamespace = media.profileSet.profiles[0].quicrNamespace;
    const auto manifest = qmedia::manifest::Manifest{.subscriptions = {}, .publications = {media}};
    controller.updateManifest(manifest);

    // Expect the manifest's data to be present.
    const std::vector<qmedia::QController::PublicationReport>& pubs = controller.getPublications();
    REQUIRE(pubs.size() == 1);
    const quicr::Namespace retrievedNamespace = pubs[0].quicrNamespace;
    REQUIRE(retrievedNamespace == expectedNamespace);
}

TEST_CASE("Test Publication States")
{
    // Start up a local relay
    const auto relay = LocalhostRelay();
    relay.run();

    // Instantiate two QControllers
    auto controller_a = make_controller(std::make_shared<SubscriptionCollector>());
    auto collector = std::make_shared<SubscriptionCollector>();
    auto controller_b = make_controller(collector);

    // Connect to the relay
    qtransport::TransportConfig config{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
        .debug = true,
    };
    controller_a.connect("a@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);
    controller_b.connect("b@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);

    // Create and configure manifests
    const auto media = make_media_stream(1);

    const auto manifest_a = qmedia::manifest::Manifest{.subscriptions = {}, .publications = {media}};
    controller_a.updateManifest(manifest_a);

    const auto manifest_b = qmedia::manifest::Manifest{.subscriptions = {media}, .publications = {}};
    controller_b.updateManifest(manifest_b);

    const quicr::Namespace& quicrNamespace = media.profileSet.profiles[0].quicrNamespace;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Send media from participant 1 and verify that it arrived at the other participants
    {
        const auto sent = test_data(1);
        for (const auto& obj : sent)
        {
            controller_a.publishNamedObject(quicrNamespace, obj.data(), obj.size(), false);
        }

        const auto received = collector->await(sent.size());
        REQUIRE(sent == received);
        REQUIRE(collector->sourceId() == "1");
        REQUIRE(collector->label() == "Participant 1");
    }

    // Set pause state, and retest, verify no media is received.
    {
        collector->clear();
        const auto sent_paused = test_data(2);
        controller_a.setPublicationState(quicrNamespace, qmedia::QController::PublicationState::paused);
        for (const auto& obj : sent_paused)
        {
            controller_a.publishNamedObject(quicrNamespace, obj.data(), obj.size(), false);
        }
    }

    // Set active, retest, verify media flows again.
    {
        const auto sent_resumed = test_data(3);
        controller_a.setPublicationState(quicrNamespace, qmedia::QController::PublicationState::active);
        int i = 0;
        for (const auto& obj : sent_resumed)
        {
            // Inject a little delay
            if (i % 30 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            ++i;

            controller_a.publishNamedObject(quicrNamespace, obj.data(), obj.size(), false);
        }

        const auto& received_resumed = collector->await(sent_resumed.size());

        REQUIRE(sent_resumed == received_resumed);
    }
}

TEST_CASE("Subscription set/get state")
{
    // Setup.
    auto collector = std::make_shared<SubscriptionCollector>();
    auto controller = make_controller(collector);
    const auto media = make_media_stream(1);
    const auto manifest = qmedia::manifest::Manifest{.subscriptions = {media}, .publications = {}};
    const auto relay = LocalhostRelay();
    relay.run();
    qtransport::TransportConfig config{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
    };
    controller.connect("a@cisco.com", "127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, 0, config);
    controller.updateManifest(manifest);

    // Wait for subscriptions to propagate.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Active state.
    const quicr::SubscriptionState& state = controller.getSubscriptionState(media.profileSet.profiles[0].quicrNamespace);
    REQUIRE(state == quicr::SubscriptionState::Ready);

    // Set to paused.
    controller.setSubscriptionState(media.profileSet.profiles[0].quicrNamespace, quicr::TransportMode::Pause);
    const quicr::SubscriptionState& pausedState = controller.getSubscriptionState(media.profileSet.profiles[0].quicrNamespace);
    REQUIRE(pausedState == quicr::SubscriptionState::Paused);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_CASE("Parent logger")
{
    // Parent logger gets log messages.
    bool messageReceived = false;
    auto parent = std::make_shared<cantina::CustomLogger>([&messageReceived](auto level, const std::string& msg, bool) {
        messageReceived |= (msg.find("QController started...") != std::string::npos && level == cantina::LogLevel::Debug);
    });
    parent->SetLogLevel(cantina::LogLevel::Debug);
    auto controller = qmedia::QController(nullptr, nullptr, parent);
    REQUIRE(messageReceived);

    // Non-parent construction still should work.
    qmedia::QController(nullptr, nullptr, nullptr);
}