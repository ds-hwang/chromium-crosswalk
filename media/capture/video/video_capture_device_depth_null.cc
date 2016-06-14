// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_depth_null.h"

namespace media {

bool VideoCaptureDeviceDepth::IsSupported() {
  return false;
}

void VideoCaptureDeviceDepth::GetDeviceNames(Names* device_names) {}

void VideoCaptureDeviceDepth::GetDeviceSupportedFormats(
    const Name& device,
    VideoCaptureFormats* formats) {}

}  // namespace media
