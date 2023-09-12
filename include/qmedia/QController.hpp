#pragma once

#include "QuicrDelegates.hpp"

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
    QController(std::shared_ptr<QSubscriberDelegate> subscriberDelegate,
                std::shared_ptr<QPublisherDelegate> publisherDelegate);

    ~QController();
    int connect(const std::string remoteAddress,
                std::uint16_t remotePort,
                quicr::RelayInfo::Protocol protocol,
                const qtransport::TransportConfig config);
    int disconnect();

    void close();

    int updateManifest(const std::string manifest);

    void publishNamedObject(const quicr::Namespace& quicrNamespace, std::uint8_t* data, std::size_t len, bool groupFlag);
    void publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag);

private:
    void periodicResubscribe(const unsigned int seconds);
    void removeSubscriptions();

    std::shared_ptr<QuicrTransportSubDelegate> findQuicrSubscriptionDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<QuicrTransportSubDelegate>
    createQuicrSubscriptionDelegate(const std::string,
                                    const quicr::Namespace&,
                                    const quicr::SubscribeIntent intent,
                                    const std::string originUrl,
                                    const bool useReliableTransport,
                                    const std::string authToken,
                                    quicr::bytes e2eToken,
                                    std::shared_ptr<qmedia::QSubscriptionDelegate>);

    std::shared_ptr<QuicrTransportPubDelegate> findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<QuicrTransportPubDelegate>
    createQuicrPublicationDelegate(const std::string,
                                   const quicr::Namespace&,
                                   const std::string& originUrl,
                                   const std::string& authToken,
                                   quicr::bytes&& payload,
                                   const std::vector<std::uint8_t>& priority,
                                   std::uint16_t expiry,
                                   bool reliableTransport,
                                   std::shared_ptr<qmedia::QPublicationDelegate>);

    std::shared_ptr<QSubscriptionDelegate> getSubscriptionDelegate(const quicr::Namespace& quicrNamespace,
                                                                   const std::string& qualityProfile);
    std::shared_ptr<QPublicationDelegate> getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                 const std::string& sourceID,
                                                                 const std::string& qualityProfile);

    int startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                          const std::string sourceId,
                          const quicr::Namespace& quicrNamespace,
                          const quicr::SubscribeIntent intent,
                          const std::string originUrl,
                          const bool useReliableTransport,
                          const std::string authToken,
                          quicr::bytes e2eToken);

    void stopSubscription(const quicr::Namespace& quicrNamespace);

    int startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                         const std::string sourceId,
                         const quicr::Namespace& quicrNamespace,
                         const std::string& origin_url,
                         const std::string& auth_token,
                         quicr::bytes&& payload,
                         const std::vector<std::uint8_t>& priority,
                         std::uint16_t expiry,
                         bool reliableTransport);

    void stopPublication(const quicr::Namespace& quicrNamespace);

    int processURLTemplates(json&);
    int processSubscriptions(json&);
    int processPublications(json&);

private:
    std::mutex qSubsMutex;
    std::mutex qPubsMutex;
    std::mutex subsMutex;
    std::mutex pubsMutex;
    const cantina::LoggerPointer logger;
    UrlEncoder encoder;

    std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate;
    std::shared_ptr<QPublisherDelegate> qPublisherDelegate;

    std::map<quicr::Namespace, std::shared_ptr<QSubscriptionDelegate>> qSubscriptionsMap;
    std::map<quicr::Namespace, std::shared_ptr<QPublicationDelegate>> qPublicationsMap;

    std::map<quicr::Namespace, std::shared_ptr<QuicrTransportSubDelegate>> quicrSubscriptionsMap;
    std::map<quicr::Namespace, std::shared_ptr<QuicrTransportPubDelegate>> quicrPublicationsMap;

    std::shared_ptr<quicr::QuicRClient> quicrClient;

    std::thread keepaliveThread;
    bool stop;
    bool closed;
};

}        // namespace qmedia
