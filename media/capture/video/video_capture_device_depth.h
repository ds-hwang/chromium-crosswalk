// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_H_

#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

class DepthStreamCaptureDelegate;

// Provide depth stream using librealsense
class MEDIA_EXPORT VideoCaptureDeviceDepth : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceDepth(const Name& device_name);
  ~VideoCaptureDeviceDepth() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;

  // Utilities used by VideoCaptureDeviceFactory.
  static bool IsSupported();
  static void GetDeviceNames(Names* device_names);
  static void GetDeviceSupportedFormats(const Name& device,
                                        VideoCaptureFormats* supported_formats);

 private:
  scoped_refptr<DepthStreamCaptureDelegate> capture_impl_;

  // Used for reading data from the device.
  base::Thread depth_stream_thread_;

  const Name device_name_;

  base::ThreadChecker thread_checker_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceDepth);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DEPTH_H_
