// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "chromecast/media/cma/base/buffering_controller.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/audio_decoder_software_wrapper.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

namespace {

// Buffering parameters when load_type is kLoadTypeUrl.
const base::TimeDelta kLowBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(2000));
const base::TimeDelta kHighBufferThresholdURL(
    base::TimeDelta::FromMilliseconds(6000));

// Buffering parameters when load_type is kLoadTypeMediaSource.
const base::TimeDelta kLowBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(0));
const base::TimeDelta kHighBufferThresholdMediaSource(
    base::TimeDelta::FromMilliseconds(300));

// Interval between two updates of the media time.
const base::TimeDelta kTimeUpdateInterval(
    base::TimeDelta::FromMilliseconds(250));

// Interval between two updates of the statistics is equal to:
// kTimeUpdateInterval * kStatisticsUpdatePeriod.
const int kStatisticsUpdatePeriod = 4;

}  // namespace

struct MediaPipelineImpl::FlushTask {
  bool audio_flushed;
  bool video_flushed;
  base::Closure done_cb;
};

MediaPipelineImpl::MediaPipelineImpl()
    : cdm_(nullptr),
      backend_state_(BACKEND_STATE_UNINITIALIZED),
      playback_rate_(1.0f),
      audio_decoder_(nullptr),
      video_decoder_(nullptr),
      pending_time_update_task_(false),
      statistics_rolling_counter_(0),
      weak_factory_(this) {
  CMALOG(kLogControl) << __FUNCTION__;
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

MediaPipelineImpl::~MediaPipelineImpl() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  weak_factory_.InvalidateWeakPtrs();

  // Since av pipeline still need to access device components in their
  // destructor, it's important to delete them first.
  video_pipeline_.reset();
  audio_pipeline_.reset();
  audio_decoder_.reset();
  media_pipeline_backend_.reset();
  if (!client_.pipeline_backend_destroyed_cb.is_null())
    client_.pipeline_backend_destroyed_cb.Run();
}

void MediaPipelineImpl::Initialize(
    LoadType load_type,
    scoped_ptr<MediaPipelineBackend> media_pipeline_backend) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_decoder_.reset();
  media_pipeline_backend_.reset(media_pipeline_backend.release());
  if (!client_.pipeline_backend_created_cb.is_null())
    client_.pipeline_backend_created_cb.Run();

  if (load_type == kLoadTypeURL || load_type == kLoadTypeMediaSource) {
    base::TimeDelta low_threshold(kLowBufferThresholdURL);
    base::TimeDelta high_threshold(kHighBufferThresholdURL);
    if (load_type == kLoadTypeMediaSource) {
      low_threshold = kLowBufferThresholdMediaSource;
      high_threshold = kHighBufferThresholdMediaSource;
    }
    scoped_refptr<BufferingConfig> buffering_config(
        new BufferingConfig(low_threshold, high_threshold));
    buffering_controller_.reset(new BufferingController(
        buffering_config,
        base::Bind(&MediaPipelineImpl::OnBufferingNotification, weak_this_)));
  }
}

void MediaPipelineImpl::SetClient(const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client.error_cb.is_null());
  DCHECK(!client.buffering_state_cb.is_null());
  DCHECK(!client.pipeline_backend_created_cb.is_null());
  DCHECK(!client.pipeline_backend_destroyed_cb.is_null());
  client_ = client;
}

void MediaPipelineImpl::SetCdm(int cdm_id) {
  CMALOG(kLogControl) << __FUNCTION__ << " cdm_id=" << cdm_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(gunsch): SetCdm(int) is not implemented.
  // One possibility would be a GetCdmByIdCB that's passed in.
}

void MediaPipelineImpl::SetCdm(BrowserCdmCast* cdm) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  cdm_ = cdm;
  if (audio_pipeline_)
    audio_pipeline_->SetCdm(cdm);
  if (video_pipeline_)
    video_pipeline_->SetCdm(cdm);
}

::media::PipelineStatus MediaPipelineImpl::InitializeAudio(
    const ::media::AudioDecoderConfig& config,
    const AvPipelineClient& client,
    scoped_ptr<CodedFrameProvider> frame_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_decoder_);

  MediaPipelineBackend::AudioDecoder* backend_audio_decoder =
      media_pipeline_backend_->CreateAudioDecoder();
  if (!backend_audio_decoder) {
    return ::media::PIPELINE_ERROR_ABORT;
  }
  audio_decoder_.reset(new AudioDecoderSoftwareWrapper(backend_audio_decoder));
  audio_pipeline_.reset(new AudioPipelineImpl(audio_decoder_.get(), client));
  if (cdm_)
    audio_pipeline_->SetCdm(cdm_);
  return audio_pipeline_->Initialize(config, std::move(frame_provider));
}

