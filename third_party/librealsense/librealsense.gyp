# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'src_dir': 'src',
  },
  'targets': [
    {
      'target_name': 'librealsense',
      'type': 'static_library',
      'include_dirs': [
        '<(src_dir)/include',
      ],
      'sources': [
        '<(src_dir)/src/context.cpp',
        '<(src_dir)/src/device.cpp',
        '<(src_dir)/src/f200-private.cpp',
        '<(src_dir)/src/f200.cpp',
        '<(src_dir)/src/image.cpp',
        '<(src_dir)/src/log.cpp',
        '<(src_dir)/src/r200-private.cpp',
        '<(src_dir)/src/r200.cpp',
        '<(src_dir)/src/rs.cpp',
        '<(src_dir)/src/stream.cpp',
        '<(src_dir)/src/sync.cpp',
        '<(src_dir)/src/types.cpp',
        '<(src_dir)/src/uvc-libuvc.cpp',
        '<(src_dir)/src/uvc-v4l2.cpp',
        '<(src_dir)/src/uvc-wmf.cpp',
        '<(src_dir)/src/uvc.cpp',
        '<(src_dir)/src/verify.c',
      ],
      'dependencies': [
        '../libusb/libusb.gyp:libusb',
      ],
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'dependencies': [
            'libuvc',
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'RS_USE_V4L2_BACKEND',
          ],
          'cflags': [ '-mssse3' ],
          'cflags!': [
            '-fno-exceptions',
          ],
          'cflags_cc!': [
            '-fno-exceptions',
          ],
        }],
        # TODO(dshwang): support mac and win later. crbug.com/616098
        ['OS=="mac"', {
          'defines': [
            'RS_USE_LIBUVC_BACKEND',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'RS_USE_WMF_BACKEND',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="linux" or OS=="mac"', {
      'targets': [
        {
          'target_name': 'libuvc',
          'type': 'static_library',
          'sources': [
            '<(src_dir)/src/libuvc/ctrl.c',
            '<(src_dir)/src/libuvc/dev.c',
            '<(src_dir)/src/libuvc/diag.c',
            '<(src_dir)/src/libuvc/frame.c',
            '<(src_dir)/src/libuvc/init.c',
            '<(src_dir)/src/libuvc/stream.c',
          ],
          'dependencies': [
            '../libusb/libusb.gyp:libusb',
          ],
        },
      ],
    }],
  ],
}
