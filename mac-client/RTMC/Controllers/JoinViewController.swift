//
//  JoinViewController.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/11/21.
//

import Cocoa
import AVFoundation


class JoinViewController: NSViewController {
  
    private var availableMics: [AVCaptureDevice] = []
    private var availableCams: [AVCaptureDevice] = []
    private var selectedMic: AVCaptureDevice?
    private var selectedCam: AVCaptureDevice?
    private var selectedLoopbackMode: Int?
    private var availableLoopbackModes: [Int] =
        [
            2, // codec
            3  // full-media loopback
        ]
    

  private var selectedMediaDirection: Constants.NEO_MEDIA.MediaDirection?
    private var availabeMediaDirection: [String] =
        [
          "publish-only",
          "subscribe-only",
          "publish&subscribe"
        ]
  
  
    
    // MARK: - Storyboard Outlets
    @IBOutlet weak var sfuAddressTextField: NSTextField!
    @IBOutlet weak var publishNameTextField: NSTextField!
    @IBOutlet weak var subcribeNameTextField: NSTextField!

    @IBOutlet weak var audioSourcePullDown: NSPopUpButton!
    @IBOutlet weak var videoSourcePullDown: NSPopUpButton!
    @IBOutlet weak var loopbackModePullDown: NSPopUpButton!
    @IBOutlet weak var mediaDirectionPullDown: NSPopUpButton!
    @IBOutlet weak var loopbackCheckbox: NSButton!
    @IBOutlet weak var joinButton: NSButton!
    
    private func mediaDirectionFromString(dir: String) -> Constants.NEO_MEDIA.MediaDirection {
      switch dir {
      case "publish-only":
        return Constants.NEO_MEDIA.MediaDirection.publish
      case "subscribe-only":
        return Constants.NEO_MEDIA.MediaDirection.subscribe
      case "publish&subscribe":
        return Constants.NEO_MEDIA.MediaDirection.publish_subscribe
      default:
        displayErrorDialog(text: "Media Direction is invalid")
      }
      return Constants.NEO_MEDIA.MediaDirection.inactive
    }
  
    // MARK: - Storyboard Actions
    @IBAction func didTapJoinButton(_ sender: Any) {
        if isValidInput() {
            
            // Prepare the initializer parameters.
            let sfuAddrString = sfuAddressTextField.stringValue
            var publishNameString = ""
            if let text = publishNameTextField?.stringValue, !text.isEmpty {
              publishNameString = publishNameTextField.stringValue;
            }
            
            var subscribeNameString = ""
            if let text = subcribeNameTextField?.stringValue, !text.isEmpty {
              subscribeNameString = subcribeNameTextField.stringValue;
            }
            let loopbackStateBoolean = Bool(loopbackCheckbox.state.rawValue)
            let conferenceIdUnsignedInt = UInt64(1234)
            
            let initParams: [String:Any] =
            [
                "sfu": sfuAddrString,
                "confId": conferenceIdUnsignedInt,
                "echo": false,
                "loopback": loopbackStateBoolean,
                "loopbackMode": selectedLoopbackMode!,
                "publishName": publishNameString,
                "subscribeName": subscribeNameString,
                "mediaDir": selectedMediaDirection!
            ]
            
            // Segue to the call view controller and pass it the neo initializer parameters and selected devices.
            if let callViewController = self.storyboard?.instantiateController(withIdentifier: "CallViewController") as? CallViewController {
                callViewController.initParams = initParams
                callViewController.mic = selectedMic
                callViewController.cam = selectedCam
                self.view.window?.contentViewController = callViewController
            }
        }
    }
    
    @IBAction func didToggleLoopbackCheckbox(_ sender: Any) {
        guard let checkbox = sender as? NSButton else {return}
        let isChecked = Bool(checkbox.state.rawValue)
        if !isChecked {
            loopbackModePullDown.isHidden = true
            loopbackModePullDown.isEnabled = false
        } else {
            loopbackModePullDown.isHidden = false
            loopbackModePullDown.isEnabled = true
        }
    }
    
    @IBAction func didSelectLoopbackMode(_ sender: Any) {
        guard let pulldown = sender as? NSPopUpButton else {return}
        selectedLoopbackMode = availableLoopbackModes[pulldown.indexOfSelectedItem]
    }
    
    @IBAction func didSelectAudioSource(_ sender: Any) {
        guard let pulldown = sender as? NSPopUpButton else {return}
        selectedMic = availableMics[pulldown.indexOfSelectedItem]
    }
    
    @IBAction func didSelectVideoSource(_ sender: Any) {
        guard let pulldown = sender as? NSPopUpButton else {return}
        selectedCam = availableCams[pulldown.indexOfSelectedItem]
    }
    
