#pragma once

#include "QuicrDelegates.hpp"
#include "ManifestTypes.hpp"

#include <nlohmann/json.hpp>
#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>
#include <cantina/logger.h>
#include <transport/transport.h>

#include <mutex>
#include <thread>

using json = nlohmann::json;
using SourceId = std::string;

namespace qmedia
{

class QController
{
public:
    enum class PublicationState { active, paused };
    struct PublicationReport {
        PublicationState state;
        quicr::Namespace quicrNamespace;
    };

    QController(std::shared_ptr<QSubscriberDelegate> subscriberDelegate,
                std::shared_ptr<QPublisherDelegate> publisherDelegate,
                const cantina::LoggerPointer& logger);

    ~QController();

    int connect(const std::string remoteAddress,
                std::uint16_t remotePort,
                quicr::RelayInfo::Protocol protocol,
                const qtransport::TransportConfig& config);

    int disconnect();
    
    bool connected() const;

    [[deprecated("Use QController::disconnect instead")]] void close();

    [[deprecated("Use parsed Manifest object instead of string")]] void updateManifest(const std::string& manifest_json);

    void updateManifest(const manifest::Manifest& manifest_obj);

    // FIXME(richbarn): These methods should use std::range<const uint8_t>
    // instead of naked pointers and lengths.
    void publishNamedObject(const quicr::Namespace& quicrNamespace,
                            const std::uint8_t* data,
                            std::size_t len,
                            bool groupFlag);
    void publishNamedObjectTest(std::uint8_t* data, std::size_t len, bool groupFlag);

    void setSubscriptionSingleOrdered(bool new_value) { is_singleordered_subscription = new_value; }
    void setPublicationSingleOrdered(bool new_value) { is_singleordered_publication = new_value; }

    void stopSubscription(const quicr::Namespace& quicrNamespace);

    std::vector<SourceId> getSwitchingSets();
    std::vector<quicr::Namespace> getSubscriptions(const std::string& sourceId);
    std::vector<PublicationReport> getPublications();
    void setPublicationState(const quicr::Namespace& quicrNamespace, const PublicationState);
private:
    struct PublicationDetails
    {
        PublicationState state;
        std::shared_ptr<PublicationDelegate> delegate;
    };

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

    std::shared_ptr<SubscriptionDelegate>
    createQuicrSubscriptionDelegate(const std::string& sourceID,
                                    const quicr::Namespace& quicrNamespace,
                                    const quicr::SubscribeIntent intent,
                                    const std::string& originUrl,
                                    const quicr::TransportMode transportMode,
                                    const std::string& authToken,
                                    quicr::bytes&& e2eToken,
                                    std::shared_ptr<qmedia::QSubscriptionDelegate> delegate);

    std::shared_ptr<PublicationDelegate> findQuicrPublicationDelegate(const quicr::Namespace& quicrNamespace);

    std::shared_ptr<PublicationDelegate> createQuicrPublicationDelegate(std::shared_ptr<qmedia::QPublicationDelegate>,
                                                                        const std::string&,
                                                                        const quicr::Namespace&,
                                                                        const std::string& originUrl,
                                                                        const std::string& authToken,
                                                                        quicr::bytes&& payload,
                                                                        const std::vector<std::uint8_t>& priority,
                                                                        const std::vector<std::uint16_t>& expiry,
                                                                        const quicr::TransportMode transportMode);

    std::shared_ptr<QSubscriptionDelegate> getSubscriptionDelegate(const SourceId& sourceId,
                                                                   const manifest::ProfileSet& profileSet);
    std::shared_ptr<QPublicationDelegate> getPublicationDelegate(const quicr::Namespace& quicrNamespace,
                                                                 const std::string& sourceID,
                                                                 const std::string& qualityProfile,
                                                                 const std::string& appTag);

    int startSubscription(std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                          const std::string& sourceId,
                          const quicr::Namespace& quicrNamespace,
                          const quicr::SubscribeIntent intent,
                          const std::string& originUrl,
                          const quicr::TransportMode transportMode,
                          const std::string& authToken,
                          quicr::bytes& e2eToken);

    int startPublication(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                         const std::string sourceId,
                         const quicr::Namespace& quicrNamespace,
                         const std::string& origin_url,
                         const std::string& auth_token,
                         quicr::bytes&& payload,
                         const std::vector<std::uint8_t>& priority,
                         const std::vector<std::uint16_t>& expiry,
                         const quicr::TransportMode transportMode);

    void stopPublication(const quicr::Namespace& quicrNamespace);

    void processURLTemplates(const std::vector<std::string>& urlTemplates);
    void processSubscriptions(const std::vector<manifest::MediaStream>& subscriptions);
    void processPublications(const std::vector<manifest::MediaStream>& publications);

private:
    std::mutex qSubsMutex;
    std::mutex qPubsMutex;
    std::mutex subsMutex;
    std::mutex pubsMutex;

    const cantina::LoggerPointer logger;

    std::shared_ptr<QSubscriberDelegate> qSubscriberDelegate;
    std::shared_ptr<QPublisherDelegate> qPublisherDelegate;

    std::map<SourceId, std::shared_ptr<QSubscriptionDelegate>> qSubscriptionsMap;
    std::map<SourceId, std::shared_ptr<QPublicationDelegate>> qPublicationsMap;

    quicr::namespace_map<std::shared_ptr<SubscriptionDelegate>> quicrSubscriptionsMap;
    quicr::namespace_map<PublicationDetails> quicrPublicationsMap;

    std::shared_ptr<quicr::Client> client_session;

    std::thread keepaliveThread;
    bool stop;
    bool closed;
    bool is_singleordered_subscription = true;
    bool is_singleordered_publication = false;
};

}        // namespace qmedia
