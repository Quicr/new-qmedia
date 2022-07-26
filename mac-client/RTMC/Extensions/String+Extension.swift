//
//  String+Extension.swift
//  RTMC
//
//  Created by mbarrass on 1/25/21.
//

import Cocoa

enum Regex {
    static let ipAddress = "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"
    static let hostname = "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$"
}

extension String {
    var isValidIpAddress: Bool {
        return self.matches(pattern: Regex.ipAddress)
    }
    
    var isValidHostname: Bool {
        return self.matches(pattern: Regex.hostname)
    }
    
    private func matches(pattern: String) -> Bool {
        return self.range(of: pattern,
                          options: .regularExpression,
                          range: nil,
                          locale: nil) != nil
    }
}
