#include <qmedia/media_client.hh>
#include <quicr/hex_endec.h>

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
MediaTransportSubDelegate::MediaTransportSubDelegate(
    MediaStreamId id,
    quicr::Namespace quicr_namespace,
    SubscribeCallback callback) :
    canReceiveSubs(false),
    id(id),
    quicr_namespace(quicr_namespace),
    callback(callback)
{
    std::cerr << "MediaTransportSubDelegate" << std::endl;
}

void MediaTransportSubDelegate::onSubscribeResponse(
    const quicr::Namespace& /* quicr_namespace */,
    const quicr::SubscribeResult::SubscribeStatus& /* result */)
{
    std::cerr << "sub::onSubscribeResponse" << std::endl;
}

void MediaTransportSubDelegate::onSubscriptionEnded(
    const quicr::Namespace& /* quicr_namespace */,
    const quicr::SubscribeResult& /* result */)
{
    std::cerr << "sub::onSubscriptionEnded" << std::endl;
}

void MediaTransportSubDelegate::onSubscribedObject(
    const quicr::Name& quicr_name,
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

    std::cerr << "onSubscribedObject " << quicr_name.to_hex() << std::endl;

    quicr::HexEndec<128, 24, 8, 24, 8, 16, 32, 16> name_format;
    auto [orgId, appId, confId, mediaType, clientId, groupId, objectId] =
        name_format.Decode(quicr_name.to_hex());
    callback(id,
             mediaType,
             clientId,
             data.data(),
             data.size() - sizeof(std::uint64_t),
             timestamp);
}

MediaTransportPubDelegate::MediaTransportPubDelegate(MediaStreamId /*id*/)
{
}

void MediaTransportPubDelegate::onPublishIntentResponse(
    const quicr::Namespace& /* quicr_namespace */,
    const quicr::PublishIntentResult& /* result */)
{
    std::cerr << "pub::onPublishIntentResponse" << std::endl;
}

MediaClient::MediaClient(const char* remote_address,
                         std::uint16_t remote_port,
                         quicr::RelayInfo::Protocol protocol,
                         const LoggerPointer& parent_logger) :
    log(std::make_shared<Logger>("qmedia", parent_logger)),
    _streamId(0),
    _orgId(0x00A11CEE),
    _appId(0x00),
    _confId(0x00F00001)
{
    quicr::RelayInfo relayInfo;
    relayInfo.hostname = remote_address;
    relayInfo.port = remote_port;
    relayInfo.proto = protocol;

    // We need two QuicRClients since the relay can't send published messages to
    // the same client that sent them.
    quicRClient = std::make_unique<quicr::QuicRClient>(relayInfo, logger);
    quicRClient = std::make_unique<quicr::QuicRClient>(relayInfo, logger);
}

