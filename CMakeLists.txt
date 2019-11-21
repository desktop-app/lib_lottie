# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_lottie OBJECT)
add_library(desktop-app::lib_lottie ALIAS lib_lottie)
init_target(lib_lottie)

get_filename_component(src_loc . REALPATH)

set(lib_lottie_sources
PRIVATE
    lottie/lottie_animation.cpp
    lottie/lottie_animation.h
    lottie/lottie_cache.cpp
    lottie/lottie_cache.h
    lottie/lottie_common.cpp
    lottie/lottie_common.h
    lottie/lottie_frame_renderer.cpp
    lottie/lottie_frame_renderer.h
    lottie/lottie_multi_player.cpp
    lottie/lottie_multi_player.h
    lottie/lottie_player.h
    lottie/lottie_single_player.cpp
    lottie/lottie_single_player.h
)
nice_target_sources(lib_lottie ${src_loc} "${lib_lottie_sources}")

if (DESKTOP_APP_LOTTIE_USE_CACHE)
    target_compile_definitions(lib_lottie PUBLIC LOTTIE_USE_CACHE)
    target_link_libraries(lib_lottie
    PUBLIC
        desktop-app::lib_ffmpeg
        desktop-app::external_lz4
    )
endif()

target_include_directories(lib_lottie
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_lottie
PUBLIC
    desktop-app::lib_ui
    desktop-app::external_rlottie
PRIVATE
    desktop-app::external_zlib
)
