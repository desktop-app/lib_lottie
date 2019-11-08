add_library(lib_lottie OBJECT)
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

target_compile_definitions(lib_lottie
PUBLIC
    LOTTIE_USE_CACHE
)

target_include_directories(lib_lottie
PUBLIC
    ${src_loc}
PRIVATE
    ${libs_loc}/zlib
)

target_link_libraries(lib_lottie
PUBLIC
    lib_ui
    lib_ffmpeg
    lib_rlottie
PRIVATE
    lib_lz4
)
