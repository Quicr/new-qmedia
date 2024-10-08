#include "qmedia/QuicrDelegates.hpp"
#include <spdlog/spdlog.h>

#include <quicr/hex_endec.h>
#include <quicr/message_buffer.h>
#include <sframe/crypto.h>

#include <iostream>
#include <sstream>
#include <ctime>

#define LOGGER_TRACE(logger, ...) if (logger) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__)
#define LOGGER_DEBUG(logger, ...) if (logger) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__)
#define LOGGER_INFO(logger, ...) if (logger) SPDLOG_LOGGER_INFO(logger, __VA_ARGS__)
#define LOGGER_WARN(logger, ...) if (logger) SPDLOG_LOGGER_WARN(logger, __VA_ARGS__)
#define LOGGER_ERROR(logger, ...) if (logger) SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__)
#define LOGGER_CRITICAL(logger, ...) if (logger) SPDLOG_LOGGER_CRITICAL(logger, __VA_ARGS__)

constexpr quicr::Name Group_ID_Mask = ~(~0x0_name << 32) << 16;
constexpr quicr::Name Object_ID_Mask = ~(~0x0_name << 16);

constexpr uint64_t Fixed_Epoch = 1;
constexpr uint8_t Quicr_SFrame_Sig_Bits = 80;

