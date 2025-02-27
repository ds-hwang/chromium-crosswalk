// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioManager;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * MediaSession is the Java counterpart of content::MediaSession.
 * It is being used to communicate from content::MediaSession (C++) to the
 * Android system. A MediaSession is implementing OnAudioFocusChangeListener,
 * making it an audio focus holder for Android. Thus two instances of
 * MediaSession can't have audio focus at the same time.
 * A MediaSession will use the type requested from its C++ counterpart and will
 * resume its play using the same type if it were to happen, for example, when
 * it got temporarily suspended by a transient sound like a notification.
 */
@JNINamespace("content")
public class MediaSession implements AudioManager.OnAudioFocusChangeListener {
    private static final String TAG = "cr.MediaSession";

    // These need to match the values in native apps.
    public static final double DUCKING_VOLUME_MULTIPLIER = 0.2f;
    public static final double DEFAULT_VOLUME_MULTIPLIER = 1.0f;

    private Context mContext;
    private int mFocusType;
    private boolean mIsDucking = false;

    // Native pointer to C++ content::MediaSession.
    private final long mNativeMediaSession;

    private MediaSession(final Context context, long nativeMediaSession) {
        mContext = context;
        mNativeMediaSession = nativeMediaSession;
    }

    @CalledByNative
    private static MediaSession createMediaSession(Context context, long nativeMediaSession) {
        return new MediaSession(context, nativeMediaSession);
    }

    @CalledByNative
    private boolean requestAudioFocus(boolean transientFocus) {
        mFocusType = transientFocus ? AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK
                : AudioManager.AUDIOFOCUS_GAIN;
        return requestAudioFocusInternal();
    }

    @CalledByNative
    private void abandonAudioFocus() {
        AudioManager am = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        am.abandonAudioFocus(this);
    }

    private boolean requestAudioFocusInternal() {
        AudioManager am = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        int result = am.requestAudioFocus(this, AudioManager.STREAM_MUSIC, mFocusType);
        return result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    @Override
    public void onAudioFocusChange(int focusChange) {
        switch (focusChange) {
            case AudioManager.AUDIOFOCUS_GAIN:
                if (mIsDucking) {
                    nativeOnSetVolumeMultiplier(mNativeMediaSession, DEFAULT_VOLUME_MULTIPLIER);
                    mIsDucking = false;
                } else {
                    nativeOnResume(mNativeMediaSession);
                }
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                nativeOnSuspend(mNativeMediaSession, true);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                mIsDucking = true;
                nativeRecordSessionDuck(mNativeMediaSession);
                nativeOnSetVolumeMultiplier(mNativeMediaSession, DUCKING_VOLUME_MULTIPLIER);
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                abandonAudioFocus();
                nativeOnSuspend(mNativeMediaSession, false);
                break;
            default:
                Log.w(TAG, "onAudioFocusChange called with unexpected value %d", focusChange);
                break;
        }
    }

    private native void nativeOnSuspend(long nativeMediaSession, boolean temporary);
    private native void nativeOnResume(long nativeMediaSession);
    private native void nativeOnSetVolumeMultiplier(
            long nativeMediaSession, double volumeMultiplier);
    private native void nativeRecordSessionDuck(long nativeMediaSession);
}
