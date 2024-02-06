#pragma once

#include "QSFrameContext.hpp"
#include "qmedia/QDelegates.hpp"

#include <cantina/logger.h>
#include <quicr/quicr_common.h>
#include <quicr/quicr_client.h>

#include <string>

namespace qmedia
{

class SubscriptionDelegate : public quicr::SubscriberDelegate, public std::enable_shared_from_this<SubscriptionDelegate>
{
    SubscriptionDelegate(const std::string& sourceId,
                         const quicr::Namespace& quicrNamespace,
                         const quicr::SubscribeIntent intent,
                         const quicr::TransportMode transport_mode,
                         const std::string& originUrl,
                         const std::string& authToken,
                         quicr::bytes e2eToken,
                         std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                         const cantina::LoggerPointer& logger);

public:
    [[nodiscard]] static std::shared_ptr<SubscriptionDelegate>
    create(const std::string& sourceId,
           const quicr::Namespace& quicrNamespace,
           const quicr::SubscribeIntent intent,
           const quicr::TransportMode transport_mode,
           const std::string& originUrl,
           const std::string& authToken,
           quicr::bytes e2eToken,
           std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
           const cantina::LoggerPointer& logger);

    std::shared_ptr<SubscriptionDelegate> getptr() { return shared_from_this(); }

    bool isActive() const { return canReceiveSubs; }

    /*===========================================================================*/
    // Events
    /*===========================================================================*/

    virtual void onSubscribeResponse(const quicr::Namespace& quicrNamespace, const quicr::SubscribeResult& result);

    virtual void onSubscriptionEnded(const quicr::Namespace& quicrNamespace,
                                     const quicr::SubscribeResult::SubscribeStatus& result);

    virtual void onSubscribedObject(const quicr::Name& quicrName,
                                    uint8_t priority,
                                    quicr::bytes&& data);

    virtual void
    onSubscribedObjectFragment(const quicr::Name&, uint8_t, const uint64_t&, bool, quicr::bytes&&);

    /*===========================================================================*/
    // Actions
    /*===========================================================================*/

    void subscribe(std::shared_ptr<quicr::Client> quicrClient, const quicr::TransportMode transport_mode);
    void unsubscribe(std::shared_ptr<quicr::Client> quicrClient);

private:
    bool canReceiveSubs;
    std::string sourceId;
    quicr::Namespace quicrNamespace;
    quicr::SubscribeIntent intent;
    std::string originUrl;
    std::string authToken;
    quicr::bytes e2eToken;
    std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate;
    const cantina::LoggerPointer logger;

    std::uint64_t groupCount;
    std::uint64_t objectCount;
    std::uint64_t groupGapCount;
    std::uint64_t objectGapCount;

    std::uint32_t currentGroupId;
    std::uint16_t currentObjectId;

    QSFrameContext sframe_context;
};

class PublicationDelegate : public quicr::PublisherDelegate, public std::enable_shared_from_this<PublicationDelegate>
{
    PublicationDelegate(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                        const std::string& sourceId,
                        const quicr::Namespace& quicrNamespace,
                        const quicr::TransportMode transport_mode,
                        const std::string& originUrl,
                        const std::string& authToken,
                        quicr::bytes&& payload,
                        const std::vector<std::uint8_t>& priority,
                        const std::vector<std::uint16_t>& expiry,
                        const cantina::LoggerPointer& logger);

public:
    [[nodiscard]] static std::shared_ptr<PublicationDelegate>
    create(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
           const std::string& sourceId,
           const quicr::Namespace& quicrNamespace,
           const quicr::TransportMode transport_mode,
           const std::string& originUrl,
           const std::string& authToken,
           quicr::bytes&& payload,
           const std::vector<std::uint8_t>& priority,
           const std::vector<std::uint16_t>& expiry,
           const cantina::LoggerPointer& logger);

    std::shared_ptr<PublicationDelegate> getptr() { return shared_from_this(); }

    /*===========================================================================*/
    // Events
    /*===========================================================================*/

    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                         const quicr::PublishIntentResult& result);

    /*===========================================================================*/
    // Actions
    /*===========================================================================*/

    void publishIntent(std::shared_ptr<quicr::Client> client,
                       quicr::TransportMode transport_mode=quicr::TransportMode::Unreliable);

    void publishIntentEnd(std::shared_ptr<quicr::Client> client);

    void publishNamedObject(std::shared_ptr<quicr::Client> client,
                            const std::uint8_t* data,
                            std::size_t len,
                            bool groupFlag);

private:
    // bool canPublish;
    std::string sourceId;
    const std::string& originUrl;
    const std::string& authToken;

    quicr::Namespace quicrNamespace;
    std::uint32_t groupId;
    std::uint16_t objectId;
    quicr::TransportMode transport_mode { quicr::TransportMode::ReliablePerTrack };
    quicr::bytes&& payload;
    std::vector<std::uint8_t> priority;
    std::vector<std::uint16_t> expiry;

    std::shared_ptr<qmedia::QPublicationDelegate> qDelegate;
    const cantina::LoggerPointer logger;

    QSFrameContext sframe_context;
};
}        // namespace qmedia
