#include <qmedia/QuicrDelegates.hpp>
#include <quicr/hex_endec.h>
#include <quicr/message_buffer.h>
#include <iostream>
#include <sstream>
#include "sframe/crypto.h"

const quicr::HexEndec<128, 24, 8, 24, 8, 16, 32, 16> delegate_name_format;

constexpr uint64_t Fixed_Epoch = 0xdeadbeefcafebabe;
constexpr uint8_t Quicr_SFrame_Sig_Bits = 96;
constexpr sframe::CipherSuite Default_Cipher_Suite =
    sframe::CipherSuite::AES_GCM_128_SHA256;

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
    sframe_context(Default_Cipher_Suite)
{
    currentGroupId = 0;
    currentObjectId = -1;
    logger.log(qtransport::LogLevel::info, "QuicrTransportSubDelegate");

    // TODO: This needs to be replaced with valid keying material
    std::string salt_string =
        "Quicr epoch master key " + std::to_string(Fixed_Epoch);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    auto epoch_key = hkdf_extract(Default_Cipher_Suite, salt,
        std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.addEpoch(Fixed_Epoch, epoch_key);

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
        auto buf = quicr::messages::MessageBuffer(data);
        auto epoch = quicr::uintVar_t(0);
        buf >> epoch;
        const auto ciphertext = buf.get();
        quicr::bytes output_buffer(ciphertext.size());
        auto cleartext = sframe_context.unprotect(
            epoch,
            quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
            (uint64_t(groupId) << 16) | objectId,
            output_buffer,
            gsl::span{ciphertext.data(), ciphertext.size()});
        output_buffer.resize(cleartext.size());

        qDelegate->subscribedObject(std::move(output_buffer),
                                    groupId,
                                    objectId);
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
    sframe_context(Default_Cipher_Suite)
{
    logger.log(qtransport::LogLevel::info, "QuicrTransportPubDelegate");

    // TODO: This needs to be replaced with valid keying material
    std::string salt_string =
        "Quicr epoch master key " + std::to_string(Fixed_Epoch);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    auto epoch_key = hkdf_extract(Default_Cipher_Suite, salt,
        std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.addEpoch(Fixed_Epoch, epoch_key);

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
            quicr::bytes b(len + 16);
            auto ciphertext = sframe_context.protect(
                quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                (uint64_t(groupId) << 16) | objectId,
                gsl::span(b.data(), b.capacity()),
                gsl::span(data, len));
            b.resize(ciphertext.size());
            auto buf = quicr::messages::MessageBuffer(b.size() + 8);
            buf << quicr::uintVar_t(Fixed_Epoch);
            buf.push(b);

            quicrClient->publishNamedObject(quicrName,
                                            pri,
                                            expiry,
                                            reliableTransport,
                                            buf.get());
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
