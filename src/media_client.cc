#include <qmedia/media_client.hh>
#include <quicr/hex_endec.h>
#include <transport/logger.h>
#include <chrono>
#include "basicLogger.h"

const quicr::HexEndec<128, 24, 8, 24, 8, 16, 32, 16> delegate_name_format;
const quicr::HexEndec<128, 24, 8, 24, 8, 16, 48> client_name_format;

namespace qmedia
{

/*
 * Delegates: CLasses derived from Quicr Delegates for providing
 * `callback` methods for certain events that happen.
 */

/*
 * MediaTransportSubDelgate - derived from quicr::SubscriberDelegate
 *
 * Implements methods for callback such as - subscription response,
 * ended and notification (onSubscribedObject)
 *
 * This class also takes a callback in the constructor. This callback is
 * invoked whenever a onSubscribedObject is invoked by quicr.
 *
 */

/*
 * MediaTransportSubDelegate::MediaTransportSubDelegate
 *
 * Delegate constructor.
 */
MediaTransportSubDelegate::MediaTransportSubDelegate(MediaStreamId id,
                                                     quicr::Namespace quicr_namespace,
                                                     SubscribeCallback callback) :
    canReceiveSubs(false), id(id), quicr_namespace(quicr_namespace), callback(callback)
{
    std::cerr << "MediaTransportSubDelegate" << std::endl;
}

/*
 * delegate: onSubscribeResponse
 */
void MediaTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult& /* result */)
{
    std::cerr << "sub::onSubscribeResponse" << std::endl;
}

/*
 * delegate: onSubscriptionEnded
 */
void MediaTransportSubDelegate::onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                                                    const quicr::SubscribeResult::SubscribeStatus& /* result */)
{
    std::cerr << "sub::onSubscriptionEnded" << std::endl;
}

/*
 * delegate: onSubscribedObject
 *
 * On receiving subscribed object notification fields are extracted
 * from the quicr::name. These fields along with the notificaiton
 * data are passed to the client callback.
 */
void MediaTransportSubDelegate::onSubscribedObject(const quicr::Name& quicr_name,
                                                   uint8_t /*priority*/,
                                                   uint16_t /*expiry_age_ms*/,
                                                   bool /*use_reliable_transport*/,
                                                   quicr::bytes&& data)
{
    // get timestamp from end of data buffer
    std::uint64_t timestamp = 0;
    std::size_t offset = data.size() - sizeof(std::uint64_t);
    const std::uint8_t* tsbytes = &data[offset];
    timestamp = *reinterpret_cast<const std::uint64_t*>(tsbytes);

    //std::cerr << "onSubscribedObject " << quicr_name << std::endl;

    auto [orgId, appId, confId, mediaType, clientId, groupId, objectId] = delegate_name_format.Decode(quicr_name);
    callback(id, mediaType, clientId, data.data(), data.size() - sizeof(std::uint64_t), timestamp);
}

/*
 * MediaTransportPubDelegate::MediaTransportPubDelegate
 *
 * Delegate constructor.
 */
MediaTransportPubDelegate::MediaTransportPubDelegate(MediaStreamId /*id*/)
{
}

/*
 * delegate: onPublishIntentResponse
 */
void MediaTransportPubDelegate::onPublishIntentResponse(const quicr::Namespace& /* quicr_namespace */,
                                                        const quicr::PublishIntentResult& /* result */)
{
    std::cerr << "pub::onPublishIntentResponse" << std::endl;
}

/*
 * MediaClient
 *
 * Provides simple wrapper to QuicrClient. Provides a simplified audio/video stream interface with
 * callbacks for data.
 *
 * MediaClient::MediaClient - constructor
 */
MediaClient::MediaClient(const char* remote_address, std::uint16_t remote_port, quicr::RelayInfo::Protocol protocol) :
    _streamId(0), _orgId(0x00A11CEE), _appId(0x00), _confId(0x00F00001)
{
    quicr::RelayInfo relayInfo;
    relayInfo.hostname = remote_address;
    relayInfo.port = remote_port;
    relayInfo.proto = protocol;



    /*
     * NOTE: data_queue_size needs to be at least
     *      max_msg_size / 1280. For example, a 500KB message
     *      should have a queue of 390.
     */
    qtransport::TransportConfig tcfg { .tls_cert_filename = NULL,
                                     .tls_key_filename = NULL,
                                     .data_queue_size = 200 };

    // Bridge to external logging.
    quicRClient = std::make_unique<quicr::QuicRClient>(relayInfo, std::move(tcfg), logger);

    // check to see if there is a timer thread...
    keepalive_thread = std::thread(&MediaClient::periodic_resubscribe, this, 5);
}

