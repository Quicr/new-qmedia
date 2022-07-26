//
//  CaptureManager.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/12/21.
//

import AVFoundation

/// This `CaptureManager` class will be responsible for doing the heavy lifting related to audio and video capture.
///
/// This class conforms to the `AVCaptureAudioDataOutputSampleBufferDelegate` and `AVCaptureVideoDataOutputSampleBufferDelegate` protocols.
/// The captured sample buffers will be handled by the `captureOutput` delegate function.
///
class CaptureManager: NSObject, AVCaptureAudioDataOutputSampleBufferDelegate, AVCaptureVideoDataOutputSampleBufferDelegate {
    
    // AVCapture Properties.
    var captureSession: AVCaptureSession?
    
    // Audio.
    var copyFormatNotificationFired: Bool = false
    var microphone: AVCaptureDevice?
    var microphoneInput: AVCaptureDeviceInput?
    var audioDataOutput: AVCaptureAudioDataOutput?
    var audioDataOuputQueue: DispatchQueue?
    
    // Video.
    var isCamMuted: Bool = false
    var camera: AVCaptureDevice?
    var cameraInput: AVCaptureDeviceInput?
    var videoDataOutput: AVCaptureVideoDataOutput?
    var videoDataOutputQueue: DispatchQueue?
    var previewLayer: AVCaptureVideoPreviewLayer?
    
    // MARK: - Lifecycle ♻️
    
    var media_client: MediaClientInstance?
    var mediaDirection: Constants.NEO_MEDIA.MediaDirection = .inactive
    // todo revisit this
    var pub_audio_stream : uint64 = 0
    var pub_video_stream : uint64 = 0
    var sub_audio_stream : uint64 = 0
    var sub_video_stream : uint64 = 0
  
    func setMediaDirection(dir: Constants.NEO_MEDIA.MediaDirection) {
        mediaDirection = dir
    }
    /// The `prepareCapture` is the main function that handles the creation and configuration of a new capture session.
    ///
    /// - Parameter completionHandler: The completion handler that executes after the function has been called.
    /// - Returns: An optional Error to the `completion` handler.
  func prepareCapture(completionHandler: @escaping (Error?) -> Void) {

        // 1. Create the AVCaptureSession.
        func createCaptureSession() {
            self.captureSession = AVCaptureSession()
            self.captureSession!.sessionPreset = .hd1280x720
        }
        
        // 2. Create inputs using the capture devices.
        func configureDeviceInputs() throws {
            // Make sure the capture session is properly initialized.
            guard let captureSession = self.captureSession else { throw CaptureManagerError.captureSessionIsMissing }
            captureSession.sessionPreset = .hd1280x720
         
            // Add audio input to the capture session.
            if let microphone = self.microphone {
                self.microphoneInput = try AVCaptureDeviceInput(device: microphone)
                
                if captureSession.canAddInput(self.microphoneInput!) {
                    captureSession.addInput(self.microphoneInput!)
                } else {
                  throw CaptureManagerError.inputsAreInvalid
                }
              
            } else {
              throw CaptureManagerError.noMicrophonesAvailable
            }
            
            // Add video input to the capture session.
          if Constants.NEO_MEDIA.video_supported  {
            if let camera = self.camera {
                 self.cameraInput = try AVCaptureDeviceInput(device: camera)
                 if captureSession.canAddInput(self.cameraInput!) {
                     captureSession.addInput(self.cameraInput!)
                 } else {
                   throw CaptureManagerError.inputsAreInvalid
                 }
              
             } else {
               throw CaptureManagerError.noCamerasAvailable
             }
          }
           
          
        }
        
        // 3. Configure the output object to process captured video.
        func configureOutput() throws {
            guard let captureSession = self.captureSession else { throw CaptureManagerError.captureSessionIsMissing }
            
            // Audio data output.
            self.audioDataOutput = AVCaptureAudioDataOutput()
            audioDataOutput!.connection(with: .audio)?.isEnabled = true
            audioDataOuputQueue = DispatchQueue(label: "AudioDataOutputQueue")
            audioDataOutput!.setSampleBufferDelegate(self, queue: audioDataOuputQueue)
            audioDataOutput!.audioSettings =
            [
                AVFormatIDKey: kAudioFormatLinearPCM,
                AVLinearPCMIsFloatKey: true,
                AVLinearPCMIsNonInterleaved: false,
                AVSampleRateKey: Constants.NEO_MEDIA.AUDIO.sampleRate,
                AVLinearPCMBitDepthKey: Constants.NEO_MEDIA.AUDIO.bitDepth,
                AVNumberOfChannelsKey: Constants.NEO_MEDIA.AUDIO.audioChannels
            ]
 
            // Video data output.
            if Constants.NEO_MEDIA.video_supported {
              self.videoDataOutput = AVCaptureVideoDataOutput()
              videoDataOutput!.alwaysDiscardsLateVideoFrames = true
              videoDataOutput!.connection(with: .video)?.isEnabled = true
              videoDataOutputQueue = DispatchQueue(label: "VideoDataOutputQueue")
              videoDataOutput!.setSampleBufferDelegate(self, queue: videoDataOutputQueue)
              videoDataOutput!.videoSettings = [kCVPixelBufferPixelFormatTypeKey.string:
                                                kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange]
              
            }

            // Add the outputs and start the capture session.
            if captureSession.canAddOutput(self.audioDataOutput!) {
                captureSession.addOutput(self.audioDataOutput!)
            }
            
            if Constants.NEO_MEDIA.video_supported {
              if captureSession.canAddOutput(self.videoDataOutput!) {
                  captureSession.addOutput(self.videoDataOutput!)
              }
            }
            
            captureSession.startRunning()
        }
        
        /// Attempt to asynchronously execute the capture preparation functions,
        /// catch any errors if necessary, and then call the completion handler.
        DispatchQueue(label: "prepareCapture").async {
            do {
                createCaptureSession()
                try configureDeviceInputs()
                try configureOutput()
                
            }
            catch {
                DispatchQueue.main.async {
                    completionHandler(error)
                }
                return
            }
            // All good, no errors found.
            DispatchQueue.main.async {
                completionHandler(nil)
            }
        }
    }
    
