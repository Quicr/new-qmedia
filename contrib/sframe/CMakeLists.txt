include(FetchContent)

FetchContent_Declare(   sframe
        GIT_REPOSITORY      https://github.com/cisco/sframe.git
        GIT_TAG             master)
FetchContent_MakeAvailable(sframe)