MediaStreamId MediaClient::add_audio_stream_subscribe(std::uint8_t codec_type,
                                                      SubscribeCallback callback)
{
    quicr::HexEndec<128, 24, 8, 24, 8, 16, 48> name_format;
    const uint8_t mediaType = codec_type << 4;        // First 4 bits is codec
    const uint16_t clientId = 0;                      // 16
    const uint64_t filler = 0;                        // 48

    const std::string nstring = name_format.Encode(
        _orgId, _appId, _confId, mediaType, clientId, filler);

    std::uint8_t namespace_mask_bits = 24 + 8 + 24 + 4;        // orgId + appId
                                                               // + confId +
                                                               // mediaType
    quicr::Namespace quicr_namespace{{nstring}, namespace_mask_bits};

    auto delegate = std::make_shared<MediaTransportSubDelegate>(
        ++_streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClient->subscribe(delegate,
                           quicr_namespace,
                           quicr::SubscribeIntent::immediate,
                           "",
                           false,
                           "",
                           std::move(e2e));
    active_subscription_delegates.insert({_streamId, delegate});
    return _streamId;
}

MediaStreamId MediaClient::add_video_stream_subscribe(std::uint8_t codec_type,
                                                      SubscribeCallback callback)
{
    quicr::HexEndec<128, 24, 8, 24, 8, 16, 48> name_format;
    const uint8_t mediaType = codec_type << 4;        // First 4 bits is codec
    const uint16_t clientId = 0;                      // 16
    const uint64_t filler = 0;                        // 48

    const std::string nstring = name_format.Encode(
        _orgId, _appId, _confId, mediaType, clientId, filler);

    std::uint8_t namespace_mask_bits = 24 + 8 + 24 + 4;        // orgId + appId
                                                               // + confId +
                                                               // mediaType
    quicr::Namespace quicr_namespace{{nstring}, namespace_mask_bits};

    auto delegate = std::make_shared<MediaTransportSubDelegate>(
        ++_streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClient->subscribe(delegate,
                           quicr_namespace,
                           quicr::SubscribeIntent::immediate,
                           "",
                           false,
                           "",
                           std::move(e2e));
    active_subscription_delegates.insert({_streamId, delegate});
    return _streamId;
}

MediaStreamId MediaClient::add_audio_publish_intent(std::uint8_t codec_type,
                                                    std::uint16_t client_id)
{
    quicr::HexEndec<128, 24, 8, 24, 8, 16, 48> name_format;
    auto time = std::time(0);
    const uint8_t mediaType = codec_type;        //  4 bits codec, 4 bits stream
                                                 //  number
    const uint16_t clientId = client_id;         // 16
    const uint64_t uniqueId = time;              // 48 - using time for now

    const std::string nstring = name_format.Encode(
        _orgId, _appId, _confId, mediaType, clientId, uniqueId);

    quicr::Name quicr_name(nstring);
    auto delegate = std::make_shared<MediaTransportPubDelegate>(++_streamId);
    active_publish_delegates.insert({_streamId, delegate});
    publish_names.insert({_streamId, quicr_name});        // save name for later

    // quicr::bytes e2e;
    //  quicr::Namespace quicr_namespace{{nstring},64}; // build namespace
    // quicRClient->publishIntent(delegate, ns, "", "", std::move(e2e));

    return _streamId;
}

MediaStreamId MediaClient::add_video_publish_intent(std::uint8_t codec_type,
                                                    std::uint16_t client_id)
{
    quicr::HexEndec<128, 24, 8, 24, 8, 16, 48> name_format;
    auto time = std::time(0);
    const uint8_t mediaType = codec_type;        // 4 bits codec, 4 bits stream
                                                 // number
    const uint16_t clientId = client_id;         // 16
    const uint64_t uniqueId = time;              // 48 - using time for now

    const std::string nstring = name_format.Encode(
        _orgId, _appId, _confId, mediaType, clientId, uniqueId);

    quicr::Name quicr_name(nstring);

    auto delegate = std::make_shared<MediaTransportPubDelegate>(++_streamId);
    active_publish_delegates.insert({_streamId, delegate});
    publish_names.insert({_streamId, quicr_name});        // save name for later

    // quicr::bytes e2e;
    //  quicr::Namespace quicr_namespace{{nstring},64}; // build namespace SAH -
    //  what should the maek be?
    // quicRClient->publishIntent(delegate, quicr_namespace, "", "",
    // std::move(e2e));

    return _streamId;
}

void MediaClient::remove_video_publish(MediaStreamId /*streamid*/)
{
}
void MediaClient::remove_video_subscribe(MediaStreamId /*streamid*/)
{
}
void MediaClient::remove_audio_publish(MediaStreamId /*streamid*/)
{
}
void MediaClient::remove_audio_subscribe(MediaStreamId /*streamid*/)
{
}

void MediaClient::send_audio_media(MediaStreamId streamid,
                                   uint8_t* data,
                                   std::uint32_t length,
                                   std::uint64_t timestamp)
{
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
    quicRClient->publishNamedObject(quicr_name, 0, 0, false, std::move(b));
    publish_names[streamid] = ++quicr_name;
}

void MediaClient::send_video_media(MediaStreamId streamid,
                                   uint8_t* data,
                                   std::uint32_t length,
                                   std::uint64_t timestamp,
                                   bool groupidflag)
{
    if (publish_names.find(streamid) == publish_names.end())
    {
        std::cerr << "ERROR: send_video_media - could not find publish name "
                     "with id "
                  << streamid << std::endl;
        return;
    }

    auto quicr_name = publish_names[streamid];
    quicr::HexEndec<128, 24, 8, 24, 8, 16, 32, 16> name_format;
    auto [orgId, appId, confId, mediaType, clientId, groupId, objectId] =
        name_format.Decode(quicr_name.to_hex());

    ++objectId;

    // if groupid flag then increment groupid and reset objectid = 0
    if (groupidflag)
    {
        ++groupId;
        objectId = 0;
    }

    const std::string nstring = name_format.Encode(
        orgId, appId, confId, mediaType, clientId, groupId, objectId);
    quicr_name = quicr::Name(nstring);
    quicr::bytes b(data, data + length);
    std::uint8_t* tsbytes = reinterpret_cast<std::uint8_t*>(
        &timestamp);        // look into network byte order
    b.insert(b.end(), tsbytes, tsbytes + sizeof(std::uint64_t));
    quicRClient->publishNamedObject(quicr_name, 0, 0, false, std::move(b));
    publish_names[streamid] = quicr_name;
}

}        // namespace qmedia
