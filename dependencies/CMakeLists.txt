include(FetchContent)


FetchContent_Declare(quicr
        GIT_REPOSITORY      git@github.com:Quicr/new-libquicr.git
        GIT_TAG             main)
set(quicr_BUILD_TESTS OFF)
set(quicrq_BUILD_TESTS OFF)

FetchContent_MakeAvailable( quicr)
