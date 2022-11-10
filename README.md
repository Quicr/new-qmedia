qmedia
========

Implementation of realtime audio/video library and experimental
MacOs client on top of [libquicr](https://github.com/Quicr/libquicr)

Note: This project is under active development and might be breaking 
and fixing things ;)

Here is a sample runnning 2-way realtime call with relay node deployed 
in Atlanta, USA and clients running on mac-book in London (on Hilton Network)

https://github.com/Quicr/qmedia/blob/main/akamia-2-way-call-720.mp4

Here is a simple demo of 1 publisher and 3 subscribers connecting 
to the Relay in Ohio

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
 1. Copy libneo_media_client.dylib into mac-client/Neo directory
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


