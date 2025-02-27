// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "jni/MediaPlayerBridge_jni.h"
#include "media/base/android/media_common_android.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/android/media_url_interceptor.h"
#include "media/base/timestamp_constants.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace media {

namespace {

enum UMAExitStatus {
  UMA_EXIT_SUCCESS = 0,
  UMA_EXIT_ERROR,
  UMA_EXIT_STATUS_MAX = UMA_EXIT_ERROR,
};

}  // namespace

MediaPlayerBridge::MediaPlayerBridge(
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& user_agent,
    bool hide_url_log,
    MediaPlayerManager* manager,
    const OnDecoderResourcesReleasedCB& on_decoder_resources_released_cb,
    const GURL& frame_url,
    bool allow_credentials)
    : MediaPlayerAndroid(player_id,
                         manager,
                         on_decoder_resources_released_cb,
                         frame_url),
      prepared_(false),
      pending_play_(false),
      should_seek_on_prepare_(false),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      user_agent_(user_agent),
      hide_url_log_(hide_url_log),
      width_(0),
      height_(0),
      can_pause_(true),
      can_seek_forward_(true),
      can_seek_backward_(true),
      allow_credentials_(allow_credentials),
      is_active_(false),
      has_error_(false),
      has_ever_started_(false),
      weak_factory_(this) {}

MediaPlayerBridge::~MediaPlayerBridge() {
  if (!j_media_player_bridge_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    CHECK(env);
    Java_MediaPlayerBridge_destroy(env, j_media_player_bridge_.obj());
  }
  Release();

  if (has_ever_started_) {
    UMA_HISTOGRAM_ENUMERATION("Media.Android.MediaPlayerSuccess",
                              has_error_ ? UMA_EXIT_ERROR : UMA_EXIT_SUCCESS,
                              UMA_EXIT_STATUS_MAX + 1);
  }
}

