#include <memory>
#include <sstream>
#include <vector>
#include <iostream>
//#include <chrono>

#include <qmedia/media_client.hh>
#include <quicr/hex_endec.h>

namespace qmedia
{


MediaTransportSubDelegate::MediaTransportSubDelegate(MediaStreamId id, 
    quicr::Namespace quicr_namespace, 
    SubscribeCallback callback) : 
    canReceiveSubs(false),
    id(id),
    quicr_namespace(quicr_namespace),
    callback(callback)
{
    std::cerr << "MediaTransportSubDelegate" << std::endl;
}

void MediaTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                                   const quicr::SubscribeResult::SubscribeStatus& result)
{
    // setting true - false for now - can change later to be more descriptive
    if (result == quicr::SubscribeResult::SubscribeStatus::Ok)
    {
        canReceiveSubs = true;
    } 
    else
    {
        canReceiveSubs = false;
    }
}

void MediaTransportSubDelegate::onSubscriptionEnded(const quicr::Namespace& /* quicr_namespace */,
                                   const quicr::SubscribeResult& /* result */)
{
    std::cerr << "sub::onSubscriptionEnded" << std::endl;
    canReceiveSubs = false;
}

void MediaTransportSubDelegate:: onSubscribedObject(const quicr::Name& /*quicr_name*/,
                                    uint8_t /*priority*/,
                                    uint16_t /*expiry_age_ms*/,
                                    bool /*use_reliable_transport*/,
                                    quicr::bytes&& data)
{
    // get timestamp from end of data buffer
    std::uint64_t timestamp = 0;
    std::size_t offset = data.size() - sizeof(std::uint64_t);
    const std::uint8_t *tsbytes = &data[offset];
    timestamp = *reinterpret_cast<const std::uint64_t*>(tsbytes);
    std::cerr << "onSubscribedObject " << std::endl;

    //quicr::bytes b = data;
    callback(id, data.data(), data.size() - sizeof(std::uint64_t), timestamp );
}

MediaTransportPubDelegate::MediaTransportPubDelegate(MediaStreamId /*id*/) : 
    canPublish(false)
{
}

void MediaTransportPubDelegate::onPublishIntentResponse(const quicr::Namespace& /* quicr_namespace */,
                                       const quicr::PublishIntentResult& result)
{
    if (result.status == quicr::PublishStatus::Ok) {
        canPublish = true;
    } else {
        canPublish = false;
    }
}

MediaClient::MediaClient(const char *remote_address,
                        std::uint16_t remote_port,
                        quicr::RelayInfo::Protocol protocol,
                        const LoggerPointer &parent_logger) :
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
    //relayInfo.proto = quicr::RelayInfo::Protocol::UDP;

    // We need two QuicRClients since the relay can't send published messages to
    // the same client that sent them.
    quicRClientSubscribe = std::make_unique<quicr::QuicRClient>(relayInfo, logger);
    quicRClientPublish = std::make_unique<quicr::QuicRClient>(relayInfo, logger);
}