    @IBAction func didSelectMediaDirection(_ sender: Any) {
      guard let pulldown = sender as? NSPopUpButton else {return}
      selectedMediaDirection = mediaDirectionFromString(dir: availabeMediaDirection[pulldown.indexOfSelectedItem])
    }
    
    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        
        // Do view setup here.
        self.title = "Media10X"
        
        // Hard-coding the SFU host address to make this a bit easier for folks.
        sfuAddressTextField.stringValue = Constants.NEO_MEDIA.sfu
        sfuAddressTextField.isEnabled = true
        
        // Setup loopback stuff.
        loopbackModePullDown.isHidden = true
        loopbackModePullDown.isEnabled = false
        selectedLoopbackMode = availableLoopbackModes.first /// The default selected loopback mode is the first object in the array.
        for mode in availableLoopbackModes {
            loopbackModePullDown.addItem(withTitle: "Loopback mode: \(String(mode))")
        }
      
        mediaDirectionPullDown.isHidden = false
        mediaDirectionPullDown.isEnabled = true
         
      selectedMediaDirection = mediaDirectionFromString(dir: availabeMediaDirection.first!)
        for dir in availabeMediaDirection {
          mediaDirectionPullDown.addItem(withTitle: "\(dir)")
        }
      
        // Populate the pull down menu items with the available AV sources.
        
        // Find microphones.
        let micDiscoverySession = AVCaptureDevice.DiscoverySession(
            deviceTypes: [.builtInMicrophone, .externalUnknown],
            mediaType: AVMediaType.audio,
            position: .unspecified)
        let mics = (micDiscoverySession.devices.compactMap { $0 })
        if mics.isEmpty {
            displayErrorDialog(text: "No microphone found.")
        }
        print("Available mics: \(mics)")
        for mic in mics {
            print("Mic: \(mic)")
            print("Formats: \(mic.formats)")
            availableMics.append(mic)
            audioSourcePullDown.addItem(withTitle: mic.localizedName)
        }
        // Use default mic.
        if let defaultMic = AVCaptureDevice.default(for: .audio) {
            guard let i = availableMics.firstIndex(of: defaultMic) else { fatalError("Couldn't find a default mic.") }
            selectedMic = defaultMic
            audioSourcePullDown.selectItem(at: i)
        } else {
            selectedMic = availableMics[availableMics.count - 1]
        }

        // Find cameras.
        let cameraDiscoverySession = AVCaptureDevice.DiscoverySession(
            deviceTypes: [.builtInWideAngleCamera, .externalUnknown],
            mediaType: AVMediaType.video,
            position: .unspecified)
        let cameras = (cameraDiscoverySession.devices.compactMap { $0 })
        if cameras.isEmpty {
            displayErrorDialog(text: "No camera found.")
        }
        print("Available cameras: \(cameras)")
        for cam in cameras {
            print("Camera: \(cam)")
            print("Formats: \(cam.formats)")
            availableCams.append(cam)
            videoSourcePullDown.addItem(withTitle: cam.localizedName)
        }
        // Use default camera.
        if let defaultCam = AVCaptureDevice.default(for: .video) {
            guard let i = availableCams.firstIndex(of: defaultCam) else { fatalError("Couldn't find a default cam.") }
            selectedCam = defaultCam
            videoSourcePullDown.selectItem(at: i)
        } else {
            selectedCam = availableCams[availableCams.count - 1]
        }
    }

    override var representedObject: Any? {
        didSet {
            // Update the view, if already loaded.
            audioSourcePullDown.synchronizeTitleAndSelectedItem()
            videoSourcePullDown.synchronizeTitleAndSelectedItem()
        }
    }
    
    // MARK: - Input Validation
    private func isValidInput() -> Bool {
        let sfuAddrString = sfuAddressTextField.stringValue
        let audioSourceItem = audioSourcePullDown.selectedItem
        let videoSourceItem = videoSourcePullDown.selectedItem
        
        // CASE: AV Sources
        guard let audioDevice = audioSourceItem else {
            displayErrorDialog(text: "Audio device is not set.")
            return false
        }
        guard let videoDevice = videoSourceItem else {
            displayErrorDialog(text: "Video device is not set.")
            return false
        }
        
        print("Join Details")
        print("~~~~~~~~~~~~")
        print("SFU Address: \(sfuAddrString)")
        print("Audio Device: \(audioDevice.title)")
        print("Video Device: \(videoDevice.title)")
        print("~~~~~~~~~~~~")

        return true
    }
    
    func displayErrorDialog(title: String = "Error!", text: String) {
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = text
        alert.alertStyle = .warning
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }
}
