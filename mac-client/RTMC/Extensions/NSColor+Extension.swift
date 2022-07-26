//
//  NSColor+Extension.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/20/21.
//

import Cocoa

extension NSColor {
    static func colorFromHex(rgbValue:UInt32, alpha:Double=1.0) -> NSColor {
        let red = CGFloat((rgbValue & 0xFF0000) >> 16)/256.0
        let green = CGFloat((rgbValue & 0xFF00) >> 8)/256.0
        let blue = CGFloat(rgbValue & 0xFF)/256.0
        return NSColor(red: red, green: green, blue: blue, alpha: CGFloat(alpha))
    }
}
