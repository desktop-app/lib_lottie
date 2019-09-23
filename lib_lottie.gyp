# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

{
  'includes': [
    '../gyp/helpers/common/common.gypi',
  ],
  'targets': [{
    'target_name': 'lib_lottie',
    'includes': [
      '../gyp/helpers/common/library.gypi',
      '../gyp/helpers/modules/openssl.gypi',
      '../gyp/helpers/modules/qt.gypi',
    ],
    'variables': {
      'src_loc': '.',
      'lottie_use_cache%': 0,
    },
    'dependencies': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
      '<(submodules_loc)/lib_rlottie/lib_rlottie.gyp:lib_rlottie',
    ],
    'export_dependent_settings': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
      '<(submodules_loc)/lib_rlottie/lib_rlottie.gyp:lib_rlottie',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '<(src_loc)',
      ],
    },
    'defines': [
      'LOT_BUILD',
    ],
    'include_dirs': [
      '<(src_loc)',
      '<(libs_loc)/zlib',
    ],
    'sources': [
      '<(src_loc)/lottie/lottie_animation.cpp',
      '<(src_loc)/lottie/lottie_animation.h',
      '<(src_loc)/lottie/lottie_cache.cpp',
      '<(src_loc)/lottie/lottie_cache.h',
      '<(src_loc)/lottie/lottie_common.cpp',
      '<(src_loc)/lottie/lottie_common.h',
      '<(src_loc)/lottie/lottie_frame_renderer.cpp',
      '<(src_loc)/lottie/lottie_frame_renderer.h',
      '<(src_loc)/lottie/lottie_multi_player.cpp',
      '<(src_loc)/lottie/lottie_multi_player.h',
      '<(src_loc)/lottie/lottie_player.h',
      '<(src_loc)/lottie/lottie_single_player.cpp',
      '<(src_loc)/lottie/lottie_single_player.h',
    ],
    'conditions': [['lottie_use_cache', {
      'dependencies': [
        '../gyp/lib_ffmpeg.gyp:lib_ffmpeg',
        '../gyp/lib_lz4.gyp:lib_lz4',
      ],
      'export_dependent_settings': [
        '../gyp/lib_ffmpeg.gyp:lib_ffmpeg',
      ],
      'defines': [
        'LOTTIE_USE_CACHE',
      ],
    }, {
      'sources!': [
        '<(src_loc)/lottie/lottie_cache.cpp',
        '<(src_loc)/lottie/lottie_cache.h',
      ],
    }]],
  }],
}
