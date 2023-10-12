#include "qmedia/QuicrDelegates.hpp"
#include <cantina/logger.h>

#include <quicr/hex_endec.h>
#include <quicr/message_buffer.h>
#include <sframe/crypto.h>

#include <iostream>
#include <sstream>
#include <ctime>

constexpr quicr::Name Group_ID_Mask = ~(~0x0_name << 32) << 16;
constexpr quicr::Name Object_ID_Mask = ~(~0x0_name << 16);

constexpr uint64_t Fixed_Epoch = 1;
constexpr uint8_t Quicr_SFrame_Sig_Bits = 80;
constexpr sframe::CipherSuite Default_Cipher_Suite = sframe::CipherSuite::AES_GCM_128_SHA256;

namespace qmedia
{
SubscriptionDelegate::SubscriptionDelegate(const quicr::Namespace& quicrNamespace,
                                           const cantina::LoggerPointer& logger) :
    logger(std::make_shared<cantina::Logger>("SDEL", logger)),
    quicrNamespace(quicrNamespace),
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
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    sframe_context.addEpoch(Fixed_Epoch, epoch_key);

    sframe_context.enableEpoch(Fixed_Epoch);
}

void SubscriptionDelegate::onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                                               const quicr::SubscribeResult& /* result */)
{
    // LOGGER_DEBUG(logger, __FUNCTION__);
}

void SubscriptionDelegate::onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                                               const quicr::SubscribeResult::SubscribeStatus& /* result */)
{
    LOGGER_DEBUG(logger, __FUNCTION__);
}

/**
 * delegate: onSubscribedObject
 *
 * On receiving subscribed object notification fields are extracted
 * from the quicr::name. These fields along with the notification
 * data are passed to the client callback.
 */
void SubscriptionDelegate::onSubscribedObject(const quicr::Name& quicrName,
                                              uint8_t /*priority*/,
                                              uint16_t /*expiry_age_ms*/,
                                              bool /*use_reliable_transport*/,
                                              quicr::bytes&& data)
{
    if (data.empty())
    {
        LOGGER_WARNING(logger, "Object " << quicrName << " is empty");
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
                                                  quicrName.bits<std::uint64_t>(0, 48),
                                                  output_buffer,
                                                  ciphertext);
        output_buffer.resize(cleartext.size());

        subscribedObject(std::move(output_buffer), groupId, objectId);
    }
    catch (const std::exception& e)
    {
        LOGGER_ERROR(logger, "Exception trying to decrypt with sframe and forward object: " << e.what());
    }
    catch (const std::string& s)
    {
        LOGGER_ERROR(logger, "Exception trying to decrypt sframe and forward object: " << s);
    }
    catch (...)
    {
        LOGGER_ERROR(logger, "Unknown error trying to decrypt sframe and forward object");
    }
}

void SubscriptionDelegate::onSubscribedObjectFragment(const quicr::Name&,
                                                      uint8_t,
                                                      uint16_t,
                                                      bool,
                                                      const uint64_t&,
                                                      bool,
                                                      quicr::bytes&&)
{
}

void SubscriptionDelegate::subscribe(std::shared_ptr<quicr::Client> client,
                                     const quicr::SubscribeIntent intent,
                                     const std::string& originUrl,
                                     const bool useReliableTransport,
                                     const std::string& authToken,
                                     quicr::bytes&& e2eToken)
{
    if (!client)
    {
        LOGGER_ERROR(logger, "Subscribe - client doesn't exist");
        return;
    }

    client->subscribe(
        shared_from_this(), quicrNamespace, intent, originUrl, useReliableTransport, authToken, std::move(e2eToken));
}

void SubscriptionDelegate::unsubscribe(std::shared_ptr<quicr::Client> client,
                                       const std::string& originUrl,
                                       const std::string& authToken)
{
    if (!client)
    {
        LOGGER_ERROR(logger, "Unsubscribe - client doesn't exist");
        return;
    }

    client->unsubscribe(quicrNamespace, originUrl, authToken);
}

