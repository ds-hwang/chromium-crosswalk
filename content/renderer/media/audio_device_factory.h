// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "media/base/audio_renderer_sink.h"

namespace media {
class AudioInputDevice;
class AudioOutputDevice;
}

namespace url {
class Origin;
}

namespace content {

// A factory for creating AudioOutputDevices and AudioInputDevices.  There is a
// global factory function that can be installed for the purposes of testing to
// provide specialized implementations.
class CONTENT_EXPORT AudioDeviceFactory {
 public:
  // Types of audio sources.
  enum SourceType {
    kSourceNone = 0,
    kSourceMediaElement,
    kSourceWebRtc,
    kSourceNonRtcAudioTrack,
    kSourceWebAudio,
    kSourceLast = kSourceWebAudio  // Only used for validation of format.
  };

  // Creates an AudioOutputDevice.
  // |render_frame_id| refers to the RenderFrame containing the entity
  // producing the audio. If |session_id| is nonzero, it is used by the browser
  // to select the correct input device ID and its associated output device, if
  // it exists. If |session_id| is zero, |device_id| and |security_origin|
  // identify the output device to use.
  // If |session_id| is zero and |device_id| and |security_origin| are empty,
  // the default output device will be selected.
  static scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin);

  // Creates an AudioRendererSink bound to an AudioOutputDevice.
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  // TODO(olka): merge it with NewRestartableOutputDevice() as soon as
  // AudioOutputDevice is fixed to be restartable.
  static scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      SourceType source_type,
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin);

  // Creates a RestartableAudioRendererSink bound to an AudioOutputDevice
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  static scoped_refptr<media::RestartableAudioRendererSink>
  NewRestartableAudioRendererSink(SourceType source_type,
                                  int render_frame_id,
                                  int session_id,
                                  const std::string& device_id,
                                  const url::Origin& security_origin);

  // A helper to get HW device status in the absence of AudioOutputDevice.
  static media::OutputDeviceStatus GetOutputDeviceStatus(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin);

  // Creates an AudioInputDevice using the currently registered factory.
  // |render_frame_id| refers to the RenderFrame containing the entity
  // consuming the audio.
  static scoped_refptr<media::AudioInputDevice> NewInputDevice(
      int render_frame_id);

 protected:
  AudioDeviceFactory();
  virtual ~AudioDeviceFactory();

  // You can derive from this class and specify an implementation for these
  // functions to provide alternate audio device implementations.
  // If the return value of either of these function is NULL, we fall back
  // on the default implementation.
  virtual media::AudioOutputDevice* CreateOutputDevice(
      int render_frame_id,
      int sesssion_id,
      const std::string& device_id,
      const url::Origin& security_origin) = 0;

  virtual media::AudioRendererSink* CreateAudioRendererSink(
      SourceType source_type,
      int render_frame_id,
      int sesssion_id,
      const std::string& device_id,
      const url::Origin& security_origin) = 0;

  virtual media::RestartableAudioRendererSink*
  CreateRestartableAudioRendererSink(SourceType source_type,
                                     int render_frame_id,
                                     int sesssion_id,
                                     const std::string& device_id,
                                     const url::Origin& security_origin) = 0;

  virtual media::AudioInputDevice* CreateInputDevice(int render_frame_id) = 0;

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default AudioRendererSinks.
  static AudioDeviceFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
