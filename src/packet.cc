
#include <cstring>
#include <string>
#include <cassert>

#include "packet.hh"
#include "media.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
//#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace qmedia
{
std::ostream &operator<<(std::ostream &os, const Packet::Type &pktType)
{
    switch (pktType)
    {
        case Packet::Type::StreamContent:
            os << "StreamContent";
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
        switch (packet->mediaType)
        {
            case Packet::MediaType::Bad:
            {
                assert(0);
                break;
            }
            case Packet::MediaType::F32:
            {
                hdr->set_mediatype(media_message::MediaType::F32);
                break;
            }

            case Packet::MediaType::L16:
            {
                hdr->set_mediatype(media_message::MediaType::L16);
                break;
            }

            case Packet::MediaType::Opus:
            {
                hdr->set_mediatype(media_message::MediaType::OPUS_40K_20MS);
                break;
            }

            case Packet::MediaType::H264:
            {
                hdr->set_mediatype(media_message::MediaType::H264);
                auto video_hdr =
                    std::make_unique<media_message::MediaDataHeader_Video>();
                video_hdr->set_intraframe(packet->is_intra_frame);
                break;
            }

            case Packet::MediaType::Raw:
            {
                hdr->set_mediatype(media_message::MediaType::RAW);
                break;
            }
        }
        hdr->set_sourceid(packet->sourceID);
        hdr->set_sourcerecordtime(packet->sourceRecordTime);
        hdr->set_encodedseqnumber(packet->encodedSequenceNum);

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

    static bool encode(Packet *packet, std::vector<uint8_t> &data_out)
    {
        auto message = media_message::StreamMessage{};
        message.set_conference_id(packet->conferenceID);
        message.set_client_id(packet->clientID);
        encode_stream_content(packet, message);
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
        if (hdr.has_video_header())
        {
            const auto &video_hdr = hdr.video_header();
            packet_out->is_intra_frame = video_hdr.intraframe();
        }

        packet_out->sourceRecordTime = hdr.sourcerecordtime();
        packet_out->sourceID = hdr.sourceid();
        packet_out->encodedSequenceNum = hdr.encodedseqnumber();
    }

    static bool decode(const std::vector<uint8_t> &data_in, Packet *packet_out)
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
        decode_stream_content(message, packet_out);
        return true;
    }
};

///
/// Packet
///
Packet::Packet() :
    conferenceID(0),
    sourceID(0),
    clientID(0),
    sourceRecordTime(0),
    mediaType(Packet::MediaType::Bad),
    data(std::vector<uint8_t>()),
    authTag(std::vector<uint8_t>())
{
}

bool Packet::encode(Packet *packet, std::vector<uint8_t> &data_out)
{
    return PBEncoder::encode(packet, data_out);
}

bool Packet::decode(const std::vector<uint8_t> &data_in, Packet *packet_out)
{
    return PBDecoder::decode(data_in, packet_out);
}

}        // namespace qmedia