#pragma once

#include "QuicrDelegates.hpp"
#include "ManifestTypes.hpp"

#include <nlohmann/json.hpp>
#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <cantina/logger.h>
#include <UrlEncoder.h>
#include <transport/transport.h>

#include <mutex>
#include <thread>

using json = nlohmann::json;

namespace qmedia
{

class QController
{
public:
    QController(std::shared_ptr<SubscriberDelegate> subscriberDelegate,
                std::shared_ptr<PublisherDelegate> publisherDelegate,
                const cantina::LoggerPointer& logger);

    ~QController();

    int connect(const std::string remoteAddress,
                std::uint16_t remotePort,
                quicr::RelayInfo::Protocol protocol,
                const qtransport::TransportConfig& config);

    int disconnect();

    void updateManifest(const manifest::Manifest& manifest_obj);

    void publishNamedObject(const quicr::Namespace& quicrNamespace,
                            std::uint8_t* data,
                            std::size_t len,
                            bool groupFlag,
                            bool reliableTransport = false);
    void publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag, bool reliableTransport = false);

private:
    /**
     * @brief Periodic keep-alive method that sends a subscribe message.
     * @param seconds The repeating interval in seconds
     */
    void periodicResubscribe(const unsigned int seconds);

    /**
     * @brief Unsubscribe from all subscriptions.
     */
    void removeSubscriptions();

    /**
     * @brief Finds the appropriate SubscriptionDelegate, or creates it if it does not exist.
     * @param quicrNamespace The namespace of the delegate.
     * @returns Shared pointer to the appropriate SubscriptionDelegate.
     */
    std::shared_ptr<SubscriptionDelegate> findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<PublicationDelegate> findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<SubscriptionDelegate> getSubscriptionDelegate(const quicr::Namespace& quicrNamespace,
                                                                  const std::string& qualityProfile);

    std::shared_ptr<PublicationDelegate> getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                const std::string& sourceID,
                                                                const std::vector<std::uint8_t>& priorities,
                                                                std::uint16_t expiry,
                                                                const std::string& qualityProfile);

    void processURLTemplates(const std::vector<std::string>& urlTemplates);
    void processSubscriptions(const std::vector<manifest::MediaStream>& subscriptions);
    void processPublications(const std::vector<manifest::MediaStream>& publications);

private:
    std::mutex qSubsMutex;
    std::mutex qPubsMutex;
    std::mutex subsMutex;
    std::mutex pubsMutex;

    const cantina::LoggerPointer logger;
    UrlEncoder encoder;

    std::shared_ptr<SubscriberDelegate> subscriberDelegate;
    std::shared_ptr<PublisherDelegate> publisherDelegate;

    quicr::namespace_map<std::shared_ptr<SubscriptionDelegate>> subscriptionsMap;
    quicr::namespace_map<std::shared_ptr<PublicationDelegate>> publicationsMap;

    std::shared_ptr<quicr::Client> client_session;

    std::thread keepaliveThread;
    bool stop;
    bool closed;
};

}        // namespace qmedia
