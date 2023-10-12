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
public:
    SubscriptionDelegate(const quicr::Namespace& quicrNamespace, const cantina::LoggerPointer& logger);

    virtual ~SubscriptionDelegate() = default;

    bool isActive() const { return canReceiveSubs; }

    /*===========================================================================*/
    // Interface
    /*===========================================================================*/

    virtual int prepare(const std::string& label, const std::string& qualityProfile, bool& reliable) = 0;
    virtual int update(const std::string& label, const std::string& qualityProfile) = 0;
    virtual int subscribedObject(quicr::bytes&& data, std::uint32_t groupId, std::uint16_t objectId) = 0;

    /*===========================================================================*/
    // Events
    /*===========================================================================*/

    virtual void onSubscribeResponse(const quicr::Namespace& quicrNamespace, const quicr::SubscribeResult& result);

    virtual void onSubscriptionEnded(const quicr::Namespace& quicrNamespace,
                                     const quicr::SubscribeResult::SubscribeStatus& result);

    virtual void onSubscribedObject(const quicr::Name& quicrName,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    quicr::bytes&& data);

    virtual void
    onSubscribedObjectFragment(const quicr::Name&, uint8_t, uint16_t, bool, const uint64_t&, bool, quicr::bytes&&);

    /*===========================================================================*/
    // Actions
    /*===========================================================================*/

    void subscribe(std::shared_ptr<quicr::Client> quicrClient,
                   const quicr::SubscribeIntent intent,
                   const std::string& originUrl,
                   const bool useReliableTransport,
                   const std::string& authToken,
                   quicr::bytes&& e2eToken);

    void unsubscribe(std::shared_ptr<quicr::Client> quicrClient,
                     const std::string& originUrl,
                     const std::string& authToken);

protected:
    const cantina::LoggerPointer logger;
    quicr::Namespace quicrNamespace;

private:
    bool canReceiveSubs;

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
protected:
    PublicationDelegate(const std::string& sourceId,
                        const quicr::Namespace& quicrNamespace,
                        const std::vector<std::uint8_t>& priorities,
                        std::uint16_t expiry,
                        const cantina::LoggerPointer& logger);

public:
    virtual ~PublicationDelegate() = default;

    void setClientSession(std::shared_ptr<quicr::Client> client);

    /*===========================================================================*/
    // Interface
    /*===========================================================================*/

    virtual int prepare(const std::string& qualityProfile, bool& reliable) = 0;
    virtual int update(const std::string& qualityProfile) = 0;
    virtual void publish(bool) = 0;

    /*===========================================================================*/
    // Events
    /*===========================================================================*/

    virtual void onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                         const quicr::PublishIntentResult& result);

    /*===========================================================================*/
    // Actions
    /*===========================================================================*/

    void publishIntent(bool reliableTransport, quicr::bytes&& payload);
    void publishIntentEnd(const std::string& authToken);
    void publishNamedObject(std::uint8_t* data,
                            std::size_t len,
                            bool groupFlag,
                            bool reliableTransport);

protected:
    const cantina::LoggerPointer logger;
    quicr::Namespace quicrNamespace;
    std::weak_ptr<quicr::Client> client_session;

private:
    std::string sourceId;
    std::vector<std::uint8_t> priorities;
    std::uint16_t expiry;

    std::uint32_t groupId;
    std::uint16_t objectId;

    QSFrameContext sframe_context;
};
}        // namespace qmedia
