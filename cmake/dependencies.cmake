################################################################################
# Pull in dependency by using FetchContent
# https://cmake.org/cmake/help/v3.18/module/FetchContent.html
include(FetchContent)

################################################################################
# Project URL: https://github.com/cisco/sframe
#     Git URL: https://github.com/cisco/sframe.git
#      Branch: master
#     Targets: sframe
FetchContent_Declare(   sframe
    GIT_REPOSITORY      https://github.com/cisco/sframe.git
    GIT_TAG             master)
FetchContent_MakeAvailable(sframe)

