// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/jni_weak_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/ui/search/search_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sessions/core/session_id.h"
#include "components/toolbar/toolbar_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Profile;
class SkBitmap;

namespace cc {
class Layer;
}

namespace chrome {
struct NavigateParams;
}

namespace chrome {
namespace android {
class TabWebContentsDelegateAndroid;
class TabContentManager;
}
}

namespace content {
class ContentViewCore;
class WebContents;
}

namespace infobars {
class InfoBar;
}

namespace prerender {
class PrerenderManager;
}

class TabAndroid : public CoreTabHelperDelegate,
                   public InstantServiceObserver,
                   public SearchTabHelperDelegate,
                   public content::NotificationObserver,
                   public favicon::FaviconDriverObserver {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
  enum TabLoadStatus {
    PAGE_LOAD_FAILED = 0,
    DEFAULT_PAGE_LOAD = 1,
    PARTIAL_PRERENDERED_PAGE_LOAD = 2,
    FULL_PRERENDERED_PAGE_LOAD = 3,
  };

  // Convenience method to retrieve the Tab associated with the passed
  // WebContents.  Can return NULL.
  static TabAndroid* FromWebContents(content::WebContents* web_contents);

  // Returns the native TabAndroid stored in the Java Tab represented by
  // |obj|.
  static TabAndroid* GetNativeTab(JNIEnv* env, jobject obj);

  // Function to attach helpers to the contentView.
  static void AttachTabHelpers(content::WebContents* web_contents);

  TabAndroid(JNIEnv* env, jobject obj);
  ~TabAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Return the WebContents, if any, currently owned by this TabAndroid.
  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Return the cc::Layer that represents the content for this TabAndroid.
  scoped_refptr<cc::Layer> GetContentLayer() const;

  // Return specific id information regarding this TabAndroid.
  const SessionID& session_id() const { return session_tab_id_; }
  int GetAndroidId() const;
  int GetSyncId() const;

  // Return the tab title.
  base::string16 GetTitle() const;

  // Return the tab url.
  GURL GetURL() const;

  // Load the tab if it was unloaded from memory.
  bool LoadIfNeeded();

  // Helper methods to make it easier to access objects from the associated
  // WebContents.  Can return NULL.
  content::ContentViewCore* GetContentViewCore() const;
  Profile* GetProfile() const;
  browser_sync::SyncedTabDelegate* GetSyncedTabDelegate() const;

  void SetWindowSessionID(SessionID::id_type window_id);
  void SetSyncId(int sync_id);

  void HandlePopupNavigation(chrome::NavigateParams* params);

  bool HasPrerenderedUrl(GURL gurl);

  bool HasOfflinePages() const;
  void ShowOfflinePages();
  void LoadOfflineCopy(const GURL& url);

  // Notifies this TabAndroid that a Lo-Fi response has been received. The
  // TabAndroid then handles showing Lo-Fi UI if this is the first Lo-Fi
  // response for a page load. |is_preview| indicates whether the response was a
  // Lo-Fi preview response.
  void OnLoFiResponseReceived(bool is_preview);

  // Overridden from CoreTabHelperDelegate:
  void SwapTabContents(content::WebContents* old_contents,
                       content::WebContents* new_contents,
                       bool did_start_load,
                       bool did_finish_load) override;

  // Overridden from InstantServiceObserver:
  void DefaultSearchProviderChanged(
      bool google_base_url_domain_changed) override;

  // Overridden from SearchTabHelperDelegate:
  void OnWebContentsInstantSupportDisabled(
      const content::WebContents* web_contents) override;

  // Overridden from NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Overridden from favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // Methods called from Java via JNI -----------------------------------------

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void InitWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean incognito,
      const base::android::JavaParamRef<jobject>& jcontent_view_core,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
      const base::android::JavaParamRef<jobject>& jcontext_menu_populator);
  void UpdateDelegates(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& obj,
        const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
        const base::android::JavaParamRef<jobject>& jcontext_menu_populator);
  void DestroyWebContents(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean delete_native);
  base::android::ScopedJavaLocalRef<jobject> GetProfileAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  TabLoadStatus LoadUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& j_extra_headers,
      const base::android::JavaParamRef<jbyteArray>& j_post_data,
      jint page_transition,
      const base::android::JavaParamRef<jstring>& j_referrer_url,
      jint referrer_policy,
      jboolean is_renderer_initiated,
      jboolean should_replace_current_entry,
      jlong intent_received_timestamp,
      jboolean has_user_gesture);
  void SetActiveNavigationEntryTitleForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle);
  bool Print(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Sets the tab as content to be printed through JNI.
  void SetPendingPrint();

  // Called to get default favicon of current tab, return null if no
  // favicon is avaliable for current tab.
  base::android::ScopedJavaLocalRef<jobject> GetFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void CreateHistoricalTab(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  static void CreateHistoricalTabFromContents(
      content::WebContents* web_contents);

  void UpdateTopControlsState(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jint constraints,
                              jint current,
                              jboolean animate);

  void LoadOriginalImage(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  jlong GetBookmarkId(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jboolean only_editable);

  jboolean HasOfflineCopy(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  jboolean IsOfflinePage(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> GetOfflinePageOriginalUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SetInterceptNavigationDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& delegate);

  // TODO(dtrainor): Remove this, pull content_layer() on demand.
  void AttachToTabContentManager(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jtab_content_manager);

  void AttachOverlayContentViewCore(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jcontent_view_core,
      jboolean visible);

  void DetachOverlayContentViewCore(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jcontent_view_core);

  bool HasPrerenderedUrl(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& url);

  // Register the Tab's native methods through JNI.
  static bool RegisterTabAndroid(JNIEnv* env);

 private:
  prerender::PrerenderManager* GetPrerenderManager() const;

  int64_t GetBookmarkIdHelper(bool only_editable) const;

  JavaObjectWeakGlobalRef weak_java_tab_;

  // The identifier used by session restore for this tab.
  SessionID session_tab_id_;

  // Identifier of the window the tab is in.
  SessionID session_window_id_;

  content::NotificationRegistrar notification_registrar_;

  scoped_refptr<cc::Layer> content_layer_;
  chrome::android::TabContentManager* tab_content_manager_;

  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<chrome::android::TabWebContentsDelegateAndroid>
      web_contents_delegate_;

  scoped_ptr<browser_sync::SyncedTabDelegateAndroid> synced_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
