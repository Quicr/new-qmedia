#include <memory>
#include <sstream>
#include <vector>

#include <qmedia/media_client.hh>

namespace qmedia
{


MediaTransportSubDelegate::MediaTransportSubDelegate(MediaStreamId id, 
    quicr::Namespace quicr_namespace, 
    SubscribeCallback callback) : 
    id(id),
    canReceiveSubs(false),
    quicr_namespace(quicr_namespace),
    callback(callback)
{
    std::cerr << "MediaTransportSubDelegate" << std::endl;
}

void MediaTransportSubDelegate::onSubscribeResponse(const quicr::Namespace& quicr_namespace,
                                   const quicr::SubscribeResult::SubscribeStatus& result)
{
    // setting true - false for now - can change later to be more descriptive
    std::cerr << "sub::onSubscribeResponse" << std::endl;
    if (result == quicr::SubscribeResult::SubscribeStatus::Ok)
    {
        canReceiveSubs = true;
    } 
    else
    {
        canReceiveSubs = false;
    }
}

void MediaTransportSubDelegate::onSubscriptionEnded(const quicr::Namespace& quicr_namespace,
                                   const quicr::SubscribeResult& result)
{
    std::cerr << "sub::onSubscriptionEnded" << std::endl;
    canReceiveSubs = false;
}

void MediaTransportSubDelegate:: onSubscribedObject(const quicr::Name& quicr_name,
                                    uint8_t priority,
                                    uint16_t expiry_age_ms,
                                    bool use_reliable_transport,
                                    quicr::bytes&& data)
{
    std::cerr << "sub::onSubscribedObject" << std::endl;
    callback(id, data.data(), data.size() );
}

MediaTransportPubDelegate::MediaTransportPubDelegate(MediaStreamId id) : 
    canPublish(false), 
    id(id)
{
    std::cerr << "MediaTransportPubDelegate" << std::endl;
}

void MediaTransportPubDelegate::onPublishIntentResponse(const quicr::Namespace& quicr_namespace,
                                       const quicr::PublishIntentResult& result)
{
    std::cerr << "pub::onPublishIntentResponse" << std::endl;
    if (result.status == quicr::PublishStatus::Ok) {
        canPublish = true;
    } else {
        canPublish = false;
    }
}

MediaClient::MediaClient(const char *remote_address,
                        std::uint16_t remote_port,
                        std::uint16_t protocol,
                        const LoggerPointer &parent_logger) :
    streamId(0),
    log(std::make_shared<Logger>("qmedia", parent_logger))
{
    quicr::RelayInfo relayInfo;
    relayInfo.hostname = remote_address;
    relayInfo.port = remote_port;
    relayInfo.proto = quicr::RelayInfo::Protocol::UDP;

    buffer = new uint8_t[3000];

    quicRClient = std::make_unique<quicr::QuicRClient>(relayInfo, logger);
}

MediaStreamId MediaClient::add_audio_stream_subscribe(std::uint8_t codec_type,
                                            SubscribeCallback callback)
{
    quicr::Namespace quicr_namespace{{"0xA11CEE00F00001000000000000000000"},64};

    ++streamId;
    auto delegate = std::make_shared<MediaTransportSubDelegate>(streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClient->subscribe(delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", std::move(e2e));
    active_subscription_delegates.insert({streamId, delegate});
    return streamId;
}

MediaStreamId MediaClient::add_video_stream_subscribe(std::uint8_t codec_type,
                                            SubscribeCallback callback)
{
    quicr::Namespace quicr_namespace{{"0xA11CEE00F00001010000000000000000"},64};

    ++streamId;
    auto delegate = std::make_shared<MediaTransportSubDelegate>(streamId, quicr_namespace, callback);

    quicr::bytes e2e;
    quicRClient->subscribe(delegate, quicr_namespace, quicr::SubscribeIntent::immediate, "", false, "", std::move(e2e));
    active_subscription_delegates.insert({streamId, delegate});
    // call quicRClient subscribe 
    return streamId;  
}
                                                                    
MediaStreamId MediaClient::add_audio_publish_intent(std::uint8_t codec_type)
{
    //quicr::Namespace quicr_namespace{{"0xA11CEE00F00001000000000000000000"},64};

    ++streamId;
    
    auto delegate = std::make_shared<MediaTransportPubDelegate>(streamId); 

    //quicr::bytes e2e;
    //quicRClient->publishIntent(delegate, ns, "", "", std::move(e2e));

    //active_publish_delegates.insert({streamId, delegate});
    // call quicRClient subscribe 
    
    return streamId;      
}

MediaStreamId MediaClient::add_video_publish_intent(std::uint8_t codec_type)
{
    quicr::Namespace quicr_namespace{{"0xA11CEE00F00001010000000000000000"},64};

    ++streamId;
    auto delegate = std::make_shared<MediaTransportPubDelegate>(streamId); 

    quicr::bytes e2e;
    //quicRClient->publishIntent(delegate, quicr_namespace, "", "", std::move(e2e));

    //active_publish_delegates[streamId] = delegate;
    active_publish_delegates.insert({streamId, delegate});
    return streamId;    
}

void MediaClient::remove_video_publish(MediaStreamId streamId)
{
    
}
void MediaClient::remove_video_subscribe(MediaStreamId streamId)
{
    
}
void MediaClient::remove_audio_publish(MediaStreamId streamId)
{
    
}
void MediaClient::remove_audio_subscribe(MediaStreamId streamId)
{
    
}

 void MediaClient::send_audio_media(MediaStreamId streamid, 
                                    uint8_t *data, 
                                    std::uint32_t length,  
                                    std::uint64_t timestamp)
 {
    // update the name and send
    std::cerr << "send_audio_media " << length << std::endl;
    std::shared_ptr<MediaTransportPubDelegate> delegate;
    if (active_publish_delegates.find(streamId) == active_publish_delegates.end())
    {
        delegate = std::make_shared<MediaTransportPubDelegate>(streamId);
        active_publish_delegates.insert({streamId, delegate});
    }
    else
    {
        delegate = active_publish_delegates[streamId];
    }

    quicr::Name quicr_name("0xA11CEE00F00001000000000000000000");

    if (publish_names.find(streamid) == publish_names.end())
    {
        publish_names.insert({streamId, quicr_name});
    }
    else
    {
        quicr_name = publish_names[streamid];
    }

    quicr::bytes b(data, data+length-1);

    std::cerr << "bytes d size " << b.size() << std::endl;

    quicRClient->publishNamedObject(quicr_name, 0, 0, false, std::move(b));

    quicr_name += 1;
    publish_names[streamId] = quicr_name;
 }

 void MediaClient::send_video_media(MediaStreamId streamid, 
                                    uint8_t *data, 
                                    std::uint32_t length,  
                                    std::uint64_t timestamp)
 {
 // update the name and send
    std::cerr << "send_audio_media " << length << std::endl;
    std::shared_ptr<MediaTransportPubDelegate> delegate;
    if (active_publish_delegates.find(streamId) == active_publish_delegates.end())
    {
        delegate = std::make_shared<MediaTransportPubDelegate>(streamId);
        active_publish_delegates.insert({streamId, delegate});
    }
    else
    {
        delegate = active_publish_delegates[streamId];
    }

    quicr::Name quicr_name("0xA11CEE00F00001010000000000000000");

    if (publish_names.find(streamid) == publish_names.end())
    {
        publish_names.insert({streamId, quicr_name});
    }
    else
    {
        quicr_name = publish_names[streamid];
    }

    quicr::bytes b(data, data+length-1);

    quicRClient->publishNamedObject(quicr_name, 0, 0, false, std::move(b));

    quicr_name += 1;
    publish_names[streamId] = quicr_name;
 }


}        // namespace qmedia
