//
//  CallViewController.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/11/21.
//

import Cocoa
import AVFoundation

class CallViewController: NSViewController {
    
    // Storyboard Outlets.
    @IBOutlet weak var rootStackView: NSStackView!
    @IBOutlet weak var localVideoView: LocalVideoView!
    @IBOutlet weak var buttonBoxView: NSBox!
    @IBOutlet weak var micButton: NSButton!
    @IBOutlet weak var camButton: NSButton!
    @IBOutlet weak var endButton: NSButton!
    
    // UI Properties.
    private let timeInterval = 5.0
    private var animationTimer: Timer?
    
    // UI State.
    private var isMicMuted: Bool = false
    private var isCamMuted: Bool = false
    
    // Momentum Design Colors.
    let primaryRed = NSColor.colorFromHex(rgbValue: 0xD4371C)
    let secondaryRed = NSColor.colorFromHex(rgbValue: 0x6E1D13)
    let primaryDarkGray = NSColor.colorFromHex(rgbValue: 0x333333)
    let disabledLightGray = NSColor.colorFromHex(rgbValue: 0xB2B2B2)
    
    /// Handles the capturing of local audio/video.
    private var captureManager: CaptureManager = CaptureManager()
    
    /// Handles the rendering of incoming audio/video.
    private var renderManager: RenderManager = RenderManager()
    
    // MARK: - Lifecycle
    
    var initParams: [String:Any]?
    
    var mic: AVCaptureDevice? {
        didSet {
            guard let mic = mic else {fatalError("Failed to set microphone.")}
            self.captureManager.microphone = mic
        }
    }
    