namespace qmedia
{
SubscriptionDelegate::SubscriptionDelegate(const std::string& sourceId,
                                           const quicr::Namespace& quicrNamespace,
                                           const quicr::SubscribeIntent intent,
                                           [[maybe_unused]] const quicr::TransportMode transport_mode,
                                           const std::string& originUrl,
                                           const std::string& authToken,
                                           quicr::bytes e2eToken,
                                           std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                                           std::shared_ptr<spdlog::logger> logger,
                                           std::optional<sframe::CipherSuite> cipherSuite) :
    canReceiveSubs(true),
    sourceId(sourceId),
    quicrNamespace(quicrNamespace),
    intent(intent),
    originUrl(originUrl),
    authToken(authToken),
    e2eToken(e2eToken),
    qDelegate(std::move(qDelegate)),
    logger(std::move(logger)),
    sframe_context(cipherSuite ? std::optional<QSFrameContext>(*cipherSuite) : std::nullopt)
{
    currentGroupId = 0;
    currentObjectId = -1;

    if (sframe_context)
    {
        // TODO: This needs to be replaced with valid keying material
        sframe::CipherSuite suite = *cipherSuite;
        std::string salt_string = "Quicr epoch master key " + std::to_string(Fixed_Epoch);
        sframe::bytes salt(salt_string.begin(), salt_string.end());
        auto epoch_key = hkdf_extract(
            suite,
            salt,
            {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
        sframe_context->addEpoch(Fixed_Epoch, epoch_key);
        sframe_context->enableEpoch(Fixed_Epoch);
    }
    else
    {
        LOGGER_WARN(logger, "[{0}] This subscription will not attempt to encrypt data", std::string(quicrNamespace));
    }
}

std::shared_ptr<SubscriptionDelegate>
SubscriptionDelegate::create(const std::string& sourceId,
                             const quicr::Namespace& quicrNamespace,
                             const quicr::SubscribeIntent intent,
                             const quicr::TransportMode transport_mode,
                             const std::string& originUrl,
                             const std::string& authToken,
                             quicr::bytes e2eToken,
                             std::shared_ptr<qmedia::QSubscriptionDelegate> qDelegate,
                             std::shared_ptr<spdlog::logger> logger,
                             std::optional<sframe::CipherSuite> cipherSuite)
{

    return std::shared_ptr<SubscriptionDelegate>(new SubscriptionDelegate(sourceId,
                                                                          quicrNamespace,
                                                                          intent,
                                                                          transport_mode,
                                                                          originUrl,
                                                                          authToken,
                                                                          e2eToken,
                                                                          std::move(qDelegate),
                                                                          std::move(logger),
                                                                          cipherSuite));
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
 * from the quicr::name. These fields along with the notificaiton
 * data are passed to the client callback.
 */
void SubscriptionDelegate::onSubscribedObject(const quicr::Name& quicrName,
                                              uint8_t /*priority*/,
                                              quicr::bytes&& data)
{
    // LOGGER_DEBUG(logger, __FUNCTION__);
    if (data.empty())
    {
        LOGGER_WARN(logger, "Object {0} is empty", std::string(quicrName));
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

    quicr::bytes output_buffer;
    if (sframe_context)
    {
        // Decrypt the received data using sframe
        try
        {
            auto buf = quicr::messages::MessageBuffer(data);
            quicr::uintVar_t epoch;
            buf >> epoch;
            const auto ciphertext = buf.take();
            output_buffer = quicr::bytes(ciphertext.size());
            auto cleartext = sframe_context->unprotect(epoch,
                                                       quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                                                        quicrName.bits<std::uint64_t>(0, 48),
                                                        output_buffer,
                                                        ciphertext);
            output_buffer.resize(cleartext.size());
        }
        catch (const std::exception& e)
        {
            LOGGER_ERROR(logger, "Exception trying to decrypt sframe: {0}", e.what());
            return;
        }
        catch (const std::string& s)
        {
            LOGGER_ERROR(logger, "Exception trying to decrypt sframe: {0}", s);
            return;
        }
        catch (...)
        {
            LOGGER_ERROR(logger, "Unknown error trying to decrypt sframe");
            return;
        }
    }
    else
    {
        output_buffer = std::move(data);
    }

    // Forward the object on.
    try
    {
        qDelegate->subscribedObject(this->quicrNamespace, std::move(output_buffer), groupId, objectId);
    }
    catch (const std::exception& e)
    {
        LOGGER_ERROR(logger, "Exception trying to forward object: {0}", e.what());
    }
    catch (const std::string& s)
    {
        LOGGER_ERROR(logger, "Exception trying to forward object: {0}", s);
    }
    catch (...)
    {
        LOGGER_ERROR(logger, "Unknown error trying to forward object");
    }
}

void SubscriptionDelegate::onSubscribedObjectFragment(const quicr::Name&, uint8_t, const uint64_t&, bool, quicr::bytes&&)
{
}

void SubscriptionDelegate::subscribe(std::shared_ptr<quicr::Client> client, const quicr::TransportMode transport_mode)
{
    if (!client)
    {
        LOGGER_ERROR(logger, "Subscribe - client doesn't exist");
        return;
    }

    client->subscribe(
        shared_from_this(), quicrNamespace, intent, transport_mode,
        originUrl, authToken, std::move(e2eToken));
}

void SubscriptionDelegate::unsubscribe(std::shared_ptr<quicr::Client> client)
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

PublicationDelegate::PublicationDelegate(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                         const std::string& sourceId,
                                         const quicr::Namespace& quicrNamespace,
                                         const quicr::TransportMode transport_mode,
                                         const std::string& originUrl,
                                         const std::string& authToken,
                                         quicr::bytes&& payload,
                                         const std::vector<std::uint8_t>& priority,
                                         const std::vector<std::uint16_t>& expiry,
                                         std::shared_ptr<spdlog::logger> logger,
                                         const std::optional<sframe::CipherSuite> cipherSuite) :
    // canPublish(true),
    sourceId(sourceId),
    originUrl(originUrl),
    authToken(authToken),
    quicrNamespace(quicrNamespace),
    groupId(time(nullptr)),        // TODO: Multiply by packet count.
    objectId(0),
    transport_mode(transport_mode),
    payload(std::move(payload)),
    priority(priority),
    expiry(expiry),
    qDelegate(std::move(qDelegate)),
    logger(std::move(logger)),
    sframe_context(cipherSuite ? std::optional<QSFrameContext>(*cipherSuite) : std::nullopt)
{
    if (sframe_context) {
        // TODO: This needs to be replaced with valid keying material
        std::string salt_string = "Quicr epoch master key " + std::to_string(Fixed_Epoch);
        sframe::bytes salt(salt_string.begin(), salt_string.end());
        auto epoch_key = hkdf_extract(
            *cipherSuite,
            salt,
            std::vector<std::uint8_t>{
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
        sframe_context->addEpoch(Fixed_Epoch, epoch_key);
        sframe_context->enableEpoch(Fixed_Epoch);
    } else {
        LOGGER_WARN(logger, "[{0}] This publication will not attempt to encrypt data", std::string(quicrNamespace));
    }

    // Publish named object and intent require priorities. Set defaults if missing
    if (this->priority.empty()) {
        this->priority = { 10, 11 };

    } else if (this->priority.size() == 1) {
        this->priority.emplace_back(priority[0]);
    }
}
std::shared_ptr<PublicationDelegate> PublicationDelegate::create(std::shared_ptr<qmedia::QPublicationDelegate> qDelegate,
                                                                 const std::string& sourceId,
                                                                 const quicr::Namespace& quicrNamespace,
                                                                 const quicr::TransportMode transport_mode,
                                                                 const std::string& originUrl,
                                                                 const std::string& authToken,
                                                                 quicr::bytes&& payload,
                                                                 const std::vector<std::uint8_t>& priority,
                                                                 const std::vector<std::uint16_t>& expiry,
                                                                 std::shared_ptr<spdlog::logger> logger,
                                                                 const std::optional<sframe::CipherSuite> cipherSuite)
{
    return std::shared_ptr<PublicationDelegate>(new PublicationDelegate(std::move(qDelegate),
                                                                        sourceId,
                                                                        quicrNamespace,
                                                                        transport_mode,
                                                                        originUrl,
                                                                        authToken,
                                                                        std::move(payload),
                                                                        priority,
                                                                        expiry,
                                                                        logger,
                                                                        cipherSuite));
}

void PublicationDelegate::onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                                  const quicr::PublishIntentResult& result)
{
    LOGGER_INFO(logger, "Received PublishIntent response for {0}: {1}", std::string(quicr_namespace), static_cast<int>(result.status));
}

void PublicationDelegate::publishIntent(std::shared_ptr<quicr::Client> client,
                                        const quicr::TransportMode transport_mode)
{
    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't send PublishIntent");
        return;
    }

    LOGGER_DEBUG(logger, "Sending PublishIntent for {0}...", std::string(quicrNamespace));
    bool success = client->publishIntent(
        shared_from_this(), quicrNamespace, originUrl,
        authToken, std::move(payload), transport_mode, priority[0]);

    if (!success)
    {
        LOGGER_ERROR(logger, "Failed to send PublishIntent for {0}", std::string(quicrNamespace));
        return;
    }

    LOGGER_INFO(logger, "Sent PublishIntent for {0}", std::string(quicrNamespace));
}

void PublicationDelegate::publishIntentEnd(std::shared_ptr<quicr::Client> client)
{
    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't send PublishIntentEnd");
        return;
    }

    client->publishIntentEnd(quicrNamespace, authToken);
}

void PublicationDelegate::publishNamedObject(std::shared_ptr<quicr::Client> client,
                                             const std::uint8_t* data,
                                             std::size_t len,
                                             bool groupFlag,
                                             std::vector<qtransport::MethodTraceItem> &&trace)
{
    // If the object data isn't present, return
    if (len == 0)
    {
        LOGGER_WARN(logger, "Cannot send empty object");
        return;
    }

    if (!client)
    {
        LOGGER_ERROR(logger, "Client was null, can't Publish");
        return;
    }

    std::uint8_t pri = priority[0];
    std::uint16_t exp = expiry[0];
    quicr::Name quicrName(quicrNamespace.name());

    if (groupFlag)
    {
        quicrName = (0x0_name | ++groupId) << 16 | (quicrName & ~Group_ID_Mask);
        quicrName &= ~Object_ID_Mask;
        objectId = 0;
    }
    else
    {
        if (priority.size() > 1) pri = priority[1];
        if (expiry.size() > 1) exp = expiry[1];

        quicrName = (0x0_name | groupId) << 16 | (quicrName & ~Group_ID_Mask);
        quicrName = (0x0_name | ++objectId) | (quicrName & ~Object_ID_Mask);
    }

    quicr::bytes to_publish;
    if (sframe_context)
    {
        // Encrypt using sframe
        try
        {
            trace.push_back({"qMediaDelegate:publishNamedObject:beforeEncrypt", trace.front().start_time});
            quicr::bytes output_buffer(len + 16);
            auto ciphertext = sframe_context->protect(quicr::Namespace(quicrName, Quicr_SFrame_Sig_Bits),
                                                      quicrName.bits<std::uint64_t>(0, 48),
                                                      output_buffer,
                                                      {data, len});
            output_buffer.resize(ciphertext.size());
            auto buf = quicr::messages::MessageBuffer(output_buffer.size() + 8);
            buf << quicr::uintVar_t(Fixed_Epoch);
            buf.push(std::move(output_buffer));
            trace.push_back({"qMediaDelegate:publishNamedObject:afterEncrypt", trace.front().start_time});
            to_publish = buf.take();
        }
        catch (const std::exception& e)
        {
            LOGGER_ERROR(logger, "Exception trying to encrypt: {0}", e.what());
            return;
        }
        catch (const std::string& s)
        {
            LOGGER_ERROR(logger, "Exception trying to encrypt: {0}", s);
            return;
        }
        catch (...)
        {
            LOGGER_ERROR(logger, "Unknown error trying to encrypt");
            return;
        }
    }
    else
    {
        to_publish = quicr::bytes(data, data + len);
    }

    try
    {
        client->publishNamedObject(quicrName, pri, exp, std::move(to_publish), std::move(trace));
    }
    catch (const std::exception& e)
    {
        LOGGER_ERROR(logger, "Exception trying to publish: {0}", e.what());
        return;
    }
    catch (const std::string& s)
    {
        LOGGER_ERROR(logger, "Exception trying to publish: {0}", s);
        return;
    }
    catch (...)
    {
        LOGGER_ERROR(logger, "Unknown error trying publish");
        return;
    }
}
}        // namespace qmedia
