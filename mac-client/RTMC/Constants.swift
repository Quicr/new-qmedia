//
//  Constants.swift
//  RTMC


struct Constants {
    
    ///
    /// Neo Media Library version
    /// Branch name: `main`
    /// Latest commit: `06e120ff7623b88becc5ef4b9b7a352c2c3b3a4a`
    ///
    struct NEO_MEDIA {

        // NeoMediaInstance Init parameters.
        static let sfu = "127.0.0.1" //"18.191.247.162" //"13.56.164.43"//"127.0.0.1" // "54.201.7.152" // "18.236.63.86"
        static let port = UInt16(7777)
        static let domainId =  UInt64(1000)
        static let confernceId =  UInt64(2000)
        static let clientId = UInt64(3000)
        static let transport = UInt16(0)  // 0=QUICR
        static let video_supported = true;
        
        // Audio settings.
        struct AUDIO {
            static let bitDepth = 32
            static let sampleRate = UInt16(48000)
            static let audioChannels = UInt8(2)
            static let sampleType = UInt8(0) // 0=F32, 1=INT
        }
        
        // Video settings.
        struct VIDEO {
            static let max_width  = UInt32(1280)
            static let max_height = UInt32(720)
            static let max_frame_rate = UInt16(30)
            static let max_bitrate = UInt32(1_500_000)
            static let enc_pixel_format = UInt16(0) // NV12 420v BiPlanar
            static let dec_pixel_format = UInt16(1) // I420 y420 Planar
        }
      
      struct MEDIA_NAMES {
          static let audioName = "audio"
          static let videoName = "video"
      }
      
      enum MediaDirection : uint16 {
       case publish = 1, subscribe, publish_subscribe, inactive
      }
      
      
      
    }
}