    var cam: AVCaptureDevice? {
        didSet {
            guard let cam = cam else {fatalError("Failed to set camera.")}
            self.captureManager.camera = cam
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        
        // Style the buttons.
        micButton.configure(color: primaryDarkGray, imageName: "microphone_36_w")
        camButton.configure(color: primaryDarkGray, imageName: "camera_36_w")
        endButton.configure(color: primaryRed, imageName: "cancel_36_w")
        
        // Register rendering callbacks.
        NotificationCenter.default.addObserver(self, selector: #selector(beginRenderingAudio(notification:)), name: .renderAudio, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(beginRenderingVideo(notification:)), name: .renderVideo, object: nil)
      
        guard let initParams = self.initParams else {fatalError("Failed to get init params.")}
        let mediaDirection: Constants.NEO_MEDIA.MediaDirection = initParams["mediaDir"] as! Constants.NEO_MEDIA.MediaDirection
        
      captureManager.setMediaDirection(dir: mediaDirection)
      
        // Configure the CaptureManager and prepare the capture session.
        captureManager.prepareCapture() { (err) in
            if err != nil {
                print("Got Capture Manager error: \(err as Any)")
            } else {
                // All good, display the local video preview.
                try? self.captureManager.displayPreview(on: self.localVideoView)
                
                // Define the tracking area for the view and sub to mouseMoved events.
                let trackingArea = NSTrackingArea(rect: self.view.frame, options: [.mouseMoved, .activeInKeyWindow], owner: self, userInfo: nil)
                self.view.addTrackingArea(trackingArea)
                self.setupAnimationTimer()
                
                guard let initParams = self.initParams else {fatalError("Failed to get init params.")}
            
                // Initialize the neo media library.
                let sfuAddr: String = initParams["sfu"] as! String
                let confId: UInt64 = initParams["confId"] as! UInt64
                let echo: Bool = initParams["echo"] as! Bool
                let loopback: Bool = initParams["loopback"] as! Bool
                let loopbackMode: Int = initParams["loopbackMode"] as! Int
                let mediaDirection: Constants.NEO_MEDIA.MediaDirection = initParams["mediaDir"] as! Constants.NEO_MEDIA.MediaDirection
                let publishName: String = initParams["publishName"] as! String
                let subscribeName: String = initParams["subscribeName"] as! String

                var media_client: UnsafeMutableRawPointer? = nil
                var pub_audio_stream_id: uint64 = 0
                var pub_video_stream_id: uint64 = 0
                var sub_audio_stream_id: uint64 = 0
                var sub_video_stream_id: uint64 = 0
            
              MediaClient_Create(externLogCallback, sourceCallback, sfuAddr, Constants.NEO_MEDIA.port, &media_client);
               
                var pubname_int = Int(publishName) ?? 0
                var subname_int = Int(subscribeName) ?? 0
              
               if mediaDirection == Constants.NEO_MEDIA.MediaDirection.publish {

                 print("PubUrl: \(publishName as String)")
                 // audio - opus
                 pub_audio_stream_id = MediaClient_AddAudioStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(pubname_int), 0, Constants.NEO_MEDIA.AUDIO.sampleType, Constants.NEO_MEDIA.AUDIO.sampleRate, Constants.NEO_MEDIA.AUDIO.audioChannels)
                 print("quicr: Audio Pub StreamId \(pub_audio_stream_id)")
                 
                 // video
                 pub_video_stream_id = MediaClient_AddVideoStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(pubname_int), 0, UInt8(Constants.NEO_MEDIA.VIDEO.enc_pixel_format), UInt32(Constants.NEO_MEDIA.VIDEO.max_width), Constants.NEO_MEDIA.VIDEO.max_height, UInt32(Constants.NEO_MEDIA.VIDEO.max_frame_rate), Constants.NEO_MEDIA.VIDEO.max_bitrate)
                 print("quicr: Video Pub StreamId \(pub_video_stream_id)")
                 
              } else if mediaDirection == Constants.NEO_MEDIA.MediaDirection.subscribe {
                
              
                // audio - opus
                sub_audio_stream_id = MediaClient_AddAudioStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(subname_int), 1, Constants.NEO_MEDIA.AUDIO.sampleType, Constants.NEO_MEDIA.AUDIO.sampleRate, Constants.NEO_MEDIA.AUDIO.audioChannels)
               
                print("quicr: Audio Sub StreamId \(sub_audio_stream_id)")
                
                // video - h264
                sub_video_stream_id = MediaClient_AddVideoStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(subname_int), 1, UInt8(Constants.NEO_MEDIA.VIDEO.dec_pixel_format), UInt32(Constants.NEO_MEDIA.VIDEO.max_width), Constants.NEO_MEDIA.VIDEO.max_height, UInt32(Constants.NEO_MEDIA.VIDEO.max_frame_rate), Constants.NEO_MEDIA.VIDEO.max_bitrate)
                print("quicr: Video Sub StreamId \(sub_video_stream_id)")
                
              } else {
                // pub a/v
                
                print("PubUrl: \(publishName as String)")
                // audio - opus
                pub_audio_stream_id = MediaClient_AddAudioStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(pubname_int), 0, Constants.NEO_MEDIA.AUDIO.sampleType, Constants.NEO_MEDIA.AUDIO.sampleRate, Constants.NEO_MEDIA.AUDIO.audioChannels)
                print("quicr: Audio Pub StreamId \(pub_audio_stream_id)")
                
                // video
                pub_video_stream_id = MediaClient_AddVideoStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(pubname_int), 0, UInt8(Constants.NEO_MEDIA.VIDEO.enc_pixel_format), UInt32(Constants.NEO_MEDIA.VIDEO.max_width), Constants.NEO_MEDIA.VIDEO.max_height, UInt32(Constants.NEO_MEDIA.VIDEO.max_frame_rate), Constants.NEO_MEDIA.VIDEO.max_bitrate)
                print("quicr: Video Pub StreamId \(pub_video_stream_id)")
                
