
#include <cstring>
#include <string>
#include <cassert>

#include "packet.hh"
#include "media.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
//#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace neo_media
{
std::ostream &operator<<(std::ostream &os, const Packet::Type &pktType)
{
    switch (pktType)
    {
        case Packet::Type::Join:
            os << "Join";
            break;
        case Packet::Type::JoinAck:
            os << "JoinAck";
            break;
        case Packet::Type::StreamContent:
            os << "StreamContent";
            break;
        case Packet::Type::StreamContentAck:
            os << "StreamContentAck";
            break;
        case Packet::Type::StreamContentNack:
            os << "StreamContentNack";
            break;
        case Packet::Type::Unknown:
            os << "Unknown";
    }
    return os;
}

// Protobuf Specific Packet encoder
struct PBEncoder
{
    // Encode Stream content (media header and media payload)
    static void encode_stream_content(Packet *packet,
                                      media_message::StreamMessage &message)
    {
        auto hdr = std::make_unique<media_message::MediaDataHeader>();
        hdr->set_prioritylevel(packet->priorityLevel);

        switch (packet->mediaType)
        {
            case Packet::MediaType::Bad:
            {
                assert(0);
                break;
            }
            case Packet::MediaType ::F32:
            {
                hdr->set_mediatype(media_message::MediaType::F32);
                auto audio_hdr =
                    std::make_unique<media_message::MediaDataHeader_Audio>();
                audio_hdr->set_audioenergylevel(packet->audioEnergyLevel);
                hdr->set_allocated_audio_header(audio_hdr.release());
                break;
            }

            case Packet::MediaType::L16:
            {
                hdr->set_mediatype(media_message::MediaType::L16);
                auto audio_hdr =
                    std::make_unique<media_message::MediaDataHeader_Audio>();
                audio_hdr->set_audioenergylevel(packet->audioEnergyLevel);
                hdr->set_allocated_audio_header(audio_hdr.release());
                break;
            }

            case Packet::MediaType::Opus:
            {
                hdr->set_mediatype(media_message::MediaType::OPUS_40K_20MS);
                auto audio_hdr =
                    std::make_unique<media_message::MediaDataHeader_Audio>();
                audio_hdr->set_audioenergylevel(packet->audioEnergyLevel);
                hdr->set_allocated_audio_header(audio_hdr.release());
                break;
            }

            case Packet::MediaType::AV1:
            {
                hdr->set_mediatype(media_message::MediaType::AV1);
                auto video_hdr =
                    std::make_unique<media_message::MediaDataHeader_Video>();
                switch (packet->videoFrameType)
                {
                    case Packet::VideoFrameType::Idr:
                        video_hdr->set_marker(
                            media_message::MediaDataHeader_VideoFrameType_Idr);
                        break;
                    default:
                        video_hdr->set_marker(
                            media_message::MediaDataHeader_VideoFrameType_None);
                }
                video_hdr->set_temporallayerid(packet->temporalLayerId);
                video_hdr->set_spatiallayerid(packet->spatialLayerId);
                video_hdr->set_discardable(packet->discardable_frame);
                video_hdr->set_intraframe(packet->intra_frame);
                hdr->set_allocated_video_header(video_hdr.release());
                break;
            }

            case Packet::MediaType::Raw:
            {
                hdr->set_mediatype(media_message::MediaType::RAW);
                auto video_hdr =
                    std::make_unique<media_message::MediaDataHeader_Video>();
                hdr->set_allocated_video_header(video_hdr.release());
                break;
            }
        }
        hdr->set_sourceid(packet->sourceID);
        hdr->set_sourcerecordtime(packet->sourceRecordTime);
        hdr->set_sequencenumber(packet->encodedSequenceNum);

        switch (packet->packetizeType)
        {
            case Packet::PacketizeType::Simple:
                hdr->set_packetizetype(media_message::PacketizeType::Simple);
                hdr->set_framesize(packet->frameSize);
                hdr->set_packetcount(packet->fragmentCount);
                hdr->set_packetnumber(packet->chunkFragmentNum);
                break;

            default:
                hdr->set_packetizetype(media_message::PacketizeType::None);
                hdr->set_framesize(packet->data.size());
                hdr->set_packetcount(1);
                hdr->set_packetnumber(1);
        }

        auto content =
            std::make_unique<media_message::StreamMessage_StreamContent>();
        auto *media_data = content->add_mediadata();
        media_data->set_allocated_header(hdr.release());

        // Note: this function ends up creating std::string internal to
        // protobuf implementation.
        media_data->set_encryptedmediadata(packet->data.data(),
                                           packet->data.size());

        message.set_allocated_stream_content(content.release());
    }

    static bool encode(Packet *packet, std::string &data_out)
    {
        auto message = media_message::StreamMessage{};
        message.set_conference_id(packet->conferenceID);
        message.set_client_id(packet->clientID);
        message.set_transport_seq_num(packet->transportSequenceNumber);
        message.set_retransmitted(packet->retransmitted);

        switch (packet->packetType)
        {
            case Packet::Type::Unknown:
                assert(0);
                break;
            case Packet::Type::IdrRequest:
            {
                auto idr =
                    std::make_unique<media_message::StreamMessage_IdrRequest>();
                idr->set_client_id(packet->idrRequestData.client_id);
                idr->set_source_id(packet->idrRequestData.source_id);
                idr->set_source_timestamp(
                    packet->idrRequestData.source_timestamp);
                message.set_allocated_idr_request(idr.release());
                std::clog << "encoded idr request\n";
            }
            break;
            case Packet::Type::Join:
            {
                auto join =
                    std::make_unique<media_message::StreamMessage_Join>();
                join->set_echo(packet->echo);
                message.set_allocated_join(join.release());
                break;
            }
            case Packet::Type::JoinAck:
            {
                auto join_ack =
                    std::make_unique<media_message::StreamMessage_JoinAck>();
                message.set_allocated_join_ack(join_ack.release());
                break;
            }
            case Packet::Type::StreamContent:
            {
                encode_stream_content(packet, message);
                break;
            }
            case Packet::Type::StreamContentAck:
            {
                auto content_ack = std::make_unique<
                    media_message::StreamMessage_StreamContentAck>();
                content_ack->set_stream_seq_num(
                    packet->transportSequenceNumber);
                message.set_allocated_stream_content_ack(content_ack.release());
                break;
            }
            case Packet::Type::StreamContentNack:
                // TODO : fix this once we have retransmissions
                assert(0);
                break;
        }
        // Serialize to binary stream
        data_out.resize(message.ByteSizeLong());
        google::protobuf::io::ArrayOutputStream aos(data_out.data(),
                                                    data_out.size());
        if (!message.SerializePartialToZeroCopyStream(&aos))
        {
            return false;
        }

        return true;
    }
};

struct PBDecoder
{
    static void
    decode_stream_content(const media_message::StreamMessage &message,
                          Packet *packet_out)
    {
        const auto &content = message.stream_content();
        // TODO: make it loop through rather than reading just one payload
        assert(content.mediadata().size() == 1);

        const auto &media_data = content.mediadata(0);
        std::copy(media_data.encryptedmediadata().begin(),
                  media_data.encryptedmediadata().end(),
                  std::back_inserter(packet_out->data));

        const auto &hdr = media_data.header();

        packet_out->priorityLevel = hdr.prioritylevel();

        if (hdr.has_audio_header())
        {
            switch (hdr.mediatype())
            {
                case media_message::MediaType::OPUS_40K_20MS:
                    packet_out->mediaType = Packet::MediaType::Opus;
                    break;
                default:
                    packet_out->mediaType = Packet::MediaType::Opus;
                    break;
            }
            packet_out->audioEnergyLevel = hdr.audio_header().audioenergylevel();
        }
        else if (hdr.has_video_header())
        {
            switch (hdr.mediatype())
            {
                case media_message::MediaType::AV1:
                    packet_out->mediaType = Packet::MediaType::AV1;
                    break;
                case media_message::MediaType::RAW:
                    packet_out->mediaType = Packet::MediaType::Raw;
                    break;
                default:
                    packet_out->mediaType = Packet::MediaType::Bad;
                    break;
            }

            const auto &video_hdr = hdr.video_header();
            auto frame_type = video_hdr.marker();
            switch (frame_type)
            {
                case media_message::MediaDataHeader_VideoFrameType_Idr:
                    packet_out->videoFrameType = Packet::VideoFrameType::Idr;
                    break;
                default:
                    packet_out->videoFrameType = Packet::VideoFrameType::None;
            }

            packet_out->spatialLayerId = video_hdr.spatiallayerid();
            packet_out->temporalLayerId = video_hdr.temporallayerid();
            packet_out->intra_frame = video_hdr.intraframe();
            packet_out->discardable_frame = video_hdr.discardable();
        }

        packet_out->encodedSequenceNum = hdr.sequencenumber();
        packet_out->sourceRecordTime = hdr.sourcerecordtime();
        packet_out->sourceID = hdr.sourceid();

        switch (hdr.packetizetype())
        {
            case media_message::PacketizeType::Simple:
                packet_out->fragmentCount = hdr.packetcount();
                packet_out->chunkFragmentNum = hdr.packetnumber();
                packet_out->packetizeType = Packet::PacketizeType::Simple;
                packet_out->frameSize = hdr.framesize();
                break;
            default:
                packet_out->fragmentCount = 1;
                packet_out->chunkFragmentNum = 1;
                packet_out->packetizeType = Packet::PacketizeType::None;
                packet_out->frameSize = packet_out->data.size();
        }
    }

    static bool decode(const std::string &data_in, Packet *packet_out)
    {
        auto message = media_message::StreamMessage{};
        google::protobuf::io::ArrayInputStream ais(data_in.data(),
                                                   data_in.size());
        if (!message.ParseFromZeroCopyStream(&ais))
        {
            return false;
        }

        packet_out->conferenceID = message.conference_id();
        packet_out->clientID = message.client_id();
        packet_out->transportSequenceNumber = message.transport_seq_num();
        packet_out->retransmitted = message.retransmitted();

        if (message.has_join())
        {
            packet_out->packetType = Packet::Type::Join;
        }
        else if (message.has_join_ack())
        {
            packet_out->packetType = Packet::Type::JoinAck;
        }
        else if (message.has_stream_content())
        {
            packet_out->packetType = Packet::Type::StreamContent;
            decode_stream_content(message, packet_out);
        }
        else if (message.has_stream_content_nack())
        {
            assert(0);        // not supported
        }
        else if (message.has_stream_content_ack())
        {
            packet_out->packetType = Packet::Type::StreamContentAck;
            packet_out->transportSequenceNumber =
                message.stream_content_ack().stream_seq_num();
        }
        else if (message.has_idr_request())
        {
            packet_out->packetType = Packet::Type::IdrRequest;
            const auto &idr = message.idr_request();
            packet_out->idrRequestData = Packet::IdrRequestData{
                idr.client_id(), idr.source_id(), idr.source_timestamp()};
            std::clog << "decoded idr request\n";
        }

        return true;
    }
};

///
/// Packet
///
Packet::Packet() :
    transportSequenceNumber(0),
    packetType(Packet::Type::Unknown),
    conferenceID(0),
    sourceID(0),
    clientID(0),
    sourceRecordTime(0),
    encodedSequenceNum(0),
    audioEnergyLevel(0),
    encrypted(false),
    mediaType(Packet::MediaType::Bad),
    data(std::vector<uint8_t>()),
    authTag(std::vector<uint8_t>()),
    packetizeType(Packet::PacketizeType::None),
    fragmentCount(1),
    chunkFragmentNum(1),
    frameSize(0),
    echo(false),
    retransmitted(false)
{
}

bool Packet::encode(Packet *packet, std::string &data_out)
{
    return PBEncoder::encode(packet, data_out);
}

bool Packet::decode(const std::string &data_in, Packet *packet_out)
{
    return PBDecoder::decode(data_in, packet_out);
}

}        // namespace neo_media
