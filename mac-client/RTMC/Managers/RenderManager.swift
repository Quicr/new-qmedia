//
//  RenderManager.swift
//  RTMC
//
//  Created by mbarrass on 2/12/21.
//

import Cocoa
import AVFoundation

class RenderManager: NSObject {
    
    // Audio Rendering Properties.
    private var synchronizer: AVSampleBufferRenderSynchronizer = AVSampleBufferRenderSynchronizer()
    private var audioRenderQueue: DispatchQueue?
    var copiedAudioFormatDesc: CMFormatDescription?

    // MARK: - Lifecycle ♻️
    var media_client: MediaClientInstance?
    // todo revisit this
    var pub_audio_stream : uint64 = 0
    var pub_video_stream : uint64 = 0
    var sub_audio_stream : uint64 = 0
    var sub_video_stream : uint64 = 0

    
    override init() {
        super.init()
        
        // Listen for format description copy.
        NotificationCenter.default.addObserver(self, selector: #selector(setFormatDesc(notification:)), name: .copyFormatDesc, object: nil)
    }
    
    @objc func setFormatDesc(notification: NSNotification) {
        let desc = notification.object as! CMFormatDescription
        self.copiedAudioFormatDesc = desc
    }
    
    // MARK: - Audio rendering. 🎤
    func renderAudio(with sourceParams: [String:Any]) {
        
        // Init renderers.
        let audioRenderer = AVSampleBufferAudioRenderer()
        synchronizer.addRenderer(audioRenderer)
        
        audioRenderQueue = DispatchQueue(label: "AudioRenderQueue")
        
        // Prepare params for `getAudio` invocation.
        let clientId: UInt64 = sourceParams["clientId"] as! UInt64
        let sourceId: UInt64 = sourceParams["sourceId"] as! UInt64
        let max_len: UInt16 = UInt16(4096) // ~10.67ms

        audioRenderer.requestMediaDataWhenReady(on: self.audioRenderQueue!) {
            while audioRenderer.isReadyForMoreMediaData {
                
                if let audioFormatDesc = self.copiedAudioFormatDesc, let media_client = self.media_client {
                    
                    var ts: UInt64 = UInt64(0)
                    var in_buffer: UnsafeMutablePointer<UInt8>? = nil
                    var media_buffer_to_free: UnsafeMutableRawPointer? = nil
                    
                    // Get audio data from library.
                  let bufferSize = MediaClient_getAudio(media_client, self.sub_audio_stream, &ts, &in_buffer, max_len, &media_buffer_to_free)
                  
                    // Guard against nil in_buffer.
                    guard let inbuffer = in_buffer else {
                        debugPrint("ERROR: in_buffer from getAudio is nil.")
                        return
                    }
                                
                    // Creates a CMBlockBuffer from the in_buffer.
                    var err: OSStatus = noErr
                    var blockBuffer: CMBlockBuffer? = nil
                    err = CMBlockBufferCreateWithMemoryBlock(
                        allocator: nil,
                        memoryBlock: inbuffer,
                        blockLength: Int(bufferSize),
                        blockAllocator: kCFAllocatorNull,
                        customBlockSource: nil,
                        offsetToData: 0,
                        dataLength: Int(bufferSize),
                        flags: 0,
                        blockBufferOut: &blockBuffer)
                    if err != noErr {
                        fatalError(String(err))
                    }
                    
                    // Guard against nil block buffer.
                    guard let block_buffer = blockBuffer else {
                        debugPrint("CMBlockBufferCreateWithMemoryBlock returned: \(err)")
                        return
                    }
                                
                    // Init sample buffer params.
                    var sampleBuffer: CMSampleBuffer? = nil
                    let sampleCount = bufferSize/8
                    let sampleSizeArray = [Int(8)]
                    var sampleTimingInfo: CMSampleTimingInfo = .invalid
            
                    // Create a CMSampleBuffer from the block buffer.
                    err = CMSampleBufferCreateReady(
                        allocator: nil,
                        dataBuffer: block_buffer,
                        formatDescription: audioFormatDesc,
                        sampleCount: CMItemCount(sampleCount),
                        sampleTimingEntryCount: 1,
                        sampleTimingArray: &sampleTimingInfo,
                        sampleSizeEntryCount: 1,
                        sampleSizeArray: sampleSizeArray,
                        sampleBufferOut: &sampleBuffer)
                    if err != noErr {
                        fatalError(String(err))
                    }

                    // Send the sample buffer to the audio queue for rendering.
                    audioRenderer.enqueue(sampleBuffer!)
                    
                    // Free the audio packet.
                    let _ = Timer.scheduledTimer(withTimeInterval: 2.5, repeats: false) { (timer) in
                        release_media_buffer(media_client, media_buffer_to_free)
                        timer.invalidate()
                    }
                                    
                    if self.synchronizer.rate == 0 {
                        self.synchronizer.setRate(1, time: sampleBuffer!.presentationTimeStamp)
                    }
                }
            }
        }
    }
  
    func stopMediaStreams() {
      if sub_video_stream != 0 {
        MediaClient_RemoveMediaStream(media_client, sub_video_stream)
      }
      
      if sub_audio_stream != 0 {
        // remove audio
        MediaClient_RemoveMediaStream(media_client, sub_video_stream)
      }
    }
    
    // MARK: - Video rendering. 🎥
    func renderVideo(on view: RemoteVideoView, with sourceParams: [String:Any]) {
        DispatchQueue.global().async {
            if let media_client = self.media_client {
                
                // Prepare params for `getVideoFrame` invocation.
                let _: UInt64 = sourceParams["clientId"] as! UInt64
                let sourceId: UInt64 = sourceParams["sourceId"] as! UInt64
                var ts     = UInt64(0)
                var width  = UInt32(0)
                var height = UInt32(0)
                var format = UInt32(Constants.NEO_MEDIA.VIDEO.dec_pixel_format)
                var in_buffer: UnsafeMutablePointer<UInt8>? = nil

              let bufferSize = MediaClient_getVideoFrame(media_client, sourceId, &ts, &width, &height, &format, &in_buffer, nil)
                // Get the video frame.
                //let bufferSize = getVideoFrame(neo, clientId, sourceId, &ts,
                //                               &width, &height, &format, &in_buffer)
                
                // Guard against nil in_buffer.
                guard let inbuffer = in_buffer else {
                    debugPrint("ERROR: in_buffer from getVideoFrame is nil.")
                    return
                }
              
                //debugPrint("App:getVidoFrame returned : \(bufferSize as Any)")


                // Wrap inbuffer to pixel buffer.
                let w = Int(width)
                let h = Int(height)
                let ysize = w * h        // Y is full width & height in YUV 4:2:0
                let usize = ysize >> 2   // U is half width & height
                let _ = usize        // V is half width & height
                let planeY = UnsafeMutableRawPointer(inbuffer)
                let planeU = UnsafeMutableRawPointer(inbuffer+ysize)
                let planeV = UnsafeMutableRawPointer(inbuffer+ysize+usize)
                var planeBaseAddress: [UnsafeMutableRawPointer?] = [planeY, planeU, planeV]
                var planeWidth: [Int] = [w, w/2, w/2]
                var planeHeight: [Int] = [h, h/2, h/2]
                var planeBytesPerRow: [Int] = [w, w/2, w/2]
                var pixelBuffer: CVPixelBuffer? = nil

                let attributes: CFDictionary = [kCVPixelBufferIOSurfacePropertiesKey:{}] as CFDictionary

                // Attempt to create the pixel buffer from the inbuffer data.
                let coreVideoConstant = CVPixelBufferCreateWithPlanarBytes(
                    nil, // default allocator
                    w, h,
                    kCVPixelFormatType_420YpCbCr8Planar, // y420 aka I420/IYUV
                    nil, // NO planar descriptor, use plane parameters below instead
                    Int(bufferSize), // total contiguous size of all planes combined
                    3, // 3 planes Y,U,V
                    &planeBaseAddress, &planeWidth, &planeHeight, &planeBytesPerRow,
                    nil, // NO release callback since library owns the buffer memory
                    nil, // NO context for callback
                    attributes, // attachments
                    &pixelBuffer)
                
                // Guard against nil pixel buffer.
                guard let pixel_buffer = pixelBuffer else {
                    debugPrint("CVPixelBufferCreateWithPlanarBytes returned: \(coreVideoConstant)")
                    return
                }

                // Apply required colorimetric attachments.
                CVBufferSetAttachment(pixel_buffer,
                                      kCVImageBufferColorPrimariesKey,
                                      kCVImageBufferColorPrimaries_ITU_R_709_2,
                                      CVAttachmentMode.shouldPropagate)
                CVBufferSetAttachment(pixel_buffer,
                                      kCVImageBufferTransferFunctionKey,
                                      kCVImageBufferTransferFunction_ITU_R_709_2,
                                      CVAttachmentMode.shouldPropagate)
                CVBufferSetAttachment(pixel_buffer,
                                      kCVImageBufferYCbCrMatrixKey,
                                      kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                                      CVAttachmentMode.shouldPropagate)

                // Create a format description for the new pixel buffer.
                var err: OSStatus = noErr
                var formatDesc: CMVideoFormatDescription? = nil
                err = CMVideoFormatDescriptionCreateForImageBuffer(
                          allocator: nil, // default allocator
                          imageBuffer: pixel_buffer,
                          formatDescriptionOut: &formatDesc)
                if err != noErr {
                    debugPrint("CMVideoFormatDescriptionCreateForImageBuffer error: \(err as Any)")
                    return
                }
                // debugPrint("Format description: \(formatDesc as Any)")
                        
                // Convert pixel buffer to CGImage.
                let cg: CGImage = CGImage.create(pixelBuffer: pixel_buffer)!
                
                // Render the new frame on the view.
                DispatchQueue.main.async {
                    view.layer!.contents = cg
                }
            }
        }
    }
}

extension RenderManager {
    /// This enum is used to manage the various errors we might encounter while rendering.
    enum RenderManagerError: Swift.Error {
        case invalidOperation
        case invalidOutput
        case unknown
    }
}