void MediaPlayerBridge::Initialize() {
  cookies_.clear();
  if (url_.SchemeIsFile() || url_.SchemeIs("data") || url_.SchemeIs("app")) {
    ExtractMediaMetadata(url_.spec());
    return;
  }

  media::MediaResourceGetter* resource_getter =
      manager()->GetMediaResourceGetter();
  if (url_.SchemeIsFileSystem() || url_.SchemeIsBlob()) {
    resource_getter->GetPlatformPathFromURL(
        url_,
        base::Bind(&MediaPlayerBridge::ExtractMediaMetadata,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Start extracting the metadata immediately if the request is anonymous.
  // Otherwise, wait for user credentials to be retrieved first.
  if (!allow_credentials_) {
    ExtractMediaMetadata(url_.spec());
    return;
  }

  resource_getter->GetCookies(url_,
                              first_party_for_cookies_,
                              base::Bind(&MediaPlayerBridge::OnCookiesRetrieved,
                                         weak_factory_.GetWeakPtr()));
}

void MediaPlayerBridge::CreateJavaMediaPlayerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_player_bridge_.Reset(Java_MediaPlayerBridge_create(
      env, reinterpret_cast<intptr_t>(this)));

  UpdateEffectiveVolume();

  AttachListener(j_media_player_bridge_.obj());
}

void MediaPlayerBridge::SetDuration(base::TimeDelta duration) {
  duration_ = duration;
}

void MediaPlayerBridge::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  surface_ = std::move(surface);

  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setSurface(
      env, j_media_player_bridge_.obj(), surface_.j_surface().obj());
}

void MediaPlayerBridge::Prepare() {
  DCHECK(j_media_player_bridge_.is_null());
  CreateJavaMediaPlayerBridge();
  if (url_.SchemeIsFileSystem() || url_.SchemeIsBlob()) {
    manager()->GetMediaResourceGetter()->GetPlatformPathFromURL(
        url_,
        base::Bind(&MediaPlayerBridge::SetDataSource,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  SetDataSource(url_.spec());
}

void MediaPlayerBridge::SetDataSource(const std::string& url) {
  if (j_media_player_bridge_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  int fd;
  int64_t offset;
  int64_t size;
  if (InterceptMediaUrl(url, &fd, &offset, &size)) {
    if (!Java_MediaPlayerBridge_setDataSourceFromFd(
        env, j_media_player_bridge_.obj(), fd, offset, size)) {
      OnMediaError(MEDIA_ERROR_FORMAT);
      return;
    }
  } else {
    // Create a Java String for the URL.
    ScopedJavaLocalRef<jstring> j_url_string =
        ConvertUTF8ToJavaString(env, url);

    jobject j_context = base::android::GetApplicationContext();
    DCHECK(j_context);

    const std::string data_uri_prefix("data:");
    if (base::StartsWith(url, data_uri_prefix, base::CompareCase::SENSITIVE)) {
      if (!Java_MediaPlayerBridge_setDataUriDataSource(
          env, j_media_player_bridge_.obj(), j_context, j_url_string.obj())) {
        OnMediaError(MEDIA_ERROR_FORMAT);
      }
      return;
    }

    ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(
        env, cookies_);
    ScopedJavaLocalRef<jstring> j_user_agent = ConvertUTF8ToJavaString(
        env, user_agent_);

    if (!Java_MediaPlayerBridge_setDataSource(
        env, j_media_player_bridge_.obj(), j_context, j_url_string.obj(),
        j_cookies.obj(), j_user_agent.obj(), hide_url_log_)) {
      OnMediaError(MEDIA_ERROR_FORMAT);
      return;
    }
  }

  if (!Java_MediaPlayerBridge_prepareAsync(env, j_media_player_bridge_.obj()))
    OnMediaError(MEDIA_ERROR_FORMAT);
}

bool MediaPlayerBridge::InterceptMediaUrl(const std::string& url,
                                          int* fd,
                                          int64_t* offset,
                                          int64_t* size) {
  // Sentinel value to check whether the output arguments have been set.
  const int kUnsetValue = -1;

  *fd = kUnsetValue;
  *offset = kUnsetValue;
  *size = kUnsetValue;
  media::MediaUrlInterceptor* url_interceptor =
      manager()->GetMediaUrlInterceptor();
  if (url_interceptor && url_interceptor->Intercept(url, fd, offset, size)) {
    DCHECK_NE(kUnsetValue, *fd);
    DCHECK_NE(kUnsetValue, *offset);
    DCHECK_NE(kUnsetValue, *size);
    return true;
  }
  return false;
}

void MediaPlayerBridge::OnDidSetDataUriDataSource(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean success) {
  if (!success) {
    OnMediaError(MEDIA_ERROR_FORMAT);
    return;
  }

  if (!Java_MediaPlayerBridge_prepareAsync(env, j_media_player_bridge_.obj()))
    OnMediaError(MEDIA_ERROR_FORMAT);
}

void MediaPlayerBridge::OnCookiesRetrieved(const std::string& cookies) {
  cookies_ = cookies;
  manager()->GetMediaResourceGetter()->GetAuthCredentials(
      url_,
      base::Bind(&MediaPlayerBridge::OnAuthCredentialsRetrieved,
                 weak_factory_.GetWeakPtr()));
}

void MediaPlayerBridge::OnAuthCredentialsRetrieved(
    const base::string16& username, const base::string16& password) {
  GURL::ReplacementsW replacements;
  if (!username.empty()) {
    replacements.SetUsernameStr(username);
    if (!password.empty())
      replacements.SetPasswordStr(password);
    url_ = url_.ReplaceComponents(replacements);
  }
  ExtractMediaMetadata(url_.spec());
}

void MediaPlayerBridge::ExtractMediaMetadata(const std::string& url) {
  if (url.empty()) {
    OnMediaError(MEDIA_ERROR_FORMAT);
    on_decoder_resources_released_cb_.Run(player_id());
    return;
  }

  int fd;
  int64_t offset;
  int64_t size;
  if (InterceptMediaUrl(url, &fd, &offset, &size)) {
    manager()->GetMediaResourceGetter()->ExtractMediaMetadata(
        fd, offset, size,
        base::Bind(&MediaPlayerBridge::OnMediaMetadataExtracted,
                   weak_factory_.GetWeakPtr()));
  } else {
    manager()->GetMediaResourceGetter()->ExtractMediaMetadata(
        url, cookies_, user_agent_,
        base::Bind(&MediaPlayerBridge::OnMediaMetadataExtracted,
                   weak_factory_.GetWeakPtr()));
  }
}

void MediaPlayerBridge::OnMediaMetadataExtracted(
    base::TimeDelta duration, int width, int height, bool success) {
  if (success) {
    duration_ = duration;
    width_ = width;
    height_ = height;
  }
  manager()->OnMediaMetadataChanged(
      player_id(), duration_, width_, height_, success);
  on_decoder_resources_released_cb_.Run(player_id());
}

void MediaPlayerBridge::Start() {
  // A second Start() call after an error is considered another attempt for UMA
  // and causes UMA reporting.
  if (has_ever_started_ && has_error_) {
    UMA_HISTOGRAM_ENUMERATION("Media.Android.MediaPlayerSuccess",
                              UMA_EXIT_ERROR, UMA_EXIT_STATUS_MAX + 1);
  }

  has_ever_started_ = true;
  has_error_ = false;
  is_active_ = true;

  if (j_media_player_bridge_.is_null()) {
    pending_play_ = true;
    Prepare();
  } else {
    if (prepared_)
      StartInternal();
    else
      pending_play_ = true;
  }
}

void MediaPlayerBridge::Pause(bool is_media_related_action) {
  if (j_media_player_bridge_.is_null()) {
    pending_play_ = false;
  } else {
    if (prepared_ && IsPlaying())
      PauseInternal();
    else
      pending_play_ = false;
  }

  is_active_ = false;
}

bool MediaPlayerBridge::IsPlaying() {
  if (!prepared_)
    return pending_play_;

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  jboolean result = Java_MediaPlayerBridge_isPlaying(
      env, j_media_player_bridge_.obj());
  return result;
}

bool MediaPlayerBridge::HasVideo() const {
  DCHECK(prepared_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_hasVideo(env, j_media_player_bridge_.obj());
}

bool MediaPlayerBridge::HasAudio() const {
  DCHECK(prepared_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_hasAudio(env, j_media_player_bridge_.obj());
}

int MediaPlayerBridge::GetVideoWidth() {
  if (!prepared_)
    return width_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_getVideoWidth(
      env, j_media_player_bridge_.obj());
}

int MediaPlayerBridge::GetVideoHeight() {
  if (!prepared_)
    return height_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaPlayerBridge_getVideoHeight(
      env, j_media_player_bridge_.obj());
}

void MediaPlayerBridge::SeekTo(base::TimeDelta timestamp) {
  // Record the time to seek when OnMediaPrepared() is called.
  pending_seek_ = timestamp;
  should_seek_on_prepare_ = true;

  if (prepared_)
    SeekInternal(GetCurrentTime(), timestamp);
}

base::TimeDelta MediaPlayerBridge::GetCurrentTime() {
  if (!prepared_)
    return pending_seek_;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeDelta::FromMilliseconds(
      Java_MediaPlayerBridge_getCurrentPosition(
          env, j_media_player_bridge_.obj()));
}

base::TimeDelta MediaPlayerBridge::GetDuration() {
  if (!prepared_)
    return duration_;
  JNIEnv* env = base::android::AttachCurrentThread();
  const int duration_ms =
      Java_MediaPlayerBridge_getDuration(env, j_media_player_bridge_.obj());
  return duration_ms < 0 ? media::kInfiniteDuration()
                         : base::TimeDelta::FromMilliseconds(duration_ms);
}

void MediaPlayerBridge::Release() {
  is_active_ = false;

  on_decoder_resources_released_cb_.Run(player_id());
  if (j_media_player_bridge_.is_null())
    return;

  time_update_timer_.Stop();
  if (prepared_) {
    pending_seek_ = GetCurrentTime();
    should_seek_on_prepare_ = true;
  }

  prepared_ = false;
  pending_play_ = false;
  SetVideoSurface(gfx::ScopedJavaSurface());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_release(env, j_media_player_bridge_.obj());
  j_media_player_bridge_.Reset();
  DetachListener();
}

void MediaPlayerBridge::UpdateEffectiveVolumeInternal(double effective_volume) {
  if (j_media_player_bridge_.is_null()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  Java_MediaPlayerBridge_setVolume(env, j_media_player_bridge_.obj(),
                                   effective_volume);
}

void MediaPlayerBridge::OnVideoSizeChanged(int width, int height) {
  width_ = width;
  height_ = height;
  MediaPlayerAndroid::OnVideoSizeChanged(width, height);
}

void MediaPlayerBridge::OnMediaError(int error_type) {
  // Gather errors for UMA only in the active state.
  // The MEDIA_ERROR_INVALID_CODE is reported by MediaPlayerListener.java in
  // the situations that are considered normal, and is ignored by upper level.
  if (is_active_ && error_type != MEDIA_ERROR_INVALID_CODE)
    has_error_ = true;

  // Do not propagate MEDIA_ERROR_SERVER_DIED. If it happens in the active state
  // we want the playback to stall. It can be recovered by pressing the Play
  // button again.
  if (error_type == MEDIA_ERROR_SERVER_DIED)
    error_type = MEDIA_ERROR_INVALID_CODE;

  MediaPlayerAndroid::OnMediaError(error_type);
}

void MediaPlayerBridge::OnPlaybackComplete() {
  time_update_timer_.Stop();
  MediaPlayerAndroid::OnPlaybackComplete();
}

void MediaPlayerBridge::OnMediaInterrupted() {
  time_update_timer_.Stop();
  MediaPlayerAndroid::OnMediaInterrupted();
}

void MediaPlayerBridge::OnMediaPrepared() {
  if (j_media_player_bridge_.is_null())
    return;

  prepared_ = true;
  duration_ = GetDuration();

  // If media player was recovered from a saved state, consume all the pending
  // events.
  if (should_seek_on_prepare_) {
    PendingSeekInternal(pending_seek_);
    pending_seek_ = base::TimeDelta::FromMilliseconds(0);
    should_seek_on_prepare_ = false;
  }

  if (!surface_.IsEmpty())
    SetVideoSurface(std::move(surface_));

  if (pending_play_) {
    StartInternal();
    pending_play_ = false;
  }

  UpdateAllowedOperations();
  manager()->OnMediaMetadataChanged(
      player_id(), duration_, width_, height_, true);
}

ScopedJavaLocalRef<jobject> MediaPlayerBridge::GetAllowedOperations() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  return Java_MediaPlayerBridge_getAllowedOperations(
      env, j_media_player_bridge_.obj());
}

void MediaPlayerBridge::UpdateAllowedOperations() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jobject> allowedOperations = GetAllowedOperations();

  can_pause_ = Java_AllowedOperations_canPause(env, allowedOperations.obj());
  can_seek_forward_ = Java_AllowedOperations_canSeekForward(
      env, allowedOperations.obj());
  can_seek_backward_ = Java_AllowedOperations_canSeekBackward(
      env, allowedOperations.obj());
}

void MediaPlayerBridge::StartInternal() {
  if (!manager()->RequestPlay(player_id(), duration_, HasAudio())) {
    Pause(true);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_start(env, j_media_player_bridge_.obj());
  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kTimeUpdateInterval),
        this, &MediaPlayerBridge::OnTimeUpdateTimerFired);
  }
}

void MediaPlayerBridge::PauseInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaPlayerBridge_pause(env, j_media_player_bridge_.obj());
  time_update_timer_.Stop();
}

void MediaPlayerBridge::PendingSeekInternal(const base::TimeDelta& time) {
  SeekInternal(GetCurrentTime(), time);
}

bool MediaPlayerBridge::SeekInternal(base::TimeDelta current_time,
                                     base::TimeDelta time) {
  // Seeking on content like live streams may cause the media player to
  // get stuck in an error state.
  if (time < current_time && !CanSeekBackward())
    return false;

  if (time >= current_time && !CanSeekForward())
    return false;

  if (time > duration_)
    time = duration_;

  // Seeking to an invalid position may cause media player to stuck in an
  // error state.
  if (time < base::TimeDelta()) {
    DCHECK_EQ(-1.0, time.InMillisecondsF());
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  int time_msec = static_cast<int>(time.InMilliseconds());
  Java_MediaPlayerBridge_seekTo(
      env, j_media_player_bridge_.obj(), time_msec);
  return true;
}

void MediaPlayerBridge::OnTimeUpdateTimerFired() {
  base::TimeDelta current_timestamp = GetCurrentTime();
  if (last_time_update_timestamp_ == current_timestamp)
    return;
  manager()->OnTimeUpdate(player_id(), current_timestamp,
                          base::TimeTicks::Now());
  last_time_update_timestamp_ = current_timestamp;
}

bool MediaPlayerBridge::RegisterMediaPlayerBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool MediaPlayerBridge::CanPause() {
  return can_pause_;
}

bool MediaPlayerBridge::CanSeekForward() {
  return can_seek_forward_;
}

bool MediaPlayerBridge::CanSeekBackward() {
  return can_seek_backward_;
}

bool MediaPlayerBridge::IsPlayerReady() {
  return prepared_;
}

GURL MediaPlayerBridge::GetUrl() {
  return url_;
}

GURL MediaPlayerBridge::GetFirstPartyForCookies() {
  return first_party_for_cookies_;
}

}  // namespace media
