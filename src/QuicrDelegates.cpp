#include <qmedia/QuicrDelegates.hpp>
#include <quicr/hex_endec.h>
#include <iostream>

namespace qmedia
{
QuicrTransportSubDelegate::QuicrTransportSubDelegate(const std::string sourceId,
                                                     const quicr::Namespace& quicrNamespace,
                                                     std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                                     qtransport::LogHandler& logger) :
    canReceiveSubs(false), sourceId(sourceId), quicrNamespace(quicrNamespace), qDelegate(qDelegate), logger(logger)
{
      logger.log(qtransport::LogLevel::info, "QuicrTransportSubDelegate");
}

/*
 * delegate: onSubscribeResponse
 */
void QuicrTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& quicr_namespace,
                                                    const quicr::SubscribeResult& /* result */)
{
    std::cerr << "onSubscibeResponse for " << quicr_namespace << std::endl;
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
void QuicrTransportSubDelegate::onSubscribedObject(const quicr::Name& quicr_name,
                                                   uint8_t /*priority*/,
                                                   uint16_t /*expiry_age_ms*/,
                                                   bool /*use_reliable_transport*/,
                                                   quicr::bytes&& data)
{
    // get timestamp from end of data buffer
    //std::uint64_t timestamp = 0;
    //std::size_t offset = data.size() - sizeof(std::uint64_t);
    //const std::uint8_t* tsbytes = &data[offset];
    //timestamp = *reinterpret_cast<const std::uint64_t*>(tsbytes);

    logger.log(qtransport::LogLevel::info, "sub::onSubscribeObject");
    qDelegate->subscribedObject(std::move(data));
}

/*
 * QuicrTransportPubDelegate::QuicrTransportPubDelegate
 *
 * Delegate constructor.
 */
QuicrTransportPubDelegate::QuicrTransportPubDelegate(std::string sourceId,
                                                     quicr::Namespace quicrNamespace,
                                                     std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                                     qtransport::LogHandler& logger) :
    canPublish(true), sourceId(sourceId), quicrNamespace(quicrNamespace), qDelegate(qDelegate), logger(logger)
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
}        // namespace qmedia
