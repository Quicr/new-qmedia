//
//  Bool+Extension.swift
//  RTMC
//
//  Created by Mark Barrasso on 1/29/21.
//

import Cocoa

extension Bool {
    init(_ number: Int) {
        self.init(truncating: number as NSNumber)
    }
}
