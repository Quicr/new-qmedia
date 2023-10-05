#include <doctest/doctest.h>

#include <cantina/logger.h>
#include <qmedia/QDelegates.hpp>
#include <qmedia/QController.hpp>

#if 0

// TODO(richbarn) Give these delegates some meaningful behaviors
class QSubscriptionTestDelegate : public qmedia::QSubscriptionDelegate
{
public:
    virtual ~QSubscriptionTestDelegate() = default;

    int prepare(const std::string& /* sourceId */,
                const std::string& /* label */,
                const std::string& /* qualityProfile */,
                bool& /* reliable */) override
    {
        return 0;
    }

    int update(const std::string& /* sourceId */,
               const std::string& /* label */,
               const std::string& /* qualityProfile */) override
    {
        return 0;
    }

    int subscribedObject(quicr::bytes&& /* data */, std::uint32_t /* groupId */, std::uint16_t /* objectId */) override
    {
        return 0;
    }
};

class QSubscriberTestDelegate : public qmedia::QSubscriberDelegate
{
public:
    virtual ~QSubscriberTestDelegate() = default;

    std::shared_ptr<qmedia::QSubscriptionDelegate> allocateSubByNamespace(const quicr::Namespace& /* quicrNamespace */,
                                                                          const std::string& /* qualityProfile */)
    {
        return std::make_shared<QSubscriptionTestDelegate>();
    }

    int removeSubByNamespace(const quicr::Namespace& /* quicrNamespace */) { return 0; }
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

class QPubisherTestDelegate : public qmedia::QPublisherDelegate
{
public:
    virtual ~QPubisherTestDelegate() = default;

    std::shared_ptr<qmedia::QPublicationDelegate> allocatePubByNamespace(const quicr::Namespace& /* quicrNamespace */,
                                                                         const std::string& /* sourceID */,
                                                                         const std::string& /* qualityProfile */)
    {
        return std::make_shared<QPublicationTestDelegate>();
    }

    int removePubByNamespace(const quicr::Namespace& /* quicrNamespace */) { return 0; }
};

TEST_CASE("Mirror session")
{
    // Instantiate the QController
    const auto qSubscriber = std::make_shared<QSubscriberTestDelegate>();
    const auto qPublisher = std::make_shared<QPubisherTestDelegate>();
    const auto logger = std::make_shared<cantina::Logger>("QTest", "QTEST");
    auto qController = std::make_shared<qmedia::QController>(qSubscriber, qPublisher, logger);

    // Connect to a relay
    // TODO(richbarn) Start a test relay and update the connection parameters to
    // point to it.
    qtransport::TransportConfig config {
        .tls_cert_filename = NULL,
        .tls_key_filename = NULL,
    };
    qController->connect("127.0.0.1", 33435, quicr::RelayInfo::Protocol::QUIC, config);
    // XXX(richbarn) Can we verify here that we successfully connected?

    // Install a manifest
    const auto manifest = qmedia::manifest::Manifest{
      // TODO(richbarn) Fill in manifest details
    };
    qController->updateManifest(manifest);

}
#endif // 0
