#include "qmedia/QuicrDelegates.hpp"
#include "basicLogger.h"

#include <quicr/hex_endec.h>
#include <quicr/message_buffer.h>
#include <sframe/crypto.h>

#include <iostream>
#include <sstream>
#include <ctime>

const quicr::Name group_id_mask = ~(~0x0_name << 32) << 16;
const quicr::Name object_id_mask = ~(~0x0_name << 16);

constexpr uint64_t Fixed_Epoch = 1;
constexpr uint8_t Quicr_SFrame_Sig_Bits = 80;
constexpr sframe::CipherSuite Default_Cipher_Suite = sframe::CipherSuite::AES_GCM_128_SHA256;

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

    // TODO: This needs to be replaced with valid keying material
    std::string salt_string = "Quicr epoch master key " + std::to_string(Fixed_Epoch);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    auto epoch_key = hkdf_extract(
        Default_Cipher_Suite,
        salt,
        std::vector<std::uint8_t>{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.addEpoch(Fixed_Epoch, epoch_key);

    sframe_context.enableEpoch(Fixed_Epoch);

    LOG_DEBUG(logger, "QuicrTransportSubDelegate");
}

QuicrTransportSubDelegate::~QuicrTransportSubDelegate()
{
    LOG_DEBUG(logger, "~QuicrTransportSubDelegate:");
    LOG_DEBUG(logger, "\tnamespace: " << quicrNamespace);
    LOG_DEBUG(logger, "\tgroup: " << groupCount);
    LOG_DEBUG(logger, "\tobject: " << objectCount);
    LOG_DEBUG(logger, "\tgroup gap:" << groupGapCount);
    LOG_DEBUG(logger, "\tobject gap " << objectGapCount);
}

/**
 * delegate: onSubscribeResponse
 */
void QuicrTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult& /* result */)
{
    // LOG_DEBUG(logger, __FUNCTION__);
}

/**
 * delegate: onSubscriptionEnded
 */
void QuicrTransportSubDelegate::onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult::SubscribeStatus& /* result */)
{
    LOG_DEBUG(logger, __FUNCTION__);
}

/**
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
    // LOG_DEBUG(logger, __FUNCTION__);
    if (data.empty())
    {
        LOG_WARNING(logger, "Object " << quicrName << " is empty");
        return;
    }

    const auto groupId = quicrName.bits<std::uint32_t>(16, 32);
    const auto objectId = quicrName.bits<std::uint16_t>(0, 16);

    // group=5, object=0
    // group=5, object=1
    // group=6, object=2
    // group=7, object=0 <---- group gap
    // group=7, object=4 <---- object gap
    // group=8, object=1 <---- object gap

    if (groupId > currentGroupId)
    {
        if (groupId > currentGroupId + 1)
        {
            ++groupGapCount;
            currentObjectId = 0;
        }
        ++groupCount;
    }

    if (objectId > currentObjectId)
    {
        if (objectId > currentObjectId + 1)
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
        quicr::uintVar_t epoch;
        buf >> epoch;
        const auto ciphertext = buf.take();
        quicr::bytes output_buffer(ciphertext.size());
        auto cleartext = sframe_context.unprotect(epoch,
                                                  quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                                                  quicrName.bits<std::uint16_t>(0, 48),
                                                  output_buffer,
                                                  ciphertext);
        output_buffer.resize(cleartext.size());

        qDelegate->subscribedObject(std::move(output_buffer), groupId, objectId);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(logger, "Exception trying to decrypt with sframe and forward object: " << e.what());
    }
    catch (const std::string& s)
    {
        LOG_ERROR(logger, "Exception trying to decrypt sframe and forward object: " << s);
    }
    catch (...)
    {
        LOG_ERROR(logger, "Unknown error trying to decrypt sframe and forward object");
    }
}

/**
 * subscribe
 *
 * Use quicrClient to send out a subscription request.
 */

void QuicrTransportSubDelegate::subscribe(std::shared_ptr<QuicrTransportSubDelegate> self,
                                          std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (!quicrClient)
    {
        LOG_ERROR(logger, "Subscribe - quicrClient doesn't exist");
        return;
    }

    quicrClient->subscribe(
        self, quicrNamespace, intent, originUrl, useReliableTransport, authToken, std::move(e2eToken));
}

