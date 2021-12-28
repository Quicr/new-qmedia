add_executable( echoSFU echoSFU.cc)  
target_link_libraries( echoSFU PUBLIC neoMedia)

add_executable( bcastSFU bcastSFU.cc)
target_link_libraries( bcastSFU PUBLIC neoMedia)

add_executable(forty forty-bytes.cc)
target_link_libraries( forty PUBLIC neoMedia)

find_package(portaudio QUIET)
if(portaudio_FOUND)
    add_executable( sound sound.cc)
    target_link_libraries( sound PUBLIC neoMedia   )

    if (WIN32)
        target_link_libraries( sound PUBLIC portaudio )
    else(WIN32)
        target_link_libraries( sound PUBLIC portaudio_static )
    endif(WIN32)
else(portaudio_FOUND)
    message(STATUS "Skipping building sound as missing portaudio")
endif(portaudio_FOUND)

if (BUILD_SEND_VIDEO_FRAME)
    add_executable(sendVideoFrame sendVideoFrame.cc)  
    target_link_libraries(sendVideoFrame PUBLIC neoMedia)
endif (BUILD_SEND_VIDEO_FRAME)
