// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_NULL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_NULL_H_

#include "base/threading/non_thread_safe.h"
#include "media/base/media_export.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// The NULL implementation of VideoCaptureDevice.
class MEDIA_EXPORT VideoCaptureDeviceDepth : public base::NonThreadSafe,
                                             public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceDepth(const Name& device_name) {}
  ~VideoCaptureDeviceDepth() override {}

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {}
  void StopAndDeAllocate() override {}

  // Utilities used by VideoCaptureDeviceFactory.
  static bool IsSupported();
  static void GetDeviceNames(Names* device_names);
  static void GetDeviceSupportedFormats(const Name& device,
                                        VideoCaptureFormats* formats);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceDepth);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_NULL_H_