void QuicrTransportSubDelegate::unsubscribe(std::shared_ptr<QuicrTransportSubDelegate> /*self*/,
                                            std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (!quicrClient)
    {
        LOG_ERROR(logger, "Unsubscibe - quicrClient doesn't exist");
        return;
    }

    quicrClient->unsubscribe(quicrNamespace, originUrl, authToken);
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
                                                     const std::vector<std::uint8_t>& priority,
                                                     std::uint16_t expiry,
                                                     bool reliableTransport,
                                                     std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                                     qtransport::LogHandler& logger) :
    // canPublish(true),
    sourceId(sourceId),
    quicrNamespace(quicrNamespace),
    originUrl(originUrl),
    authToken(authToken),
    payload(std::move(payload)),
    groupId(time(nullptr)),        // TODO: Multiply by packet count.
    objectId(0),
    priority(priority),
    expiry(expiry),
    reliableTransport(reliableTransport),
    qDelegate(qDelegate),
    logger(logger),
    sframe_context(Default_Cipher_Suite)
{
    // TODO: This needs to be replaced with valid keying material
    std::string salt_string = "Quicr epoch master key " + std::to_string(Fixed_Epoch);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    auto epoch_key = hkdf_extract(
        Default_Cipher_Suite,
        salt,
        std::vector<std::uint8_t>{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.addEpoch(Fixed_Epoch, epoch_key);

    sframe_context.enableEpoch(Fixed_Epoch);

    LOG_DEBUG(logger, "QuicrTransportPubDelegate");
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
    LOG_INFO(logger, __FUNCTION__);
}

void QuicrTransportPubDelegate::publishIntent(std::shared_ptr<QuicrTransportPubDelegate> self,
                                              std::shared_ptr<quicr::QuicRClient> quicrClient,
                                              bool reliableTransport)
{
    if (!quicrClient)
    {
        LOG_DEBUG(logger, "Client was null, can't send PublishIntent");
        return;
    }

    LOG_DEBUG(logger, "Sending PublishIntent for " << quicrNamespace << "...");
    auto result = quicrClient->publishIntent(self, quicrNamespace, originUrl, authToken,
                                             std::move(payload), reliableTransport);
    if (!result)
        LOG_ERROR(logger, "Failed to send PublishIntent for " << quicrNamespace);
    else
        LOG_INFO(logger, "Sent PublishIntent for " << quicrNamespace);
}

void QuicrTransportPubDelegate::publishIntentEnd(std::shared_ptr<QuicrTransportPubDelegate> /*self*/,
                                                 std::shared_ptr<quicr::QuicRClient> quicrClient)
{
    if (!quicrClient)
    {
        LOG_DEBUG(logger, "Client was null, can't send PublishIntentEnd");
        return;
    }

    quicrClient->publishIntentEnd(quicrNamespace, authToken);
}

void QuicrTransportPubDelegate::publishNamedObject(std::shared_ptr<quicr::QuicRClient> quicrClient,
                                                   std::uint8_t* data,
                                                   std::size_t len,
                                                   bool groupFlag)
{
    // If the object data isn't present, return
    if (len == 0)
    {
        LOG_WARNING(logger, "Object is empty");
        return;
    }

    if (!quicrClient) return;

    std::uint8_t pri = priority[0];
    quicr::Name quicrName(quicrNamespace.name());

    if (groupFlag)
    {
        quicrName = (0x0_name | ++groupId) << 16 | (quicrName & ~group_id_mask);
        quicrName &= ~object_id_mask;
        objectId = 0;
    }
    else
    {
        if (priority.size() > 1)
        {
            pri = priority[1];
        }
        quicrName = (0x0_name | groupId) << 16 | (quicrName & ~group_id_mask);
        quicrName = (0x0_name | ++objectId) | (quicrName & ~object_id_mask);
    }

    // Encrypt using sframe
    try
    {
        quicr::bytes output_buffer(len + 16);
        auto ciphertext = sframe_context.protect(quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                                                 quicrName.bits<std::uint16_t>(0, 48),
                                                 output_buffer,
                                                 {data, len});
        output_buffer.resize(ciphertext.size());
        auto buf = quicr::messages::MessageBuffer(output_buffer.size() + 8);
        buf << quicr::uintVar_t(Fixed_Epoch);
        buf.push(std::move(output_buffer));

        quicrClient->publishNamedObject(quicrName, pri, expiry, reliableTransport, buf.take());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(logger, "Exception trying to encrypt sframe and publish: " << e.what());
        return;
    }
    catch (const std::string& s)
    {
        LOG_ERROR(logger, "Exception trying to decrypt sframe and forward object: " << s);
    }
    catch (...)
    {
        LOG_ERROR(logger, "Unknown error trying to encrypt sframe and publish");
        return;
    }
}
}        // namespace qmedia