::media::PipelineStatus MediaPipelineImpl::InitializeVideo(
    const std::vector<::media::VideoDecoderConfig>& configs,
    const VideoPipelineClient& client,
    scoped_ptr<CodedFrameProvider> frame_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!video_decoder_);

  video_decoder_ = media_pipeline_backend_->CreateVideoDecoder();
  if (!video_decoder_) {
    return ::media::PIPELINE_ERROR_ABORT;
  }
  video_pipeline_.reset(new VideoPipelineImpl(video_decoder_, client));
  if (cdm_)
    video_pipeline_->SetCdm(cdm_);
  return video_pipeline_->Initialize(configs, std::move(frame_provider));
}

void MediaPipelineImpl::StartPlayingFrom(base::TimeDelta time) {
  CMALOG(kLogControl) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_task_);

  // Lazy initialize.
  if (backend_state_ == BACKEND_STATE_UNINITIALIZED) {
    if (!media_pipeline_backend_->Initialize()) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
    backend_state_ = BACKEND_STATE_INITIALIZED;
  }

  // Start the backend.
  if (!media_pipeline_backend_->Start(time.InMicroseconds())) {
    OnError(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  backend_state_ = BACKEND_STATE_PLAYING;

  // Enable time updates.
  statistics_rolling_counter_ = 0;
  if (!pending_time_update_task_) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_));
  }

  // Setup the audio and video pipeline for the new timeline.
  if (audio_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("audio");
    if (!audio_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
  if (video_pipeline_) {
    scoped_refptr<BufferingState> buffering_state;
    if (buffering_controller_)
      buffering_state = buffering_controller_->AddStream("video");
    if (!video_pipeline_->StartPlayingFrom(time, buffering_state)) {
      OnError(::media::PIPELINE_ERROR_ABORT);
      return;
    }
  }
}

void MediaPipelineImpl::Flush(const base::Closure& flush_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));
  DCHECK(audio_pipeline_ || video_pipeline_);
  DCHECK(!pending_flush_task_);

  buffering_controller_->Reset();

  // 1. Stop both the audio and video pipeline so that they stop feeding
  // buffers to the backend while the pipeline is being flushed.
  if (audio_pipeline_)
    audio_pipeline_->Stop();
  if (video_pipeline_)
    video_pipeline_->Stop();

  // 2. Stop the backend, so that the backend won't push their pending buffer,
  // which may be invalidated later, to hardware. (b/25342604)
  CHECK(media_pipeline_backend_->Stop());
  backend_state_ = BACKEND_STATE_INITIALIZED;

  // 3. Flush both the audio and video pipeline. This will flush the frame
  // provider and invalidate all the unreleased buffers.
  pending_flush_task_.reset(new FlushTask);
  pending_flush_task_->audio_flushed = !audio_pipeline_;
  pending_flush_task_->video_flushed = !video_pipeline_;
  pending_flush_task_->done_cb = flush_cb;
  if (audio_pipeline_) {
    audio_pipeline_->Flush(
        base::Bind(&MediaPipelineImpl::OnFlushDone, weak_this_, true));
  }
  if (video_pipeline_) {
    video_pipeline_->Flush(
        base::Bind(&MediaPipelineImpl::OnFlushDone, weak_this_, false));
  }
}

void MediaPipelineImpl::Stop() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_pipeline_ || video_pipeline_);

  // Cancel pending flush callbacks since we are about to stop/shutdown
  // audio/video pipelines. This will ensure A/V Flush won't happen in
  // stopped state.
  pending_flush_task_.reset();

  // Stop both the audio and video pipeline.
  if (audio_pipeline_)
    audio_pipeline_->Stop();
  if (video_pipeline_)
    video_pipeline_->Stop();

  // Release hardware resources on Stop.
  audio_pipeline_ = nullptr;
  video_pipeline_ = nullptr;
  audio_decoder_.reset();
  media_pipeline_backend_.reset();
  backend_state_ = BACKEND_STATE_UNINITIALIZED;
}

void MediaPipelineImpl::SetPlaybackRate(double rate) {
  CMALOG(kLogControl) << __FUNCTION__ << " rate=" << rate;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));

  playback_rate_ = rate;
  if (buffering_controller_ && buffering_controller_->IsBuffering())
    return;

  if (rate != 0.0f) {
    media_pipeline_backend_->SetPlaybackRate(rate);
    if (backend_state_ == BACKEND_STATE_PAUSED) {
      media_pipeline_backend_->Resume();
      backend_state_ = BACKEND_STATE_PLAYING;
    }
  } else if (backend_state_ == BACKEND_STATE_PLAYING) {
    media_pipeline_backend_->Pause();
    backend_state_ = BACKEND_STATE_PAUSED;
  }
}

