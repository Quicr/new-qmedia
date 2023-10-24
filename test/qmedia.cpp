#include <doctest/doctest.h>

#include <UrlEncoder.h>
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
        const auto status = object_future.wait_for(3000ms);
        if (status != std::future_status::ready)
        {
            throw std::runtime_error("Object collection timed out");
        }

        return _objects;
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
                const std::string& qualityProfile,
                bool& /* reliable */) override
    {
        collector->sourceId(sourceId);
        collector->label(label);
        collector->qualityProfile(qualityProfile);
        return 0;
    }

    int update(const std::string& /* sourceId */,
               const std::string& /* label */,
               const std::string& /* qualityProfile */) override
    {
        // Always error on update to force a prepare call
        return 1;
    }

    int subscribedObject(quicr::bytes&& data, std::uint32_t /* groupId */, std::uint16_t /* objectId */) override
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

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& /* quicrNamespace */,
                                                                          const std::string& /* qualityProfile */)
    {
        return std::make_shared<QSubscriptionTestDelegate>(collector);
    }

    int removeSubByNamespace(const quicr::Namespace& /* quicrNamespace */) { return 0; }

private:
    std::shared_ptr<SubscriptionCollector> collector;
};

class QPublicationTestDelegate : public qmedia::QPublicationDelegate
{
public:
    virtual ~QPublicationTestDelegate() = default;

    int prepare(const std::string& /* sourceId */, const std::string& /* qualityProfile */, bool& /* reliable */)
    {
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
                                                                         const std::string& /* qualityProfile */)
    {
        return std::make_shared<QPublicationTestDelegate>();
    }

    int removePubByNamespace(const quicr::Namespace& /* quicrNamespace */) { return 0; }
};

static qmedia::QController make_controller(std::shared_ptr<SubscriptionCollector> collector)
{
    const auto sub = std::make_shared<QSubscriberTestDelegate>(std::move(collector));
    const auto pub = std::make_shared<QPublisherTestDelegate>();
    const auto logger = std::make_shared<cantina::Logger>("QTest", "QTEST");
    return {sub, pub, logger};
}

static qmedia::manifest::MediaStream make_media_stream(uint32_t endpoint_id)
{
    const auto source_base = "source "s;
    const auto label_base = "Participant "s;

    const auto url_template =
        "quicr://webex.cisco.com<pen=1><sub_pen=1>/conferences/<int24>/mediatype/<int8>/endpoint/<int16>"s;
    const auto url_base = "quicr://webex.cisco.com/conferences/34/mediatype/1/endpoint/"s;
    auto encoder = UrlEncoder{};
    encoder.AddTemplate(url_template);

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
                            .quicrNamespace = encoder.EncodeUrl(url_base + endpoint_id_string),
                            .priorities = {1},
                            .expiry = 500,
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

TEST_CASE("Two-party session")
{
    // Start up a local relay
    const auto relay = LocalhostRelay();
    relay.run();

    // Instantiate two QControllers
    auto collector_a = std::make_shared<SubscriptionCollector>();
    auto controller_a = make_controller(collector_a);

    auto collector_b = std::make_shared<SubscriptionCollector>();
    auto controller_b = make_controller(collector_b);

    // Connect to the relay
    qtransport::TransportConfig config{
        .tls_cert_filename = nullptr,
        .tls_key_filename = nullptr,
    };
    controller_a.connect("127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, config);
    controller_b.connect("127.0.0.1", LocalhostRelay::port, quicr::RelayInfo::Protocol::QUIC, config);

    // Create and configure manifests
    const auto media_a = make_media_stream(1);
    const auto media_b = make_media_stream(2);

    const auto manifest_a = qmedia::manifest::Manifest{.subscriptions = {media_b}, .publications = {media_a}};
    controller_a.updateManifest(manifest_a);

    const auto manifest_b = qmedia::manifest::Manifest{.subscriptions = {media_a}, .publications = {media_b}};
    controller_b.updateManifest(manifest_b);

    const auto ns_a = media_a.profileSet.profiles[0].quicrNamespace;
    const auto ns_b = media_b.profileSet.profiles[0].quicrNamespace;

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
    REQUIRE(collector_b->qualityProfile() == "opus,br=6");

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
    REQUIRE(collector_a->qualityProfile() == "opus,br=6");
}
