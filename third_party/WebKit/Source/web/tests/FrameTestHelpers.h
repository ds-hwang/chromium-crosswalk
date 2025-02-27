/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameTestHelpers_h
#define FrameTestHelpers_h

#include "core/frame/Settings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebViewClient.h"
#include "web/WebViewImpl.h"
#include "wtf/PassOwnPtr.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace blink {

class WebFrame;
class WebFrameWidget;
class WebLocalFrame;
class WebRemoteFrame;
class WebRemoteFrameImpl;

namespace FrameTestHelpers {

class TestWebFrameClient;

// Loads a url into the specified WebFrame for testing purposes. Pumps any
// pending resource requests, as well as waiting for the threaded parser to
// finish, before returning.
void loadFrame(WebFrame*, const std::string& url);
// Same as above, but for WebFrame::loadHTMLString().
void loadHTMLString(WebFrame*, const std::string& html, const WebURL& baseURL);
// Same as above, but for WebFrame::loadHistoryItem().
void loadHistoryItem(WebFrame*, const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy);
// Same as above, but for WebFrame::reload().
void reloadFrame(WebFrame*);
void reloadFrameIgnoringCache(WebFrame*);

// Pumps pending resource requests while waiting for a frame to load. Don't use
// this. Use one of the above helpers.
void pumpPendingRequestsDoNotUse(WebFrame*);

// Calls WebRemoteFrame::createLocalChild, but with some arguments prefilled
// with default test values (i.e. with a default |client| or |properties| and/or
// with a precalculated |uniqueName|).
WebLocalFrame* createLocalChild(WebRemoteFrame* parent, const WebString& name = WebString::fromUTF8("frameName"), WebFrameClient* = nullptr, WebFrame* previousSibling = nullptr, const WebFrameOwnerProperties& = WebFrameOwnerProperties());

class SettingOverrider {
public:
    virtual void overrideSettings(WebSettings*) = 0;
};

// Forces to use mocked overlay scrollbars instead of the default native theme scrollbars to avoid
// crash in Chromium code when it tries to load UI resources that are not available when running
// blink unit tests, and to ensure consistent layout regardless of differences between scrollbar themes.
// WebViewHelper includes this, so this is only needed if a test doesn't use WebViewHelper or the test
// needs a bigger scope of mock scrollbar settings than the scope of WebViewHelper.
class UseMockScrollbarSettings {
public:
    UseMockScrollbarSettings()
        : m_originalMockScrollbarEnabled(Settings::mockScrollbarsEnabled())
        , m_originalOverlayScrollbarsEnabled(RuntimeEnabledFeatures::overlayScrollbarsEnabled())
    {
        Settings::setMockScrollbarsEnabled(true);
        RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
        EXPECT_TRUE(ScrollbarTheme::theme().usesOverlayScrollbars());
    }

    ~UseMockScrollbarSettings()
    {
        Settings::setMockScrollbarsEnabled(m_originalMockScrollbarEnabled);
        RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_originalOverlayScrollbarsEnabled);
    }

private:
    bool m_originalMockScrollbarEnabled;
    bool m_originalOverlayScrollbarsEnabled;
};

class TestWebViewClient : public WebViewClient {
public:
    TestWebViewClient() : m_animationScheduled(false) { }
    virtual ~TestWebViewClient() { }
    void initializeLayerTreeView() override;
    WebLayerTreeView* layerTreeView() override { return m_layerTreeView.get(); }

    void scheduleAnimation() override { m_animationScheduled = true; }
    bool animationScheduled() { return m_animationScheduled; }
    void clearAnimationScheduled() { m_animationScheduled = false; }

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
    bool m_animationScheduled;
};

// Convenience class for handling the lifetime of a WebView and its associated mainframe in tests.
class WebViewHelper {
    WTF_MAKE_NONCOPYABLE(WebViewHelper);
public:
    WebViewHelper(SettingOverrider* = 0);
    ~WebViewHelper();

    // Creates and initializes the WebView. Implicitly calls reset() first. IF a
    // WebFrameClient or a WebViewClient are passed in, they must outlive the
    // WebViewHelper.
    WebViewImpl* initialize(bool enableJavascript = false, TestWebFrameClient* = 0, TestWebViewClient* = 0, void (*updateSettingsFunc)(WebSettings*) = 0);

    // Same as initialize() but also performs the initial load of the url. Only
    // returns once the load is complete.
    WebViewImpl* initializeAndLoad(const std::string& url, bool enableJavascript = false, TestWebFrameClient* = 0, TestWebViewClient* = 0, void (*updateSettingsFunc)(WebSettings*) = 0);

    void resize(WebSize);

    void reset();

    WebView* webView() const { return m_webView; }
    WebViewImpl* webViewImpl() const { return m_webView; }

private:
    WebViewImpl* m_webView;
    WebFrameWidget* m_webViewWidget;
    SettingOverrider* m_settingOverrider;
    UseMockScrollbarSettings m_mockScrollbarSettings;
    TestWebViewClient* m_testWebViewClient;
};

// Minimal implementation of WebFrameClient needed for unit tests that load frames. Tests that load
// frames and need further specialization of WebFrameClient behavior should subclass this.
class TestWebFrameClient : public WebFrameClient {
public:
    TestWebFrameClient();

    WebFrame* createChildFrame(WebLocalFrame* parent, WebTreeScopeType, const WebString& name, const WebString& uniqueName, WebSandboxFlags, const WebFrameOwnerProperties&) override;
    void frameDetached(WebFrame*, DetachType) override;
    void didStartLoading(bool) override;
    void didStopLoading() override;

    bool isLoading() { return m_loadsInProgress > 0; }
    void waitForLoadToComplete();

private:
    int m_loadsInProgress;
};

// Minimal implementation of WebRemoteFrameClient needed for unit tests that load remote frames. Tests that load
// frames and need further specialization of WebFrameClient behavior should subclass this.
class TestWebRemoteFrameClient : public WebRemoteFrameClient {
public:
    TestWebRemoteFrameClient();

    WebRemoteFrameImpl* frame() const { return m_frame; }

    // WebRemoteFrameClient overrides:
    void frameDetached(DetachType) override;
    void postMessageEvent(
        WebLocalFrame* sourceFrame,
        WebRemoteFrame* targetFrame,
        WebSecurityOrigin targetOrigin,
        WebDOMMessageEvent) override { }

private:
    RawPtrWillBePersistent<WebRemoteFrameImpl> const m_frame;
};

} // namespace FrameTestHelpers
} // namespace blink

#endif // FrameTestHelpers_h
