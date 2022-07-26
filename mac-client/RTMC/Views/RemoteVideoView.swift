//
//  RemoteVideoView.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/13/21.
//

import Cocoa
import AVFoundation

class RemoteVideoView: NSView {
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        self.layer?.masksToBounds = true
        self.layer?.contentsGravity = .resizeAspect
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
