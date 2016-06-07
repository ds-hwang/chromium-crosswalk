// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_depth.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "third_party/librealsense/src/include/librealsense/rs.hpp"

namespace media {

// Provide depth stream in the dedicated thread.
class DepthStreamCaptureDelegate final
    : public base::RefCountedThreadSafe<DepthStreamCaptureDelegate> {
 public:
  DepthStreamCaptureDelegate(
      const VideoCaptureDevice::Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : task_runner_(task_runner),
        device_name_(device_name),
        is_capturing_(false) {}

  // Forward-to versions of VideoCaptureDevice virtual methods.
  void AllocateAndStart(int width,
                        int height,
                        float frame_rate,
                        std::unique_ptr<VideoCaptureDevice::Client> client) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    DCHECK(client);
    client_ = std::move(client);

    ctx_.reset(new rs::context);
    if (!ctx_->get_device_count()) {
      SetErrorState(FROM_HERE, "Failed to connect depth camera.");
      return;
    }
    dev_ = ctx_->get_device(0);
    dev_->enable_stream(rs::stream::depth, width, height, rs::format::z16,
                        frame_rate);
    dev_->start();

    rs::intrinsics depth_intrin =
        dev_->get_stream_intrinsics(rs::stream::depth);
    capture_format_.frame_size.SetSize(depth_intrin.width, depth_intrin.height);
    capture_format_.frame_rate = dev_->get_stream_framerate(rs::stream::depth);
    capture_format_.pixel_format = PIXEL_FORMAT_YUY2;

    is_capturing_ = true;
    // Post task to start fetching frames from v4l2.
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&DepthStreamCaptureDelegate::DoCapture, this));
  }

  void StopAndDeAllocate() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    // At this point we can close the device.
    // This is also needed for correctly changing settings later via
    // VIDIOC_S_FMT.
    dev_->stop();
    is_capturing_ = false;
    client_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<DepthStreamCaptureDelegate>;
  ~DepthStreamCaptureDelegate() {}

  void DoCapture() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    if (!is_capturing_)
      return;

    try {
      dev_->wait_for_frames();
      DCHECK(dev_->is_stream_enabled(rs::stream::depth));
      base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
          dev_->get_frame_timestamp(rs::stream::depth));
      const uint8_t* depth_image =
          static_cast<const uint8_t*>(dev_->get_frame_data(rs::stream::depth));
      client_->OnIncomingCapturedData(
          depth_image, capture_format_.ImageAllocationSize(), capture_format_,
          0, base::TimeTicks::Now(), timestamp);

      task_runner_->PostTask(
          FROM_HERE, base::Bind(&DepthStreamCaptureDelegate::DoCapture, this));
    } catch (const rs::error& e) {
      SetErrorState(FROM_HERE, "Failed to capture depth stream.");
    }
  }

  void SetErrorState(const tracked_objects::Location& from_here,
                     const std::string& reason) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    is_capturing_ = false;
    client_->OnError(from_here, reason);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const VideoCaptureDevice::Name device_name_;

  // The following members are only known on AllocateAndStart().
  VideoCaptureFormat capture_format_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  bool is_capturing_;

  std::unique_ptr<rs::context> ctx_;
  rs::device* dev_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DepthStreamCaptureDelegate);
};

VideoCaptureDeviceDepth::VideoCaptureDeviceDepth(const Name& device_name)
    : depth_stream_thread_("DepthStreamCaptureThread"),
      device_name_(device_name) {}

VideoCaptureDeviceDepth::~VideoCaptureDeviceDepth() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool VideoCaptureDeviceDepth::IsSupported() {
  return true;
}

void VideoCaptureDeviceDepth::GetDeviceNames(Names* device_names) {
  for (auto device_name : *device_names) {
    std::size_t found = device_name.name().find("RealSense");
    if (found!=std::string::npos) {
      device_names->push_back(VideoCaptureDevice::Name(
          "Depth Camera", device_name.id(),
          VideoCaptureDevice::Name::DEPTH_STREAM));
      return;
    }
  }
}

void VideoCaptureDeviceDepth::GetDeviceSupportedFormats(
    const Name& device,
    VideoCaptureFormats* supported_formats) {
  // DEPTH_STREAM requires to create and initialize device to get supported
  // formats.
  VideoCaptureFormat supported_format;
  supported_format.frame_size.SetSize(640, 480);
  supported_format.pixel_format = PIXEL_FORMAT_YUY2;
  supported_format.frame_rate = 60.f;
  supported_formats->push_back(supported_format);
  DVLOG(1) << VideoCaptureFormat::ToString(supported_format);
}

void VideoCaptureDeviceDepth::AllocateAndStart(const VideoCaptureParams& params,
                                               std::unique_ptr<Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capture_impl_);
  if (depth_stream_thread_.IsRunning())
    return;  // Wrong state.
  depth_stream_thread_.Start();

  LOG(ERROR)<<"HEREEEEEEEEEEEE";

  capture_impl_ = new DepthStreamCaptureDelegate(
      device_name_, depth_stream_thread_.task_runner());
  if (!capture_impl_) {
    client->OnError(FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }
  depth_stream_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DepthStreamCaptureDelegate::AllocateAndStart, capture_impl_,
                 params.requested_format.frame_size.width(),
                 params.requested_format.frame_size.height(),
                 params.requested_format.frame_rate, base::Passed(&client)));
}

void VideoCaptureDeviceDepth::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!depth_stream_thread_.IsRunning())
    return;  // Wrong state.
  depth_stream_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&DepthStreamCaptureDelegate::StopAndDeAllocate,
                            capture_impl_));
  depth_stream_thread_.Stop();

  capture_impl_ = nullptr;
}

}  // namespace media