MediaClient::~MediaClient()
{
    close();
}

void MediaClient::close()
{
    std::cerr << "Closing media client" << std::endl;

    stop = true;
    keepalive_thread.join();        // waif for thread to go away...

    quicRClient.reset();

    {
        const std::lock_guard<std::mutex> lock(pubsub_mutex);
        // remove items from containers
        active_subscription_delegates.clear();
        active_publish_delegates.clear();
        subscriptions.clear();
        publish_names.clear();
    }
}

void MediaClient::periodic_resubscribe(const unsigned int seconds)
{
    std::chrono::system_clock::time_point timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    while (!stop)
    {
        std::chrono::duration<int, std::milli> timespan(100);        // sleep duration in mills
        std::this_thread::sleep_for(timespan);

        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        if (now >= timeout && !stop)
        {
            const std::lock_guard<std::mutex> lock(pubsub_mutex);
            for (auto const& [key, val] : subscriptions)
            {
                auto subscription = val;
                quicRClient->subscribe(subscription->sub_delegate,
                                       subscription->quicr_namespace,
                                       subscription->intent,
                                       subscription->origin_url,
                                       subscription->use_reliable_transport,
                                       subscription->auth_token,
                                       std::move(subscription->e2e_token));
            }
            timeout = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        }
    }
}

void MediaClient::add_raw_subscribe(const quicr::Namespace& quicr_namespace,
                                    const std::shared_ptr<quicr::SubscriberDelegate>& delegate)
{
    quicr::bytes e2e;
    quicRClient->subscribe(delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", std::move(e2e));
}

MediaStreamId MediaClient::add_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback)
{
    const uint8_t mediaType = media_type;        // defined by client
    const uint16_t clientId = 0;                 // 16
    const uint64_t filler = 0;                   // 48

    MediaStreamId streamid = ++_streamId;

    const quicr::Name name(client_name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, filler));

    std::uint8_t namespace_mask_bits = 24 + 8 + 24 + 4;        // orgId + appId  + confId +  mediaType
    quicr::Namespace quicr_namespace{name, namespace_mask_bits};

    auto delegate = std::make_shared<MediaTransportSubDelegate>(streamid, quicr_namespace, callback);
    auto subscription = std::make_shared<MediaSubscription>(
        MediaSubscription{delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", quicr::bytes()});
    add_raw_subscribe(quicr_namespace, delegate);

    {
        const std::lock_guard<std::mutex> lock(pubsub_mutex);
        active_subscription_delegates.insert({streamid, delegate});
        subscriptions.insert({streamid, subscription});
    }
    return streamid;
}

// TODO: Figure out if this needs specialisation
MediaStreamId MediaClient::add_audio_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback)
{
    return add_stream_subscribe(media_type, callback);
}

// TODO: Figure out if this needs specialisation
MediaStreamId MediaClient::add_video_stream_subscribe(std::uint8_t media_type, SubscribeCallback callback)
{
    return add_stream_subscribe(media_type, callback);
}

MediaStreamId MediaClient::add_publish_intent(std::uint8_t media_type, std::uint16_t client_id)
{
    auto time = std::time(0);
    const uint8_t mediaType = media_type;        // defined by client
    const uint16_t clientId = client_id;         // 16
    const uint64_t uniqueId = time;              // 48 - using time for now
    MediaStreamId streamid = ++_streamId;

    const quicr::Name quicr_name(client_name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, uniqueId));
    quicr::Namespace ns({quicr_name}, 60);

    auto delegate = std::make_shared<MediaTransportPubDelegate>(streamid);
    auto publishIntent = std::make_shared<PublishIntent>(PublishIntent{ns, ""});

    {
        const std::lock_guard<std::mutex> lock(pubsub_mutex);
        active_publish_delegates.insert({streamid, delegate});
        publish_intents.insert({streamid, publishIntent});
        publish_names.insert({streamid, quicr_name});
    }

    quicr::bytes e2e;
    quicRClient->publishIntent(delegate, ns, "", "", std::move(e2e));

    return streamid;
}

