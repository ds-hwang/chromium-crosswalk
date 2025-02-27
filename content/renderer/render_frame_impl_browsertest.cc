// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/document_state.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/fake_compositor_dependencies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace {
const int32_t kSubframeRouteId = 20;
const int32_t kSubframeWidgetRouteId = 21;
const int32_t kFrameProxyRouteId = 22;
}  // namespace

namespace content {

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  ~RenderFrameImplTest() override {}

  void SetUp() override {
    RenderViewTest::SetUp();
    EXPECT_TRUE(GetMainRenderFrame()->is_main_frame_);

    FrameMsg_NewFrame_WidgetParams widget_params;
    widget_params.routing_id = kSubframeWidgetRouteId;
    widget_params.hidden = false;

    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

    LoadHTML("Parent frame <iframe name='frame'></iframe>");

    FrameReplicationState frame_replication_state;
    frame_replication_state.name = "frame";
    frame_replication_state.unique_name = "frame-uniqueName";

    RenderFrameImpl::FromWebFrame(
        view_->GetMainRenderFrame()->GetWebFrame()->firstChild())
        ->OnSwapOut(kFrameProxyRouteId, false, frame_replication_state);

    RenderFrameImpl::CreateFrame(
        kSubframeRouteId, MSG_ROUTING_NONE, MSG_ROUTING_NONE,
        kFrameProxyRouteId, MSG_ROUTING_NONE, frame_replication_state,
        &compositor_deps_, widget_params, blink::WebFrameOwnerProperties());

    frame_ = RenderFrameImpl::FromRoutingID(kSubframeRouteId);
    EXPECT_FALSE(frame_->is_main_frame_);
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  void SetIsUsingLoFi(RenderFrameImpl* frame, bool is_using_lofi) {
    frame->is_using_lofi_ = is_using_lofi;
  }

  RenderFrameImpl* GetMainRenderFrame() {
    return static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());
  }

  RenderFrameImpl* frame() { return frame_; }

  content::RenderWidget* frame_widget() const {
    return frame_->render_widget_.get();
  }

 private:
  RenderFrameImpl* frame_;
  FakeCompositorDependencies compositor_deps_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame), visible_(false) {}

  ~RenderFrameTestObserver() override {}

  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }

  bool visible() { return visible_; }

 private:
  bool visible_;
};

#if defined(OS_ANDROID)
// See https://crbug.com/472717
#define MAYBE_SubframeWidget DISABLED_SubframeWidget
#define MAYBE_FrameResize DISABLED_FrameResize
#define MAYBE_FrameWasShown DISABLED_FrameWasShown
#define MAYBE_FrameWasShownAfterWidgetClose DISABLED_FrameWasShownAfterWidgetClose
#else
#define MAYBE_SubframeWidget SubframeWidget
#define MAYBE_FrameResize FrameResize
#define MAYBE_FrameWasShown FrameWasShown
#define MAYBE_FrameWasShownAfterWidgetClose FrameWasShownAfterWidgetClose
#endif

// Verify that a frame with a RenderFrameProxy as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, MAYBE_SubframeWidget) {
  EXPECT_TRUE(frame_widget());
  // We can't convert to RenderWidget* directly, because
  // it and RenderView are two unrelated base classes
  // of RenderViewImpl. If a class has multiple base classes,
  // each base class pointer will be distinct, and direct casts
  // between unrelated base classes are undefined, even if they share
  // a common derived class. The compiler has no way in general of
  // determining the displacement between the two classes, so these
  // types of casts cannot be implemented in a type safe way.
  // To overcome this, we make two legal static casts:
  // first, downcast from RenderView* to RenderViewImpl*,
  // then upcast from RenderViewImpl* to RenderWidget*.
  EXPECT_NE(frame_widget(),
            static_cast<content::RenderWidget*>(
                static_cast<content::RenderViewImpl*>((view_))));
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, MAYBE_FrameResize) {
  ResizeParams resize_params;
  gfx::Size size(200, 200);
  resize_params.screen_info = blink::WebScreenInfo();
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  resize_params.top_controls_height = 0.f;
  resize_params.top_controls_shrink_blink_size = false;
  resize_params.resizer_rect = gfx::Rect();
  resize_params.is_fullscreen_granted = false;

  ViewMsg_Resize resize_message(0, resize_params);
  frame_widget()->OnMessageReceived(resize_message);

  EXPECT_EQ(frame_widget()->webwidget()->size(), blink::WebSize(size));
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, MAYBE_FrameWasShown) {
  RenderFrameTestObserver observer(frame());

  ViewMsg_WasShown was_shown_message(0, true, ui::LatencyInfo());
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

// Ensure that a RenderFrameImpl does not crash if the RenderView receives
// a WasShown message after the frame's widget has been closed.
TEST_F(RenderFrameImplTest, MAYBE_FrameWasShownAfterWidgetClose) {
  RenderFrameTestObserver observer(frame());

  ViewMsg_Close close_message(0);
  frame_widget()->OnMessageReceived(close_message);

  ViewMsg_WasShown was_shown_message(0, true, ui::LatencyInfo());
  static_cast<RenderViewImpl*>(view_)->OnMessageReceived(was_shown_message);

  // This test is primarily checking that this case does not crash, but
  // observers should still be notified.
  EXPECT_TRUE(observer.visible());
}

// Test that LoFi state only updates for new main frame documents. Subframes
// inherit from the main frame and should not change at commit time.
TEST_F(RenderFrameImplTest, LoFiNotUpdatedOnSubframeCommits) {
  SetIsUsingLoFi(GetMainRenderFrame(), true);
  SetIsUsingLoFi(frame(), true);
  EXPECT_TRUE(GetMainRenderFrame()->IsUsingLoFi());
  EXPECT_TRUE(frame()->IsUsingLoFi());

  blink::WebHistoryItem item;
  item.initialize();

  // The main frame's and subframe's LoFi states should stay the same on
  // navigations within the page.
  frame()->didNavigateWithinPage(frame()->GetWebFrame(), item,
                                 blink::WebStandardCommit);
  EXPECT_TRUE(frame()->IsUsingLoFi());
  GetMainRenderFrame()->didNavigateWithinPage(
      GetMainRenderFrame()->GetWebFrame(), item, blink::WebStandardCommit);
  EXPECT_TRUE(GetMainRenderFrame()->IsUsingLoFi());

  // The subframe's LoFi state should not be reset on commit.
  DocumentState* document_state =
      DocumentState::FromDataSource(frame()->GetWebFrame()->dataSource());
  static_cast<NavigationStateImpl*>(document_state->navigation_state())
      ->set_was_within_same_page(false);

  frame()->didCommitProvisionalLoad(frame()->GetWebFrame(), item,
                                    blink::WebStandardCommit);
  EXPECT_TRUE(frame()->IsUsingLoFi());

  // The main frame's LoFi state should be reset to off on commit.
  document_state = DocumentState::FromDataSource(
      GetMainRenderFrame()->GetWebFrame()->dataSource());
  static_cast<NavigationStateImpl*>(document_state->navigation_state())
      ->set_was_within_same_page(false);

  // Calling didCommitProvisionalLoad is not representative of a full navigation
  // but serves the purpose of testing the LoFi state logic.
  GetMainRenderFrame()->didCommitProvisionalLoad(
      GetMainRenderFrame()->GetWebFrame(), item, blink::WebStandardCommit);
  EXPECT_FALSE(GetMainRenderFrame()->IsUsingLoFi());
  // The subframe would be deleted here after a cross-document navigation. It
  // happens to be left around in this test because this does not simulate the
  // frame detach.
}

}  // namespace