void MediaPipelineImpl::SetVolume(float volume) {
  CMALOG(kLogControl) << __FUNCTION__ << " vol=" << volume;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_pipeline_)
    audio_pipeline_->SetVolume(volume);
}

base::TimeDelta MediaPipelineImpl::GetMediaTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return last_media_time_;
}

bool MediaPipelineImpl::HasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return audio_pipeline_ != nullptr;
}

bool MediaPipelineImpl::HasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return video_pipeline_ != nullptr;
}

void MediaPipelineImpl::OnFlushDone(bool is_audio_stream) {
  CMALOG(kLogControl) << __FUNCTION__ << " is_audio_stream=" << is_audio_stream;
  if (!pending_flush_task_)
    return;

  if (is_audio_stream) {
    DCHECK(!pending_flush_task_->audio_flushed);
    pending_flush_task_->audio_flushed = true;
  } else {
    DCHECK(!pending_flush_task_->video_flushed);
    pending_flush_task_->video_flushed = true;
  }

  if (pending_flush_task_->audio_flushed &&
      pending_flush_task_->video_flushed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  pending_flush_task_->done_cb);
    pending_flush_task_.reset();
  }
}

void MediaPipelineImpl::OnBufferingNotification(bool is_buffering) {
  CMALOG(kLogControl) << __FUNCTION__ << " is_buffering=" << is_buffering;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK((backend_state_ == BACKEND_STATE_PLAYING) ||
         (backend_state_ == BACKEND_STATE_PAUSED));
  DCHECK(buffering_controller_);
  DCHECK_EQ(is_buffering, buffering_controller_->IsBuffering());

  if (!client_.buffering_state_cb.is_null()) {
    if (is_buffering) {
      // TODO(alokp): WebMediaPlayerImpl currently only handles HAVE_ENOUGH.
      // See WebMediaPlayerImpl::OnPipelineBufferingStateChanged,
      // http://crbug.com/144683.
      LOG(WARNING) << "Ignoring buffering notification.";
    } else {
      client_.buffering_state_cb.Run(::media::BUFFERING_HAVE_ENOUGH);
    }
  }

  if (is_buffering && (backend_state_ == BACKEND_STATE_PLAYING)) {
    media_pipeline_backend_->Pause();
    backend_state_ = BACKEND_STATE_PAUSED;
  } else if (!is_buffering && (backend_state_ == BACKEND_STATE_PAUSED)) {
    // Once we finish buffering, we need to honour the desired playback rate
    // (rather than just resuming). This way, if playback was paused while
    // buffering, it will remain paused rather than incorrectly resuming.
    SetPlaybackRate(playback_rate_);
  }
}

void MediaPipelineImpl::UpdateMediaTime() {
  pending_time_update_task_ = false;
  if ((backend_state_ != BACKEND_STATE_PLAYING) &&
      (backend_state_ != BACKEND_STATE_PAUSED))
    return;

  if (statistics_rolling_counter_ == 0) {
    if (audio_pipeline_)
      audio_pipeline_->UpdateStatistics();
    if (video_pipeline_)
      video_pipeline_->UpdateStatistics();
  }
  statistics_rolling_counter_ =
      (statistics_rolling_counter_ + 1) % kStatisticsUpdatePeriod;

  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(
      media_pipeline_backend_->GetCurrentPts());
  if (media_time == ::media::kNoTimestamp()) {
    pending_time_update_task_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
        kTimeUpdateInterval);
    return;
  }
  base::TimeTicks stc = base::TimeTicks::Now();

  base::TimeDelta max_rendering_time = media_time;
  if (buffering_controller_) {
    buffering_controller_->SetMediaTime(media_time);

    // Receiving the same time twice in a row means playback isn't moving,
    // so don't interpolate ahead.
    if (media_time != last_media_time_) {
      max_rendering_time = buffering_controller_->GetMaxRenderingTime();
      if (max_rendering_time == ::media::kNoTimestamp())
        max_rendering_time = media_time;

      // Cap interpolation time to avoid interpolating too far ahead.
      max_rendering_time =
          std::min(max_rendering_time, media_time + 2 * kTimeUpdateInterval);
    }
  }

  last_media_time_ = media_time;
  if (!client_.time_update_cb.is_null())
    client_.time_update_cb.Run(media_time, max_rendering_time, stc);

  pending_time_update_task_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&MediaPipelineImpl::UpdateMediaTime, weak_this_),
      kTimeUpdateInterval);
}

void MediaPipelineImpl::OnError(::media::PipelineStatus error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(error, ::media::PIPELINE_OK) << "PIPELINE_OK is not an error!";
  if (!client_.error_cb.is_null())
    client_.error_cb.Run(error);
}

}  // namespace media
}  // namespace chromecast