MediaStreamId MediaClient::add_audio_stream_subscribe(std::uint8_t codec_type,
                                            SubscribeCallback callback)
{
    quicr::HexEndec<128,24,8,24,8,16,48> name_format;
    const uint8_t  mediaType = 0x00 | codec_type; //  0 - [0 - first bit = audio, 1 - 7bits = opus]
    const uint16_t clientId  = 0xBBBB;     // 16
    const uint64_t filler = 0; // 48

    const std::string nstring = name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, filler);
    quicr::Namespace quicr_namespace{{nstring},64};

    auto delegate = std::make_shared<MediaTransportSubDelegate>(++_streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClientSubscribe->subscribe(delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", std::move(e2e));
    active_subscription_delegates.insert({_streamId, delegate});
    return _streamId;
}

MediaStreamId MediaClient::add_video_stream_subscribe(std::uint8_t codec_type,
                                            SubscribeCallback callback)
{
    quicr::HexEndec<128,24,8,24,8,16,48> name_format;
    const uint8_t  mediaType = 0x80 | codec_type; //  8 - [0 - first bit = audio, 1 - 7bits = opus]
    const uint16_t clientId  = 0xBBBB;     // 16
    const uint64_t filler = 0; // 48

    const std::string nstring = name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, filler);
    quicr::Namespace quicr_namespace{{nstring},64};

    auto delegate = std::make_shared<MediaTransportSubDelegate>(++_streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClientSubscribe->subscribe(delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", std::move(e2e));
    active_subscription_delegates.insert({_streamId, delegate});
    // call quicRClient subscribe 
    return _streamId;  
}
                                                                    
MediaStreamId MediaClient::add_audio_publish_intent(std::uint8_t codec_type)
{
    quicr::HexEndec<128,24,8,24,8,16,48> name_format;
    auto time = std::time(0);
    const uint8_t  mediaType = 0x00 | codec_type; //  8 - [0 - first bit = audio, 1 - 7bits = opus]
    const uint16_t clientId  = 0xBBBB;     // 16
    const uint64_t uniqueId  = time;       // 48 - using time for now

    const std::string nstring = name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, uniqueId);

    quicr::Name quicr_name(nstring);
    quicr::Namespace quicr_namespace{{nstring},64}; // build namespace
    auto delegate = std::make_shared<MediaTransportPubDelegate>(++_streamId); 
    active_publish_delegates.insert({_streamId, delegate});
    publish_names.insert({_streamId, quicr_name}); // save name for later

    //quicr::bytes e2e;
    //quicRClientPublish->publishIntent(delegate, ns, "", "", std::move(e2e));

    return _streamId;      
}

MediaStreamId MediaClient::add_video_publish_intent(std::uint8_t codec_type)
{
    quicr::HexEndec<128,24,8,24,8,16,48> name_format;
    auto time = std::time(0);
    const uint8_t  mediaType = 0x80 | codec_type; //  8 - [1 - first bit = video, 2 - 7bits = h264]
    const uint16_t clientId  = 0xBBBB;     // 16
    const uint64_t uniqueId  = time;       // 48 - using time for now

    const std::string nstring = name_format.Encode(_orgId, _appId, _confId, mediaType, clientId, uniqueId);

    quicr::Name quicr_name(nstring);
    quicr::Namespace quicr_namespace{{nstring},64}; // build namespace
    auto delegate = std::make_shared<MediaTransportPubDelegate>(++_streamId); 
    active_publish_delegates.insert({_streamId, delegate});
    publish_names.insert({_streamId, quicr_name}); // save name for later

    //quicr::bytes e2e;
    //quicRClientPublish->publishIntent(delegate, quicr_namespace, "", "", std::move(e2e));

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
                                    uint8_t *data, 
                                    std::uint32_t length,  
                                    std::uint64_t timestamp)
 {
    if (publish_names.find(streamid) == publish_names.end())
    {
        std::cerr << "ERROR: send_audio_media - could not find publish name with id " << streamid << std::endl;
        return;
    }

    auto quicr_name = publish_names[streamid];
    quicr::bytes b(data, data+length);
    std::uint8_t* tsbytes = reinterpret_cast<std::uint8_t*>(&timestamp);
    b.insert(b.end(), tsbytes, tsbytes + sizeof(std::uint64_t));
    quicRClientPublish->publishNamedObject(quicr_name, 0, 0, false, std::move(b));
    publish_names[streamid] = ++quicr_name;
 }

 void MediaClient::send_video_media(MediaStreamId streamid, 
                                    uint8_t *data, 
                                    std::uint32_t length,  
                                    std::uint64_t timestamp,
                                    bool groupidflag)
 {
    if (publish_names.find(streamid) == publish_names.end())
    {
        std::cerr << "ERROR: send_video_media - could not find publish name with id " << streamid << std::endl;
        return;
    }

    auto quicr_name = publish_names[streamid];
    quicr::HexEndec<128,24,8,24,8,16,32,16> name_format;
    auto [orgId, appId, confId, mediaType, clientId, groupId, objectId] = name_format.Decode(quicr_name.to_hex());

    ++objectId;
    
    // if groupid flag then increment groupid and reset objectid = 0
    if (groupidflag) 
    {
        ++groupId;
        objectId = 0;
    }

    const std::string nstring = name_format.Encode(orgId, appId, confId, mediaType, clientId, groupId, objectId);
    quicr_name = quicr::Name(nstring);
    quicr::bytes b(data, data+length);
    std::uint8_t* tsbytes = reinterpret_cast<std::uint8_t*>(&timestamp); // look into network byte order
    b.insert(b.end(), tsbytes, tsbytes + sizeof(std::uint64_t));      
    quicRClientPublish->publishNamedObject(quicr_name, 0, 0, false, std::move(b));
    publish_names[streamid] = quicr_name;
 }

}        // namespace qmedia