                // sub a/v
                // audio - opus
                sub_audio_stream_id = MediaClient_AddAudioStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(subname_int), 1, Constants.NEO_MEDIA.AUDIO.sampleType, Constants.NEO_MEDIA.AUDIO.sampleRate, Constants.NEO_MEDIA.AUDIO.audioChannels)
               
                print("quicr: Audio Sub StreamId \(sub_audio_stream_id)")
                
                // video - h264
                sub_video_stream_id = MediaClient_AddVideoStream(media_client, Constants.NEO_MEDIA.domainId, Constants.NEO_MEDIA.confernceId, UInt64(subname_int), 1, UInt8(Constants.NEO_MEDIA.VIDEO.dec_pixel_format), UInt32(Constants.NEO_MEDIA.VIDEO.max_width), Constants.NEO_MEDIA.VIDEO.max_height, UInt32(Constants.NEO_MEDIA.VIDEO.max_frame_rate), Constants.NEO_MEDIA.VIDEO.max_bitrate)
                print("quicr: Video Sub StreamId \(sub_video_stream_id)")
              
              }
                                  
                // Pass the neo instance to the capture & render managers.
                self.captureManager.media_client = media_client
                self.captureManager.pub_audio_stream = pub_audio_stream_id;
                self.captureManager.pub_video_stream = pub_video_stream_id;
                self.captureManager.sub_audio_stream = sub_audio_stream_id;
                self.captureManager.sub_video_stream =  sub_video_stream_id;
            
                self.renderManager.media_client = media_client
                self.renderManager.pub_audio_stream = pub_audio_stream_id;
                self.renderManager.pub_video_stream = pub_video_stream_id;
                self.renderManager.sub_audio_stream = sub_audio_stream_id;
                self.renderManager.sub_video_stream =  sub_video_stream_id;
          
                
            }
        }
    }
    
    // MARK: - Render callbacks
    
    @objc func beginRenderingAudio(notification: NSNotification) {
        print("Begin rendering audio...")
        if let sourceParams = notification.object as? [String:Any] {
            self.renderManager.renderAudio(with: sourceParams)
        }
    }
        
    @objc func beginRenderingVideo(notification: NSNotification) {
        print("Begin rendering video...")
        if let sourceParams = notification.object as? [String:Any] {
            DispatchQueue.main.async {
                // Create a new remote view and start video rendering.
                let remoteVideoView = RemoteVideoView()
                self.rootStackView.addView(remoteVideoView, in: NSStackView.Gravity.top)
                
                let _ = Timer.scheduledTimer(withTimeInterval: 0.03, repeats: true, block: { (timer) in
                    self.renderManager.renderVideo(on: remoteVideoView, with: sourceParams)
                })
            }
        }
    }
    
    // MARK: - Helper functions
    
    private func setupAnimationTimer() {
        self.animationTimer = Timer.scheduledTimer(withTimeInterval: self.timeInterval, repeats: false, block: { (timer) in
            self.buttonBoxView.fadeOut()
            self.localVideoView.fadeOut()
        })
    }
    
    // MARK: - NSTracking mouse events
    
    override func mouseMoved(with event: NSEvent) {
        
        // Show the localVideoView and buttonBoxView if they are hidden.
        if localVideoView.isHidden || buttonBoxView.isHidden {
            buttonBoxView.isHidden = false
            localVideoView.isHidden = false
        }
        
        // Restart the animation timer.
        if let _ = self.animationTimer?.isValid {
            self.animationTimer?.invalidate()
            self.setupAnimationTimer()
        }
    }
    
    // MARK: - Button Actions
    
    @IBAction func micButtonClicked(_ sender: Any) {
        if isMicMuted {
            captureManager.startMic()
            micButton.configure(color: primaryDarkGray, imageName: "microphone_36_w")
        } else {
            captureManager.stopMic()
            micButton.configure(color: disabledLightGray, imageName: "microphone-muted_36_w")
        }
        isMicMuted = !isMicMuted
    }
    
    @IBAction func camButtonClicked(_ sender: Any) {
        if isCamMuted {
            captureManager.startCamera()
            camButton.configure(color: primaryDarkGray, imageName: "camera_36_w")
        } else {
            captureManager.stopCamera()
            camButton.configure(color: disabledLightGray, imageName: "camera-muted_36_w")
        }
        isCamMuted = !isCamMuted
    }
    
    @IBAction func endButtonClicked(_ sender: Any) {
        // remove all the media streams
         captureManager.stopMediaStreams()
         exit(0)
    }
}

// Global source callback for NeoMediaInstance Initializer.
/// This callback determines the incoming media type and sends
/// a notification to the observer to spin up the correct renderer.
func sourceCallback(_ clientId: UInt64, _ sourceId: UInt64, _ timestamp: UInt64, _ mediaType: Int32) {
    
    let sourceParams: [String:Any] =
    [
        "sourceId": sourceId,
        "clientId": clientId,
        "timestamp": timestamp,
        "mediaType": mediaType
    ]
    print("~~~~~~~~~~~~")
    print("Got a new source:")
    print(sourceParams)
    print("~~~~~~~~~~~~")
    
    if mediaType == 1 { // Audio
        // Tell the observer to start rendering audio.
        NotificationCenter.default.post(name: .renderAudio, object: sourceParams)
    }
        
    if mediaType == 2 { // Video
        // Tell the observer to start rendering video.
        NotificationCenter.default.post(name: .renderVideo, object: sourceParams)
    }
}

// Global source callback for NeoMediaInstance Initializer.
/// This callback is for the logger
/// For now this is just a shell to get the client working correctly with the library
/// TBD: actual code
func externLogCallback(_ logMsg: Optional<UnsafePointer<Int8>>) {
    print("log: \(logMsg as Any)")
}
