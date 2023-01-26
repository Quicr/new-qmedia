//
//  MediaClient.m
//  RTMC
//
//  Created by Scott Henning on 1/24/23.
//

//
//  XEncode.mm
//  XCodedLib
//
//  Created by Scott Henning on 11/15/22.
//
#import "MediaClientI.h"
//#include "MediaClient.h"

// Implement the XEncode Interface
@implementation MediaClientIFace

// Begin Objection-C
/*
 * Initialize XEncode.
 *
 * Allocate the models and the input buffer.
 */
- (instancetype) init {
    self = [super init];
    if (self) {
        
    }
    return self;
}

- (int32_t) createObjectStream {
    return c.createObjectStream();
}
// End Objective-C

// Begin Objective-C++

// Constructor
MediaClient::MediaClient(void) : self(nullptr) {}

// Destructor
MediaClient::~MediaClient(void) {
   // [(id)CFBridgingRelease(self) dealloc];
}

int32_t MediaClient::createObjectStream()
{
    return 0;
}

@end

