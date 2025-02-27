// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/download/download_request_infobar_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#else
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_factory.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

using content::WebContents;

namespace {
enum TestingAction {
  ACCEPT,
  CANCEL,
  WAIT
};

#if defined(OS_ANDROID)
class TestingDelegate {
 public:
  void SetUp(WebContents* web_contents) {
    InfoBarService::CreateForWebContents(web_contents);
    fake_create_callback_ =
        base::Bind(&TestingDelegate::FakeCreate, base::Unretained(this));
    DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(
        &fake_create_callback_);
    ResetCounts();
  }

  void TearDown() { UnsetInfobarDelegate(); }

  void LoadCompleted(WebContents* /*web_contents*/) {
    // No action needed on OS_ANDROID.
  }

  void ResetCounts() { ask_allow_count_ = 0; }

  int AllowCount() { return ask_allow_count_; }

  void UpdateExpectations(TestingAction action) { testing_action_ = action; }

  void FakeCreate(
      InfoBarService* infobar_service,
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
    ask_allow_count_++;
    switch (testing_action_) {
      case ACCEPT:
        host->Accept();
        break;
      case CANCEL:
        host->Cancel();
        break;
      case WAIT:
        break;
    }
  }

  void UnsetInfobarDelegate() {
    DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(nullptr);
  }

 private:
  // Number of times ShouldAllowDownload was invoked.
  int ask_allow_count_;

  // The action that FakeCreate() should take.
  TestingAction testing_action_;

  DownloadRequestInfoBarDelegateAndroid::FakeCreateCallback
      fake_create_callback_;
};
#else
class TestingDelegate {
 public:
  void SetUp(WebContents* web_contents) {
    PermissionBubbleManager::CreateForWebContents(web_contents);
    mock_permission_bubble_factory_.reset(new MockPermissionBubbleFactory(
        false, PermissionBubbleManager::FromWebContents(web_contents)));
    PermissionBubbleManager::FromWebContents(web_contents)
        ->DisplayPendingRequests();
  }

  void TearDown() { mock_permission_bubble_factory_.reset(); }

  void LoadCompleted(WebContents* web_contents) {
    mock_permission_bubble_factory_->DocumentOnLoadCompletedInMainFrame();
  }

  void ResetCounts() { mock_permission_bubble_factory_->ResetCounts(); }

  int AllowCount() { return mock_permission_bubble_factory_->show_count(); }

  void UpdateExpectations(TestingAction action) {
    // Set expectations for PermissionBubbleManager.
    if (action == ACCEPT) {
      mock_permission_bubble_factory_->set_response_type(
          PermissionBubbleManager::ACCEPT_ALL);
    } else if (action == CANCEL) {
      mock_permission_bubble_factory_->set_response_type(
          PermissionBubbleManager::DENY_ALL);
    } else if (action == WAIT) {
      mock_permission_bubble_factory_->set_response_type(
          PermissionBubbleManager::NONE);
    } else {
      mock_permission_bubble_factory_->set_response_type(
          PermissionBubbleManager::DISMISS);
    }
  }

 private:
  scoped_ptr<MockPermissionBubbleFactory> mock_permission_bubble_factory_;
};
#endif
}  // namespace

class DownloadRequestLimiterTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_.reset(new TestingProfile());
    testing_delegate_.SetUp(web_contents());

    UpdateExpectations(ACCEPT);
    cancel_count_ = continue_count_ = 0;
    download_request_limiter_ = new DownloadRequestLimiter();

    content_settings_ = new HostContentSettingsMap(
        profile_->GetPrefs(), false /* incognito_profile */,
        false /* guest_profile */);
    DownloadRequestLimiter::SetContentSettingsForTesting(
        content_settings_.get());
  }

  void TearDown() override {
    content_settings_->ShutdownOnUIThread();
    content_settings_ = nullptr;
    testing_delegate_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CanDownload() {
    CanDownloadFor(web_contents());
  }

  void CanDownloadFor(WebContents* web_contents) {
    download_request_limiter_->CanDownloadImpl(
        web_contents,
        "GET",  // request method
        base::Bind(&DownloadRequestLimiterTest::ContinueDownload,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void OnUserInteraction(blink::WebInputEvent::Type type) {
    OnUserInteractionFor(web_contents(), type);
  }

  void OnUserInteractionFor(WebContents* web_contents,
                            blink::WebInputEvent::Type type) {
    DownloadRequestLimiter::TabDownloadState* state =
        download_request_limiter_->GetDownloadState(web_contents, nullptr,
                                                    false);
    if (state)
      state->DidGetUserInteraction(type);
  }

  void ExpectAndResetCounts(
      int expect_continues,
      int expect_cancels,
      int expect_asks,
      int line) {
    EXPECT_EQ(expect_continues, continue_count_) << "line " << line;
    EXPECT_EQ(expect_cancels, cancel_count_) << "line " << line;
    EXPECT_EQ(expect_asks, AskAllowCount()) << "line " << line;
    continue_count_ = cancel_count_ = 0;
    testing_delegate_.ResetCounts();
  }

  void UpdateContentSettings(WebContents* web_contents,
                             ContentSetting setting) {
    // Ensure a download state exists.
    download_request_limiter_->GetDownloadState(web_contents, nullptr, true);
    SetHostContentSetting(web_contents, setting);

    // Manually send the update notification. In the browser, this is sent from
    // ContentSettingRPHBubbleModel.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
        content::Source<WebContents>(web_contents),
        content::NotificationService::NoDetails());
  }

 protected:
  void ContinueDownload(bool allow) {
    if (allow) {
      continue_count_++;
    } else {
      cancel_count_++;
    }
  }

  void SetHostContentSetting(WebContents* contents, ContentSetting setting) {
    content_settings_->SetContentSetting(
        ContentSettingsPattern::FromURL(contents->GetURL()),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
        std::string(),
        setting);
  }

  void LoadCompleted() { testing_delegate_.LoadCompleted(web_contents()); }

  int AskAllowCount() { return testing_delegate_.AllowCount(); }

  void UpdateExpectations(TestingAction action) {
    testing_delegate_.UpdateExpectations(action);
  }

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  scoped_refptr<HostContentSettingsMap> content_settings_;
  TestingDelegate testing_delegate_;

 private:
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_Allow) {
  LoadCompleted();

  // All tabs should initially start at ALLOW_ONE_DOWNLOAD.
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask if the tab can do a download. This moves to PROMPT_BEFORE_DOWNLOAD.
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  // Ask again. This triggers asking the delegate for allow/disallow.
  UpdateExpectations(ACCEPT);
  CanDownload();
  // This should ask us if the download is allowed.
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask again and make sure continue is invoked.
  CanDownload();
  // The state is at allow_all, which means the delegate shouldn't be asked.
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnNavigation) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(ACCEPT);
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://foo.com/bar2"));
  LoadCompleted();
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  OnUserInteraction(blink::WebInputEvent::RawKeyDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a completely different host, which should reset the state.
  NavigateAndCommit(GURL("http://fooey.com"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://fooey.com/bar2"));
  LoadCompleted();
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnUserGesture) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture with mouse scroll, which should be ignored.
  OnUserInteraction(blink::WebInputEvent::MouseWheel);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // Do a user gesture with mouse click, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::MouseDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture with gesture tap, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::GestureTapDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture with keyboard down, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::RawKeyDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  // A user gesture now should NOT change the state.
  OnUserInteraction(blink::WebInputEvent::MouseDown);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // And make sure we really can't download.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  // And the state shouldn't have changed.
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnReload) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // If the user refreshes the page without responding to the infobar, pretend
  // like the refresh is the initial load: they get 1 free download (probably
  // the same as the actual initial load), then an infobar.
  UpdateExpectations(WAIT);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  Reload();
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  UpdateExpectations(CANCEL);
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  Reload();
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

#if defined(OS_ANDROID)
TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_RawWebContents) {
  scoped_ptr<WebContents> web_contents(CreateTestWebContents());

  // DownloadRequestLimiter won't try to make a permission bubble if there's
  // no permission bubble manager, so don't put one on the test WebContents.

  // DownloadRequestLimiter won't try to make an infobar if it doesn't have an
  // InfoBarService, and we want to test that it will Cancel() instead of
  // prompting when it doesn't have a InfoBarService, so unset the delegate.
  testing_delegate_.UnsetInfobarDelegate();
  ExpectAndResetCounts(0, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  // You get one freebie.
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserInteractionFor(web_contents.get(),
                       blink::WebInputEvent::GestureTapDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserInteractionFor(web_contents.get(), blink::WebInputEvent::RawKeyDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
}
#endif

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_SetHostContentSetting) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  SetHostContentSetting(web_contents(), CONTENT_SETTING_ALLOW);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  SetHostContentSetting(web_contents(), CONTENT_SETTING_BLOCK);

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_ContentSettingChanged) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Simulate an accidental deny.
  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to allow and send the notification. Ensure that the
  // limiter states update to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ALLOW);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask to download, and assert that it succeeded and we are still in allow.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to block and send the notification. Ensure that the
  // limiter states updates to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_BLOCK);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ensure downloads are blocked.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Reset to ask. Verify that the download counts have not changed on the
  // content settings change (ensuring there is no "free" download after
  // changing the content setting).
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ASK);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  UpdateExpectations(WAIT);
  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}
