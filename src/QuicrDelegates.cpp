#include <qmedia/QuicrDelegates.hpp>
#include <quicr/hex_endec.h>
#include <iostream>
#include <sstream>
#include "quic_varint.h"

const quicr::HexEndec<128, 24, 8, 24, 8, 16, 32, 16> delegate_name_format;

constexpr uint64_t Fake_Key_ID = 0xdeadbeefcafebabe;
constexpr uint8_t Quicr_SFrame_Sig_Bits = 96;

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
    logger(logger),
    sframe_context(sframe::CipherSuite::AES_GCM_128_SHA256)
{
    currentGroupId = 0;
    currentObjectId = -1;
    logger.log(qtransport::LogLevel::info, "QuicrTransportSubDelegate");

    // TODO: This needs to be replaced with valid keying material
    sframe_context.addEpoch(
        0xdeadbeefcafebabe,
        std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.enableEpoch(0xdeadbeefcafebabe);
}

QuicrTransportSubDelegate::~QuicrTransportSubDelegate()
{
    std::cerr << "~QuicrTransportSubDelegate" << std::endl;
    std::stringstream s;
    s << "~QuicrTransportSubDelegate::";
    s << "namespace " << quicrNamespace << ":";
    s << "\tgroup: " << groupCount << " ";
    s << "\tobject: " << objectCount << " ";
    s << "\tgroup gap:" << groupGapCount << " ";
    s << "\tobject gap " << objectGapCount;
    std::cerr << s.str() << std::endl;
    logger.log(qtransport::LogLevel::info, s.str());
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
void QuicrTransportSubDelegate::onSubscribedObject(const quicr::Name& quicrName,
                                                   uint8_t /*priority*/,
                                                   uint16_t /*expiry_age_ms*/,
                                                   bool /*use_reliable_transport*/,
                                                   quicr::bytes&& data)
{
    logger.log(qtransport::LogLevel::info, "sub::onSubscribeObject");
    if (data.empty())
    {
        logger.log(qtransport::LogLevel::warn, "Object is empty");
        return;
    }
    auto [orgId, appId, confId, mediaType, clientId, groupId, objectId] = delegate_name_format.Decode(quicrName);

    // group=5, object=0
    // group=5, object=1
    // group=6, object=2
    // group=7, object=0 <---- group gap
    // group=7, object=4 <---- object gap
    // group=8, object=1 <---- object gap

    if (groupId > currentGroupId) 
    {
        if (groupId > currentGroupId+1)
        {
            ++groupGapCount;
            currentObjectId = 0;
        }
        ++groupCount;
    }

    if (objectId > currentObjectId)
    {
        if (objectId > currentObjectId+1)
        {
            ++objectGapCount;
        }
        ++objectCount;
    }

    currentGroupId = groupId;
    currentObjectId = objectId;

    // Decrypt the received data using sframe
    try
    {
        auto key_id_length = QUICVarIntSize(data.data());
        if (data.size() <= key_id_length)
        {
            logger.log(qtransport::LogLevel::error,
                       "Received an object with a corrupt key ID");
            return;
        }
        auto sframe_key_id = QUICVarIntDecode(data.data());
        if (sframe_key_id == std::numeric_limits<std::uint64_t>::max())
        {
            logger.log(qtransport::LogLevel::error,
                       "Received an object with an invalid key ID");
            return;
        }
        quicr::bytes output_buffer(data.size() - key_id_length);
        auto cleartext = sframe_context.unprotect(
            sframe_key_id,
            quicr::Namespace(quicrNamespace.name(), Quicr_SFrame_Sig_Bits),
            uint64_t(groupId << 16) | objectId,
            output_buffer,
            gsl::span{data.data() + key_id_length,
                      data.size() - key_id_length});
        output_buffer.resize(cleartext.size());

        qDelegate->subscribedObject(std::move(output_buffer), groupId, objectId);
    }
    catch (const std::exception &e)
    {
        logger.log(qtransport::LogLevel::error,
                   std::string("Exception trying to decrypt with sframe and "
                               "forward object: ") +
                       e.what());
    }
    catch (...)
    {
        logger.log(qtransport::LogLevel::error,
                    "Exception trying to encrypt sframe and forward object");
    }
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

void QuicrTransportSubDelegate::unsubscribe(std::shared_ptr<QuicrTransportSubDelegate> /*self*/,
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
    groupId(1),
    objectId(0),
    priority(priority),
    expiry(expiry),
    reliableTransport(reliableTransport),
    qDelegate(qDelegate),
    logger(logger),
    sframe_context(sframe::CipherSuite::AES_GCM_128_SHA256)
{
    logger.log(qtransport::LogLevel::info, "QuicrTransportPubDelegate");

    // TODO: This needs to be replaced with valid keying material
    sframe_context.addEpoch(
        0xdeadbeefcafebabe,
        std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.enableEpoch(0xdeadbeefcafebabe);
}

QuicrTransportPubDelegate::~QuicrTransportPubDelegate()
{
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

void QuicrTransportPubDelegate::publishIntentEnd(std::shared_ptr<QuicrTransportPubDelegate> /*self*/,
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
    // If the object data isn't present, return
    if (len == 0)
    {
        logger.log(qtransport::LogLevel::warn, "Object is empty");
        return;
    }

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

        // Encrypt using sframe
        try
        {
            auto key_id_length = QUICVarIntSize(Fake_Key_ID);
            quicr::bytes b(key_id_length + len + 16);
            QUICVarIntEncode(Fake_Key_ID, b.data());
            auto ciphertext = sframe_context.protect(
                quicr::Namespace(quicrNamespace.name(), Quicr_SFrame_Sig_Bits),
                uint64_t(groupId << 16) | objectId,
                gsl::span(b.data() + key_id_length, len + 16),
                gsl::span(data, len));
            b.resize(key_id_length + ciphertext.size());

            quicrClient->publishNamedObject(quicrName,
                                            pri,
                                            expiry,
                                            reliableTransport,
                                            std::move(b));
        }
        catch (const std::exception &e)
        {
            logger.log(qtransport::LogLevel::error,
                       std::string("Exception trying to encrypt sframe and "
                                   "pubblish: ") +
                           e.what());
            return;
        }
        catch (...)
        {
            logger.log(qtransport::LogLevel::error,
                       "Exception trying to encrypt sframe and publish");
            return;
        }
    }
}

}        // namespace qmedia
