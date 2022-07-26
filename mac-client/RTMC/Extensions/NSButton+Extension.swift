//
//  NSButton+Extension.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/20/21.
//

import Cocoa

extension NSButton {
    
    // MARK: - NSButton UI configurations.
    func configure(color: NSColor = .blue, radius: CGFloat = 18, imageName: String) {
        self.wantsLayer = true
        self.layer?.backgroundColor = color.cgColor
        self.layer?.cornerRadius = radius
        
        self.image = NSImage(named: NSImage.Name(imageName))
        self.isBordered = false
        self.imageScaling = .scaleProportionallyDown
    }

    func configureTitle(title: String, font: NSFont = NSFont.boldSystemFont(ofSize: 12)) {
        self.attributedTitle = NSAttributedString(
            string: title,
            attributes: [
                NSAttributedString.Key.foregroundColor: NSColor.white,
                NSAttributedString.Key.font: NSFont.labelFont(ofSize: 13)
        ])
    }
}
