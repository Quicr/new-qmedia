//
//  MediaClient.h
//  RTMC
//
//  Created by Scott Henning on 1/24/23.
//
#ifndef MediaClient_h
#define MediaClient_h

#include "qmedia/media_client.hh"

class MediaClientObj {
public:
    MediaClientObj();
    ~MediaClientObj();
    int32_t createObjectStream();
    void removeObjectStream(int32_t streamId);
    void addStreamObject(int32_t streamId);
    void getStreamObject(int32_t streamId);
private:
    void * self;
    
    
};


#endif /* Header_h */