    /// Implement the AVDataOutputSampleBufferDelegate protocol and its related methods.
    /// This is where the captured buffers will come in.
    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        
        if !CMSampleBufferDataIsReady(sampleBuffer) {
            print("Sample buffer is not ready yet. Skipping sample...")
            return
        }
        
        // Debug: Check captured format description.
        //guard let formatDescription: CMFormatDescription = CMSampleBufferGetFormatDescription(sampleBuffer) else {return}
        //print("Format description: \(formatDescription)")
        
        // Handle the captured audio output data.
        if output == audioDataOutput {
            do {
              try self.processCapturedAudio(sampleBuffer)
              
            } catch { /* TODO: handle exceptions */ }
        }
        
        // Handle the captured video output data.
        if Constants.NEO_MEDIA.video_supported {
          if ((output == videoDataOutput) && (!isCamMuted)) {
              do { try self.processCapturedVideo(sampleBuffer) }
              catch { /* TODO: handle exceptions */ }
          }
        }
    }
    
    // This callback fires if a frame was dropped.
    func captureOutput(_ output: AVCaptureOutput, didDrop sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        debugPrint("Frame dropped. \(sampleBuffer.formatDescription.debugDescription)")
    }
    
    private func processCapturedAudio(_ sampleBuffer: CMSampleBuffer) throws {
        ///
        /// Do Audio buffer stuff here...
        ///
      
      if let media_client = self.media_client {
            // Get the audio format description from the captured audio
            // and pass it to the RenderManager so we can reconstruct the
            // sample buffer on the receive side with the same format.
            if !copyFormatNotificationFired {
                guard let formatDescription: CMFormatDescription = CMSampleBufferGetFormatDescription(sampleBuffer) else {
                    print("Unable to get format description for captured audio.")
                    return
                }
                print("Format description: \(formatDescription)")
                NotificationCenter.default.post(name: .copyFormatDesc, object: formatDescription)
                copyFormatNotificationFired = true
            }
            
            // Make sure to get the data buffer out of the sample buffer.
            guard let blockBuffer = sampleBuffer.dataBuffer else { return }
            let length = blockBuffer.dataLength
            let timestamp = try UInt64(sampleBuffer.sampleTimingInfo(at:0) .presentationTimeStamp.seconds * 1_000_000.0) // microseconds
        
            let temporaryBlock = UnsafeMutablePointer<Int16>.allocate(capacity: length)
            defer { temporaryBlock.deallocate() }
            var returnedPointer: UnsafeMutablePointer<Int8>?

            let status = CMBlockBufferAccessDataBytes(blockBuffer, atOffset: 0, length: length, temporaryBlock: temporaryBlock, returnedPointerOut: &returnedPointer)
            if status != kCMBlockBufferNoErr {
                print("Error in captureOutput: \(status)")
                return
            } else {
                //let temporaryInput = UnsafeBufferPointer<Int16>(start: temporaryBlock, count: length)
                //let temporaryInputArray = Array(temporaryInput)
                //debugPrint("Array: \(temporaryInputArray)")

                // Pass the returned pointer to neo media send audio.
                /// Add +1 to the clientId here because sourceId cannot be the same for audio and video.
                //print("send audio frame \(pub_audio_stream)");
                MediaClient_sendAudio(media_client, pub_audio_stream, returnedPointer, UInt16(length),timestamp)
                //sendAudio(neo, returnedPointer, UInt16(length), timestamp, Constants.NEO_MEDIA.clientId)
            }
        }
    }
    
    private func processCapturedVideo(_ sampleBuffer: CMSampleBuffer) throws {
        ///
        /// Do Video buffer stuff here...
        ///
      if self.mediaDirection == Constants.NEO_MEDIA.MediaDirection.publish
          || self.mediaDirection == Constants.NEO_MEDIA.MediaDirection.publish_subscribe {
        if let media_client = self.media_client{
              // Get an image (aka pixel) buffer from the captured sample buffer.
              guard let imageBuffer = sampleBuffer.imageBuffer else {return}
              guard let width  = sampleBuffer.formatDescription?.dimensions.width else {return}
              guard let height = sampleBuffer.formatDescription?.dimensions.height else {return}
              let format = Constants.NEO_MEDIA.VIDEO.enc_pixel_format
              let timestamp = try UInt64(sampleBuffer.sampleTimingInfo(at:0) .presentationTimeStamp.seconds * 1_000_000.0) // microseconds

              // Must lock the buffer before GetBaseAddress otherwise it can change.
              CVPixelBufferLockBaseAddress(imageBuffer, CVPixelBufferLockFlags.readOnly)

              // Get a pointer to the raw pixels of the image buffer.
              // Don't use CVPixelBufferGetBaseAddress! It points to metadata not pixels.
              let buffer   = CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0)
              let bufferUV = CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1)
              let offsetUV = bufferUV!-buffer!
              let strideY  = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0)
              let strideUV = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1)

              // This returns an oversized buffer with 2880 extra bytes for 1280x720.
              // No clue how to get the real exact size without adding up each plane.
              let bufferSize = CVPixelBufferGetDataSize(imageBuffer)
              
              MediaClient_sendVideoFrame(media_client,
                                         pub_video_stream,
                                         buffer!.bindMemory(to: Int8.self, capacity: bufferSize),
                                         UInt32(bufferSize),
                                         UInt32(width),
                                         UInt32(height),
                                         UInt32(strideY),
                                         UInt32(strideUV),
                                         UInt32(offsetUV),
                                         UInt32(offsetUV),
                                         UInt32(format),
                                         UInt64(timestamp))
              CVPixelBufferUnlockBaseAddress(imageBuffer, CVPixelBufferLockFlags.readOnly)
          }
      }
      
    }
    
    // MARK: - Helper methods
    
    /// This method binds the capture preview layer to the local video view.
    func displayPreview(on view: LocalVideoView) throws {
        guard let captureSession = self.captureSession, captureSession.isRunning else { throw CaptureManagerError.captureSessionIsMissing }
     
        self.previewLayer = AVCaptureVideoPreviewLayer(session: captureSession)
        self.previewLayer?.videoGravity = AVLayerVideoGravity.resizeAspectFill

        let rootLayer: CALayer = view.layer!
        rootLayer.masksToBounds = true
        self.previewLayer?.frame = rootLayer.bounds
        rootLayer.addSublayer(self.previewLayer!)
    }
    
    func startMic() {
        //setMicrophoneMute(self.neo, false)
    }
    
    func stopMic() {
        //setMicrophoneMute(self.neo, true)
    }
    
    func startCamera() {
        //isCamMuted = false
        isCamMuted = true;
        captureSession?.addInput(self.cameraInput!)
    }

    func stopCamera() {
        isCamMuted = true
        captureSession?.removeInput(self.cameraInput!)
    }
  
  func stopMediaStreams() {
    if pub_video_stream != 0 {
      captureSession?.removeInput(self.cameraInput!)
      MediaClient_RemoveMediaStream(media_client, pub_video_stream)
    }
    
    if pub_audio_stream != 0 {
      // remove audio
      captureSession?.removeInput(self.microphoneInput!)
      MediaClient_RemoveMediaStream(media_client, pub_video_stream)
    }
  }
}

extension CaptureManager {
    /// This enum is used to manage the various errors we might encounter while creating a capture session.
    enum CaptureManagerError: Swift.Error {
        case captureSessionAlreadyRunning
        case captureSessionIsMissing
        case inputsAreInvalid
        case invalidOperation
        case noCamerasAvailable
        case noMicrophonesAvailable
        case unknown
    }
}
