//
//  InCallButton.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/20/21.
//

import Cocoa

@IBDesignable class InCallButton: NSButton {
    
    /// Adjust these padding variables using the storyboard inspector.
    @IBInspectable var verticalImagePadding: CGFloat = 0
    @IBInspectable var horizontalImagePadding: CGFloat = 0
    
    /// Override the draw function to style the button with image padding.
    override func draw(_ dirtyRect: NSRect) {
        
        // Reset the bounds after drawing is complete.
        let originalBounds = self.bounds
        defer { self.bounds = originalBounds }

        // Inset bounds by the image padding.
        self.bounds = originalBounds.insetBy(
            dx: horizontalImagePadding,
            dy: verticalImagePadding
        )

        super.draw(dirtyRect)
    }
}
