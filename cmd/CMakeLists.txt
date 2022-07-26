
add_executable(sendVideoFrame sendVideoFrame.cc)
target_link_libraries(sendVideoFrame PUBLIC qmedia)

find_package(portaudio QUIET)
if(portaudio_FOUND)
    add_executable(sound sound.cc)
    target_link_libraries(sound PUBLIC qmedia)

    if (WIN32)
        target_link_libraries(sound PUBLIC portaudio)
    else(WIN32)
        target_link_libraries(sound PUBLIC portaudio_static)
    endif(WIN32)
    target_compile_options(sound
        PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
            $<$<CXX_COMPILER_ID:MSVC>: >)
    set_target_properties(sound
        PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS OFF)
else(portaudio_FOUND)
    message(STATUS "Skipping building sound as missing portaudio")
endif(portaudio_FOUND)