// TODO: Figure out if this needs specialisation
MediaStreamId MediaClient::add_audio_publish_intent(std::uint8_t media_type, std::uint16_t client_id)
{
    return add_publish_intent(media_type, client_id);
}

// TODO: Figure out if this needs specialisation
MediaStreamId MediaClient::add_video_publish_intent(std::uint8_t media_type, std::uint16_t client_id)
{
    return add_publish_intent(media_type, client_id);
}

void MediaClient::remove_publish(MediaStreamId streamid)
{
    const std::lock_guard<std::mutex> lock(pubsub_mutex);

    auto itr = publish_intents.find(streamid);

    if (itr != publish_intents.end())
    {
        auto publish_intent = itr->second;
        if (quicRClient)
        {
            quicRClient->publishIntentEnd(publish_intent->quicr_namespace, publish_intent->auth_token);
        }
        // remove sub delegate
        active_publish_delegates.erase(streamid);
        publish_intents.erase(streamid);
        publish_names.erase(streamid);
    }
    // remove from publish intent???
    // remove from publishes map
}

void MediaClient::remove_subscribe(MediaStreamId streamid)
{
    const std::lock_guard<std::mutex> lock(pubsub_mutex);

    // find subscription using stream id
    auto sub_itr = subscriptions.find(streamid);

    if (sub_itr != subscriptions.end())
    {
        auto subscription = sub_itr->second;
        if (quicRClient)
        {
            quicRClient->unsubscribe(subscription->quicr_namespace, subscription->origin_url, subscription->auth_token);
        }

        // remove sub delegate
        active_subscription_delegates.erase(streamid);
        subscriptions.erase(streamid);
    }
    // remove from subscriptions
}

void MediaClient::send_raw(const quicr::Name& quicr_name, uint8_t* data, std::uint32_t length)
{
    quicr::bytes b(data, data + length);
    quicRClient->publishNamedObject(quicr_name, 0, 0, false, std::move(b));
}

void MediaClient::send_audio_media(MediaStreamId streamid, uint8_t* data, std::uint32_t length, std::uint64_t timestamp)
{
    const std::lock_guard<std::mutex> lock(pubsub_mutex);
    if (publish_names.find(streamid) == publish_names.end())
    {
        std::cerr << "ERROR: send_audio_media - could not find publish name "
                     "with id "
                  << streamid << std::endl;
        return;
    }

    auto quicr_name = publish_names[streamid];
    quicr::bytes b(data, data + length);
    std::uint8_t* tsbytes = reinterpret_cast<std::uint8_t*>(&timestamp);
    b.insert(b.end(), tsbytes, tsbytes + sizeof(std::uint64_t));

    send_raw(quicr_name, b.data(), b.size());

    publish_names[streamid] = ++quicr_name;
}

const quicr::Name object_id_mask = ~(~quicr::Name() << 16);
const quicr::Name group_id_mask = ~(~quicr::Name() << 32) << 16;
void MediaClient::send_video_media(MediaStreamId streamid,
                                   uint8_t* data,
                                   std::uint32_t length,
                                   std::uint64_t timestamp,
                                   bool groupidflag)
{
    const std::lock_guard<std::mutex> lock(pubsub_mutex);
    if (publish_names.find(streamid) == publish_names.end())
    {
        std::cerr << "ERROR: send_video_media - could not find publish name "
                     "with id "
                  << streamid << std::endl;
        return;
    }

    auto quicr_name = publish_names[streamid];

    // Increment objectid flag
    quicr_name = (quicr_name & ~object_id_mask) | (++quicr_name & object_id_mask);

    // if groupid flag then increment groupid and reset objectid = 0
    if (groupidflag)
    {
        auto group_id_bits = (++(quicr_name >> 16) << 16) & group_id_mask;
        quicr_name = ((quicr_name & ~group_id_mask) | group_id_bits) & ~object_id_mask;
    }

    quicr::bytes b(data, data + length);
    std::uint8_t* tsbytes = reinterpret_cast<std::uint8_t*>(&timestamp);
    b.insert(b.end(), tsbytes, tsbytes + sizeof(std::uint64_t));

    send_raw(quicr_name, b.data(), b.size());

    publish_names[streamid] = quicr_name;
}

}        // namespace qmedia
