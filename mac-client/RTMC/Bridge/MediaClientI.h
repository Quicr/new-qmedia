//
//  MediaClientI.h
//  RTMC
//
//  Created by Scott Henning on 1/24/23.
//
#ifndef MediaClientI_h
#define MediaClientI_h

#import <Foundation/Foundation.h>
#include "MediaClient.h"


@interface MediaClientIFace : NSObject {
    MediaClient c;
}

-(instancetype) init;
-(int32_t) createObjectStream;;

@end

#endif /* MediaClientI_h */
