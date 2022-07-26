//
//  UIView+Extension.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/20/21.
//

import Cocoa

extension NSView {
    // MARK: - Animation helper functions.
    func fadeOut(_ duration: TimeInterval = 1.0) -> Void {
        NSAnimationContext.runAnimationGroup { context in
            context.duration = duration
            self.animator().alphaValue = 0
        } completionHandler: {
            self.isHidden = true
            self.alphaValue = 1
        }
    }
}
