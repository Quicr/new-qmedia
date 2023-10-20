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
    struct Object
    {
        uint32_t groupId;
        uint16_t objectId;
        quicr::bytes data;

        bool operator<(const Object& rhs) const
        {
            return groupId < rhs.groupId || (groupId == rhs.groupId && objectId < rhs.objectId);
        }

        bool operator==(const Object& rhs) const
        {
            return groupId == rhs.groupId && objectId == rhs.objectId && data == rhs.data;
        }
    };

    std::optional<std::string> sourceId;
    std::optional<std::string> label;
    std::optional<std::string> qualityProfile;

    std::set<Object> objects;

    std::mutex object_mutex;
    size_t expected_object_count = std::numeric_limits<size_t>::max();
    std::promise<void> object_promise;
    std::future<void> object_future;

    SubscriptionCollector()
      : object_future(object_promise.get_future())
    {}

    void add_object(Object obj)
    {
        const auto _ = std::lock_guard(object_mutex);
        objects.insert(std::move(obj));

        if (objects.size() >= expected_object_count)
        {
            object_promise.set_value();
        }
    }

    // XXX(richbarn): Currently we collect only the payloads of the subscribed
    // objects.  We should also verify that the group/object IDs are
    // consistent, but it appears that the sender has no idea what the transmitted
    // IDs are, so we can't compare to them.  So we have this method to map the
    // collected objects to just their payloads.
    //
    // Note that we use std::set and not an ordered container like std::vector so
    // that delivery order doesn't matter.
    std::set<quicr::bytes> collected_payloads()
    {
        auto out = std::set<quicr::bytes>{};
        std::transform(
            objects.begin(), objects.end(), std::inserter(out, out.end()), [](const auto& obj) { return obj.data; });
        objects.clear();
        return out;
    }

    std::set<quicr::bytes> await(size_t object_count)
    {
        {
            const auto _ = std::lock_guard(object_mutex);
            expected_object_count = object_count;

            if (objects.size() >= object_count)
            {
                return collected_payloads();
            }
        }

        // The mutex must be unlocked here so that the transport thread can
        // add objects to the set.
        const auto status = object_future.wait_for(1000ms);
        if (status != std::future_status::ready) {
           throw std::runtime_error("Object collection timed out");
        }

        const auto _ = std::lock_guard(object_mutex);
        return collected_payloads();
    }
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
        collector->sourceId = sourceId;
        collector->label = label;
        collector->qualityProfile = qualityProfile;
        return 0;
    }

    int update(const std::string& /* sourceId */,
               const std::string& /* label */,
               const std::string& /* qualityProfile */) override
    {
        // Always error on update to force a prepare call
        return 1;
    }

    int subscribedObject(quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) override
    {
        collector->add_object({groupId, objectId, data});
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
    REQUIRE(collector_b->sourceId.value() == "1");
    REQUIRE(collector_b->label.value() == "Participant 1");
    REQUIRE(collector_b->qualityProfile.value() == "opus,br=6");

    // Send media from participant 2 and verify that it arrived at the other participants
    const auto sent_b = test_data(2);
    for (const auto& obj : sent_b)
    {
        controller_b.publishNamedObject(ns_b, obj.data(), obj.size(), false);
    }

    const auto received_a = collector_a->await(sent_b.size());
    REQUIRE(sent_b == received_a);
    REQUIRE(collector_a->sourceId.value() == "2");
    REQUIRE(collector_a->label.value() == "Participant 2");
    REQUIRE(collector_a->qualityProfile.value() == "opus,br=6");
}
