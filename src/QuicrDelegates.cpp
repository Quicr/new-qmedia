#include <qmedia/QuicrDelegates.hpp>
#include <quicr/hex_endec.h>
#include <iostream>

namespace qmedia
{
QuicrTransportSubDelegate::QuicrTransportSubDelegate(const std::string sourceId,
                                                     const quicr::Namespace& quicrNamespace,
                                                     const quicr::SubscribeIntent intent,
                                                     const std::string originUrl,
                                                     const bool useReliableTransport,
                                                     const std::string authToken,
                                                     quicr::bytes e2eToken,
                                                     std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                                     qtransport::LogHandler& logger) :
    canReceiveSubs(true),
    sourceId(sourceId),
    quicrNamespace(quicrNamespace),
    intent(intent),
    originUrl(originUrl),
    useReliableTransport(useReliableTransport),
    authToken(authToken),
    e2eToken(e2eToken),
    qDelegate(qDelegate),
    logger(logger)
{
    logger.log(qtransport::LogLevel::info, "QuicrTransportSubDelegate");
}

/*
 * delegate: onSubscribeResponse
 */
void QuicrTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult& /* result */)
{
    logger.log(qtransport::LogLevel::info, "sub::onSubscribeResponse");
}

/*
 * delegate: onSubscriptionEnded
 */
void QuicrTransportSubDelegate::onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult::SubscribeStatus& /* result */)
{
    logger.log(qtransport::LogLevel::info, "sub::onSubscriptionEnded");
}

/*
 * delegate: onSubscribedObject
 *
 * On receiving subscribed object notification fields are extracted
 * from the quicr::name. These fields along with the notificaiton
 * data are passed to the client callback.
 */
void QuicrTransportSubDelegate::onSubscribedObject(const quicr::Name& /* quicrName */,
                                                   uint8_t /*priority*/,
                                                   uint16_t /*expiry_age_ms*/,
                                                   bool /*use_reliable_transport*/,
                                                   quicr::bytes&& data)
{
    logger.log(qtransport::LogLevel::info, "sub::onSubscribeObject");
    qDelegate->subscribedObject(std::move(data));
}

/*
 * subscribe
 *
 * Use quicrClient to send out a subscription request.
 */

void QuicrTransportSubDelegate::subscribe(std::shared_ptr<QuicrTransportSubDelegate> self,
                                          std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (quicrClient)
    {
        quicrClient->subscribe(
            self, quicrNamespace, intent, originUrl, useReliableTransport, authToken, std::move(e2eToken));
    }
    else
    {
        logger.log(qtransport::LogLevel::error, "Subscribe - quicrCliet doesn't exist");
    }
}

void QuicrTransportSubDelegate::unsubscribe(std::shared_ptr<QuicrTransportSubDelegate> self,
                                            std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (quicrClient)
    {
        quicrClient->unsubscribe(quicrNamespace, originUrl, authToken);
    }
    else
    {
        logger.log(qtransport::LogLevel::error, "Unsubscibe - quicrCliet doesn't exist");
    }
}

/*
 * QuicrTransportPubDelegate::QuicrTransportPubDelegate
 *
 * Delegate constructor.
 */

QuicrTransportPubDelegate::QuicrTransportPubDelegate(std::string sourceId,
                                                     quicr::Namespace quicrNamespace,
                                                     const std::string& originUrl,
                                                     const std::string& authToken,
                                                     quicr::bytes&& payload,
                                                     const std::vector<std::uint8_t> &priority,
                                                     std::uint16_t expiry,
                                                     bool reliableTransport,
                                                     std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                                     qtransport::LogHandler& logger) :
    //canPublish(true),
    sourceId(sourceId),
    quicrNamespace(quicrNamespace),
    originUrl(originUrl),
    authToken(authToken),
    payload(std::move(payload)),
    groupId(0),
    objectId(0),
    priority(priority),
    expiry(expiry),
    reliableTransport(reliableTransport),
    qDelegate(qDelegate),
    logger(logger)
{
    logger.log(qtransport::LogLevel::info, "QuicrTransportPubDelegate");
}

/*
 * delegate: onPublishIntentResponse
 */
void QuicrTransportPubDelegate::onPublishIntentResponse(const quicr::Namespace& /* quicr_namespace */,
                                                        const quicr::PublishIntentResult& /* result */)
{
    logger.log(qtransport::LogLevel::info, "pub::onPublishIntentResponse");
}

void QuicrTransportPubDelegate::publishIntent(std::shared_ptr<QuicrTransportPubDelegate> self,
                                              std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (quicrClient)
    {
        quicrClient->publishIntent(self, quicrNamespace, originUrl, authToken, std::move(payload));
    }
}

void QuicrTransportPubDelegate::publishIntentEnd(std::shared_ptr<QuicrTransportPubDelegate> self,
                                                 std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (quicrClient)
    {
        quicrClient->publishIntentEnd(quicrNamespace, authToken);
    }
}

const quicr::Name object_id_mask = ~(~quicr::Name() << 16);
const quicr::Name group_id_mask = ~(~quicr::Name() << 32) << 16;
void QuicrTransportPubDelegate::publishNamedObject(std::shared_ptr<quicr::QuicRClient> quicrClient,
                                                   std::uint8_t* data,
                                                   std::size_t len,
                                                   bool groupFlag)
{
    if (quicrClient)
    {
        std::uint8_t pri = priority[0];
        quicr::Name quicrName(quicrNamespace.name());
        if (groupFlag)
        {
            quicrName = (0x0_name | groupId) << 16 | (quicrName & ~group_id_mask);
            quicrName = (0x0_name | objectId) | (quicrName & ~object_id_mask);
            ++groupId;
            objectId = 0;
        }
        else
        {
            if (priority.size() > 1) {
                pri = priority[1];
            }
            quicrName = (0x0_name | groupId) << 16 | (quicrName & ~group_id_mask);
            quicrName = (0x0_name | objectId) | (quicrName & ~object_id_mask);
            ++objectId;
        }
        std::cerr << "publish named object " << quicrName.to_hex() << std::endl;
        quicr::bytes b(data, data + len);

        quicrClient->publishNamedObject(quicrName, pri, expiry, reliableTransport, std::move(b));
    }
}

}        // namespace qmedia
