qmedia
========

[![CMake](https://github.com/Quicr/new-qmedia/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/Quicr/new-qmedia/actions/workflows/cmake.yml)

Implementation of realtime audio/video library and experimental
MacOs client on top of [libquicr](https://github.com/Quicr/libquicr)

Note: This project is under active development and might be breaking
and fixing things ;)

## 2-Way Realtime Call over QUICR Datagrams
This video captures a 2-way realtime call with QUICR relay node
deployed in Akamai's Atlanta Network, USA and the 2 clients running
on mac-book in London (on Hilton Hotel Network)

Here both the clients publish audio and video streams, as well as
subscribe to each other's audio and video streams.

The Relay node has experimental feature to drop/forward
based on the priority set by the application.

https://user-images.githubusercontent.com/947528/201170693-629525d4-211e-4849-98c5-57b883bccba7.mp4


## 1 Publisher and 3 Subscribers audio/video flows over QUICR Datagrams

Here is a simple demo of 1 publisher sending audio and video streams
with 3 subscribers asking for the same, The Relay node is running on AWS
in Ohio with clients running in SanJose California

https://user-images.githubusercontent.com/947528/181114950-400c22da-f623-4bc5-a8d6-7f4e3188a9c5.mp4


Quickstart
----------
```
git clone https://github.com/Quicr/qmedia
cd qmedia
```

vcpkg is used for some non-cmake friendly dependencies
```
git submodule init
git submodule update
```

Rest of the dependencies are fetched via cmake and should build
libqmedia.a and libneo_media_client.dylib
```
mkdir build
cd build
cmake ..
make all

or with the convenient Makefile

make cclean
make all
```

Please check [Build Prequisites](BUILD-PREREQUISITES.md) in case of
errors.

Running Mac Client
------------------
The source code for Mac client is under mac-client/

mac-client can be run with
```
 1. Copy libneo_media_client.dylib from build/src/extern into mac-client/Neo directory
 2. Open RTMC.xcodeproj under mac-client
 3. Build and Run
   3.1 This needs a QuicR relay to be started as explained below
```

Running mac-client needs a backend QuicR relay server which
can be run locally with
```
1. cd build/_deps/quicrq-build
2.  ./quicrq_app -p 7777 -c ../picoquic-src/certs/cert.pem -k ../picoquic-src/certs/key.pem  server
```



