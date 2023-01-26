include(FetchContent)

################################################################################
# Project URL: https://github.com/cisco/sframe
#     Git URL: https://github.com/cisco/sframe.git
#      Branch: master
#     Targets: sframe
#FetchContent_Declare(   sframe
#    GIT_REPOSITORY      https://github.com/cisco/sframe.git
#    GIT_TAG             master)

################################################################################
# Project URL: https://github.com/cisco/sframe
#     Git URL: https://github.com/cisco/sframe.git
#      Branch: master
#     Targets: sframe
FetchContent_Declare(quicr
        GIT_REPOSITORY      git@github.com:Quicr/new-libquicr.git
        GIT_TAG             main)
set(quicr_BUILD_TESTS OFF)
set(quicrq_BUILD_TESTS OFF)

FetchContent_MakeAvailable(quicr)