/*
 * QuicrTransportPubDelegate::QuicrTransportPubDelegate
 *
 * Delegate constructor.
 */

PublicationDelegate::PublicationDelegate(const std::string& sourceId,
                                         const quicr::Namespace& quicrNamespace,
                                         const std::vector<std::uint8_t>& priorities,
                                         std::uint16_t expiry,
                                         const cantina::LoggerPointer& logger) :
    logger(std::make_shared<cantina::Logger>("PDEL", logger)),
    quicrNamespace(quicrNamespace),
    sourceId(sourceId),
    groupId(time(nullptr)),        // TODO: Multiply by packet count.
    objectId(0),
    expiry(expiry),
    priorities(priorities),
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
}

void PublicationDelegate::setClientSession(std::shared_ptr<quicr::Client> client)
{
    client_session = std::move(client);
}

void PublicationDelegate::onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                                  const quicr::PublishIntentResult& result)
{
    LOGGER_INFO(logger,
                "Received PublishIntent response for " << quicr_namespace << ": " << static_cast<int>(result.status));
}

void PublicationDelegate::publishIntent(bool reliableTransport,
                                        quicr::bytes&& payload)
{
    auto client = client_session.lock();
    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't send PublishIntent");
        return;
    }

    LOGGER_DEBUG(logger, "Sending PublishIntent for " << quicrNamespace << "...");
    bool success = client->publishIntent(
        shared_from_this(), quicrNamespace, "", "", std::move(payload), reliableTransport);

    if (!success)
    {
        LOGGER_ERROR(logger, "Failed to send PublishIntent for " << quicrNamespace);
        return;
    }

    LOGGER_INFO(logger, "Sent PublishIntent for " << quicrNamespace);
}

void PublicationDelegate::publishIntentEnd(const std::string& authToken)
{
    auto client = client_session.lock();
    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't send PublishIntentEnd");
        return;
    }

    client->publishIntentEnd(quicrNamespace, authToken);
}

void PublicationDelegate::publishNamedObject(std::uint8_t* data,
                                             std::size_t len,
                                             bool groupFlag,
                                             bool reliableTransport)
{
    // If the object data isn't present, return
    if (len == 0)
    {
        LOGGER_WARNING(logger, "Cannot send empty object");
        return;
    }

    auto client = client_session.lock();
    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't Publish");
        return;
    }

    std::uint8_t pri = priorities[0];
    quicr::Name quicrName(quicrNamespace.name());

    if (groupFlag)
    {
        quicrName = (0x0_name | ++groupId) << 16 | (quicrName & ~Group_ID_Mask);
        quicrName &= ~Object_ID_Mask;
        objectId = 0;
    }
    else
    {
        if (priorities.size() > 1) pri = priorities[1];

        quicrName = (0x0_name | groupId) << 16 | (quicrName & ~Group_ID_Mask);
        quicrName = (0x0_name | ++objectId) | (quicrName & ~Object_ID_Mask);
    }

    // Encrypt using sframe
    try
    {
        quicr::bytes output_buffer(len + 16);
        auto ciphertext = sframe_context.protect(quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                                                 quicrName.bits<std::uint64_t>(0, 48),
                                                 output_buffer,
                                                 {data, len});
        output_buffer.resize(ciphertext.size());
        auto buf = quicr::messages::MessageBuffer(output_buffer.size() + 8);
        buf << quicr::uintVar_t(Fixed_Epoch);
        buf.push(std::move(output_buffer));

        client->publishNamedObject(quicrName, pri, expiry, reliableTransport, buf.take());
    }
    catch (const std::exception& e)
    {
        LOGGER_ERROR(logger, "Exception trying to encrypt sframe and publish: " << e.what());
        return;
    }
    catch (const std::string& s)
    {
        LOGGER_ERROR(logger, "Exception trying to encrypt sframe and publish: " << s);
        return;
    }
    catch (...)
    {
        LOGGER_ERROR(logger, "Unknown error trying to encrypt sframe and publish");
        return;
    }
}
}        // namespace qmedia
