// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

using aura::test::WindowIsAbove;

class PanelLayoutManagerTest : public test::AshTestBase {
 public:
  PanelLayoutManagerTest() {}
  ~PanelLayoutManagerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    ASSERT_TRUE(test::TestShelfDelegate::instance());

    shelf_view_test_.reset(new test::ShelfViewTestAPI(
        GetShelfView(Shelf::ForPrimaryDisplay())));
    shelf_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreateNormalWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithBounds(bounds);
  }

  aura::Window* CreatePanelWindowWithDelegate(aura::WindowDelegate* delegate,
                                              const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        delegate, ui::wm::WINDOW_TYPE_PANEL, 0, bounds);
    test::TestShelfDelegate* shelf_delegate =
        test::TestShelfDelegate::instance();
    shelf_delegate->AddShelfItem(window);
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    return CreatePanelWindowWithDelegate(NULL, bounds);
  }

  aura::Window* GetPanelContainer(aura::Window* panel) {
    return Shell::GetContainer(panel->GetRootWindow(),
                               kShellWindowId_PanelContainer);
  }

  views::Widget* GetCalloutWidgetForPanel(aura::Window* panel) {
    PanelLayoutManager* manager =
        static_cast<PanelLayoutManager*>(GetPanelContainer(panel)->
                                         layout_manager());
    DCHECK(manager);
    PanelLayoutManager::PanelList::iterator found = std::find(
        manager->panel_windows_.begin(), manager->panel_windows_.end(),
        panel);
    DCHECK(found != manager->panel_windows_.end());
    DCHECK(found->callout_widget);
    return reinterpret_cast<views::Widget*>(found->callout_widget);
  }

  void PanelInScreen(aura::Window* panel) {
    gfx::Rect panel_bounds = panel->GetBoundsInRootWindow();
    gfx::Point root_point = gfx::Point(panel_bounds.x(), panel_bounds.y());
    gfx::Display display = ScreenUtil::FindDisplayContainingPoint(root_point);

    gfx::Rect panel_bounds_in_screen = panel->GetBoundsInScreen();
    gfx::Point screen_bottom_right = gfx::Point(
        panel_bounds_in_screen.right(),
        panel_bounds_in_screen.bottom());
    gfx::Rect display_bounds = display.bounds();
    EXPECT_TRUE(screen_bottom_right.x() < display_bounds.width() &&
                screen_bottom_right.y() < display_bounds.height());
  }

  void PanelsNotOverlapping(aura::Window* panel1, aura::Window* panel2) {
    // Waits until all shelf view animations are done.
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();
    gfx::Rect window1_bounds = panel1->GetBoundsInRootWindow();
    gfx::Rect window2_bounds = panel2->GetBoundsInRootWindow();

    EXPECT_FALSE(window1_bounds.Intersects(window2_bounds));
  }

  void IsPanelAboveLauncherIcon(const aura::Window* panel) {
    // Waits until all shelf view animations are done.
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();

    Shelf* shelf = RootWindowController::ForShelf(panel)->shelf()->shelf();
    gfx::Rect icon_bounds = shelf->GetScreenBoundsOfItemIconForWindow(panel);
    ASSERT_FALSE(icon_bounds.width() == 0 && icon_bounds.height() == 0);

    gfx::Rect window_bounds = panel->GetBoundsInScreen();
    ASSERT_LT(icon_bounds.width(), window_bounds.width());
    ASSERT_LT(icon_bounds.height(), window_bounds.height());
    gfx::Rect shelf_bounds = shelf->shelf_widget()->GetWindowBoundsInScreen();
    ShelfAlignment alignment = GetAlignment(panel->GetRootWindow());

    if (IsHorizontal(alignment)) {
      // The horizontal bounds of the panel window should contain the bounds of
      // the shelf icon.
      EXPECT_LE(window_bounds.x(), icon_bounds.x());
      EXPECT_GE(window_bounds.right(), icon_bounds.right());
    } else {
      // The vertical bounds of the panel window should contain the bounds of
      // the shelf icon.
      EXPECT_LE(window_bounds.y(), icon_bounds.y());
      EXPECT_GE(window_bounds.bottom(), icon_bounds.bottom());
    }

    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        EXPECT_EQ(shelf_bounds.y(), window_bounds.bottom());
        break;
      case SHELF_ALIGNMENT_LEFT:
        EXPECT_EQ(shelf_bounds.right(), window_bounds.x());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        EXPECT_EQ(shelf_bounds.x(), window_bounds.right());
        break;
      case SHELF_ALIGNMENT_TOP:
        EXPECT_EQ(shelf_bounds.bottom(), window_bounds.y());
        break;
    }
  }

  void IsCalloutAboveLauncherIcon(aura::Window* panel) {
    // Flush the message loop, since callout updates use a delayed task.
    base::RunLoop().RunUntilIdle();
    views::Widget* widget = GetCalloutWidgetForPanel(panel);

    Shelf* shelf = RootWindowController::ForShelf(panel)->shelf()->shelf();
    gfx::Rect icon_bounds = shelf->GetScreenBoundsOfItemIconForWindow(panel);
    ASSERT_FALSE(icon_bounds.IsEmpty());

    gfx::Rect panel_bounds = panel->GetBoundsInScreen();
    gfx::Rect callout_bounds = widget->GetWindowBoundsInScreen();
    ASSERT_FALSE(icon_bounds.IsEmpty());

    EXPECT_TRUE(widget->IsVisible());

    ShelfAlignment alignment = GetAlignment(panel->GetRootWindow());
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        EXPECT_EQ(panel_bounds.bottom(), callout_bounds.y());
        break;
      case SHELF_ALIGNMENT_LEFT:
        EXPECT_EQ(panel_bounds.x(), callout_bounds.right());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        EXPECT_EQ(panel_bounds.right(), callout_bounds.x());
        break;
      case SHELF_ALIGNMENT_TOP:
        EXPECT_EQ(panel_bounds.y(), callout_bounds.bottom());
        break;
    }

    if (IsHorizontal(alignment)) {
      EXPECT_NEAR(icon_bounds.CenterPoint().x(),
                  widget->GetWindowBoundsInScreen().CenterPoint().x(),
                  1);
    } else {
      EXPECT_NEAR(icon_bounds.CenterPoint().y(),
                  widget->GetWindowBoundsInScreen().CenterPoint().y(),
                  1);
    }
  }

  bool IsPanelCalloutVisible(aura::Window* panel) {
    views::Widget* widget = GetCalloutWidgetForPanel(panel);
    return widget->IsVisible();
  }

  test::ShelfViewTestAPI* shelf_view_test() {
    return shelf_view_test_.get();
  }

  // Clicks the shelf items on |shelf_view| that is associated with given
  // |window|.
  void ClickShelfItemForWindow(ShelfView* shelf_view, aura::Window* window) {
    test::ShelfViewTestAPI test_api(shelf_view);
    test_api.SetAnimationDuration(1);
    test_api.RunMessageLoopUntilAnimationsDone();
    ShelfModel* model = test::ShellTestApi(Shell::GetInstance()).shelf_model();
    int index = model->ItemIndexByID(GetShelfIDForWindow(window));
    gfx::Rect bounds = test_api.GetButton(index)->GetBoundsInScreen();

    ui::test::EventGenerator& event_generator = GetEventGenerator();
    event_generator.MoveMouseTo(bounds.CenterPoint());
    event_generator.ClickLeftButton();

    test_api.RunMessageLoopUntilAnimationsDone();
  }

  void SetAlignment(aura::Window* root_window, ShelfAlignment alignment) {
    ash::Shell* shell = ash::Shell::GetInstance();
    shell->SetShelfAlignment(alignment, root_window);
  }

  ShelfAlignment GetAlignment(const aura::Window* root_window) {
    ash::Shell* shell = ash::Shell::GetInstance();
    return shell->GetShelfAlignment(root_window);
  }

  void SetShelfAutoHideBehavior(aura::Window* window,
                                ShelfAutoHideBehavior behavior) {
    ShelfLayoutManager* shelf = RootWindowController::ForWindow(window)
                                    ->shelf()
                                    ->shelf_layout_manager();
    shelf->SetAutoHideBehavior(behavior);
    ShelfView* shelf_view = GetShelfView(Shelf::ForWindow(window));
    test::ShelfViewTestAPI test_api(shelf_view);
    test_api.RunMessageLoopUntilAnimationsDone();
  }

  void SetShelfVisibilityState(aura::Window* window,
                               ShelfVisibilityState visibility_state) {
    ShelfLayoutManager* shelf = RootWindowController::ForWindow(window)
                                    ->shelf()
                                    ->shelf_layout_manager();
    shelf->SetState(visibility_state);
  }

  ShelfView* GetShelfView(Shelf* shelf) {
    return test::ShelfTestAPI(shelf).shelf_view();
  }

 private:
  scoped_ptr<test::ShelfViewTestAPI> shelf_view_test_;

  bool IsHorizontal(ShelfAlignment alignment) {
    return alignment == SHELF_ALIGNMENT_BOTTOM ||
           alignment == SHELF_ALIGNMENT_TOP;
  }

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManagerTest);
};

class PanelLayoutManagerTextDirectionTest
    : public PanelLayoutManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  PanelLayoutManagerTextDirectionTest() : is_rtl_(GetParam()) {}
  virtual ~PanelLayoutManagerTextDirectionTest() {}

  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");
    PanelLayoutManagerTest::SetUp();
    ASSERT_EQ(is_rtl_, base::i18n::IsRTL());
  }

  void TearDown() override {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale(original_locale_);
    PanelLayoutManagerTest::TearDown();
  }

 private:
  bool is_rtl_;
  std::string original_locale_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManagerTextDirectionTest);
};

// Tests that a created panel window is above the shelf icon in LTR and RTL.
TEST_P(PanelLayoutManagerTextDirectionTest, AddOnePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  EXPECT_EQ(GetPanelContainer(window.get()), window->parent());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(window.get()));
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(window.get()));
}

// Tests that a created panel window is successfully aligned over a hidden
// shelf icon.
TEST_F(PanelLayoutManagerTest, PanelAlignsToHiddenLauncherIcon) {
  gfx::Rect bounds(0, 0, 201, 201);
  SetShelfAutoHideBehavior(Shell::GetPrimaryRootWindow(),
                           SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  scoped_ptr<aura::Window> normal_window(CreateNormalWindow(bounds));
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  EXPECT_EQ(GetPanelContainer(window.get()), window->parent());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(window.get()));
}

TEST_F(PanelLayoutManagerTest, PanelAlignsToHiddenLauncherIconSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Keep the displays wide so that shelves have enough space for shelves
  // buttons.
  UpdateDisplay("400x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> normal_window(
      CreateNormalWindow(gfx::Rect(450, 0, 100, 100)));
  scoped_ptr<aura::Window> panel(CreatePanelWindow(gfx::Rect(400, 0, 50, 50)));
  EXPECT_EQ(root_windows[1], panel->GetRootWindow());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(panel.get()));
  gfx::Rect shelf_visible_position = panel->GetBoundsInScreen();

  SetShelfAutoHideBehavior(root_windows[1],
                           SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  // Expect the panel X position to remain the same after the shelf is hidden
  // but the Y to move down.
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(panel.get()));
  EXPECT_EQ(shelf_visible_position.x(), panel->GetBoundsInScreen().x());
  EXPECT_GT(panel->GetBoundsInScreen().y(), shelf_visible_position.y());
}

// Tests interactions between multiple panels
TEST_F(PanelLayoutManagerTest, MultiplePanelsAreAboveIcons) {
  gfx::Rect odd_bounds(0, 0, 201, 201);
  gfx::Rect even_bounds(0, 0, 200, 200);

  scoped_ptr<aura::Window> w1(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));

  scoped_ptr<aura::Window> w2(CreatePanelWindow(even_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));

  scoped_ptr<aura::Window> w3(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelStacking) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // Default stacking order.
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  // Changing the active window should update the stacking order.
  wm::ActivateWindow(w1.get());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));

  wm::ActivateWindow(w2.get());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  wm::ActivateWindow(w3.get());
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelStackingVertical) {
  // Set shelf to be aligned on the right.
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_RIGHT);

  // Size panels in such a way that ordering them by X coordinate would cause
  // stacking order to be incorrect. Test that stacking order is based on Y.
  scoped_ptr<aura::Window> w1(CreatePanelWindow(gfx::Rect(0, 0, 210, 201)));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(gfx::Rect(0, 0, 220, 201)));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(gfx::Rect(0, 0, 200, 201)));

  // Default stacking order.
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  // Changing the active window should update the stacking order.
  wm::ActivateWindow(w1.get());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));

  wm::ActivateWindow(w2.get());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  wm::ActivateWindow(w3.get());
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelCallout) {
  gfx::Rect bounds(0, 0, 200, 200);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w4(CreateNormalWindow(gfx::Rect()));
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(IsPanelCalloutVisible(w1.get()));
  EXPECT_TRUE(IsPanelCalloutVisible(w2.get()));
  EXPECT_TRUE(IsPanelCalloutVisible(w3.get()));
  wm::ActivateWindow(w1.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w1.get()));
  wm::ActivateWindow(w2.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w2.get()));
  wm::ActivateWindow(w3.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w3.get()));
  wm::ActivateWindow(w4.get());
  wm::ActivateWindow(w3.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w3.get()));
  w3.reset();
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w2.get()));
}

// Tests removing panels.
TEST_F(PanelLayoutManagerTest, RemoveLeftPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w1.get());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  // Now, windows should be stacked 1 > 2 > 3
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveMiddlePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w2.get());
  // Windows should be stacked 1 < 2 > 3
  w2.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w3.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveRightPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w3.get());
  // Order shouldn't change.
  w3.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveNonActivePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w2.get());
  // Windows should be stacked 1 < 2 > 3
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
}

TEST_F(PanelLayoutManagerTest, SplitView) {
  gfx::Rect bounds(0, 0, 90, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));

  EXPECT_NO_FATAL_FAILURE(PanelsNotOverlapping(w1.get(), w2.get()));
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_SplitViewOverlapWhenLarge DISABLED_SplitViewOverlapWhenLarge
#else
#define MAYBE_SplitViewOverlapWhenLarge SplitViewOverlapWhenLarge
#endif

TEST_F(PanelLayoutManagerTest, MAYBE_SplitViewOverlapWhenLarge) {
  gfx::Rect bounds(0, 0, 600, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));

  EXPECT_NO_FATAL_FAILURE(PanelInScreen(w1.get()));
  EXPECT_NO_FATAL_FAILURE(PanelInScreen(w2.get()));
}

TEST_F(PanelLayoutManagerTest, FanWindows) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  int window_x1 = w1->GetBoundsInRootWindow().CenterPoint().x();
  int window_x2 = w2->GetBoundsInRootWindow().CenterPoint().x();
  int window_x3 = w3->GetBoundsInRootWindow().CenterPoint().x();
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  int icon_x1 = shelf->GetScreenBoundsOfItemIconForWindow(w1.get()).x();
  int icon_x2 = shelf->GetScreenBoundsOfItemIconForWindow(w2.get()).x();
  EXPECT_EQ(window_x2 - window_x1, window_x3 - window_x2);
  int spacing = window_x2 - window_x1;
  EXPECT_GT(spacing, icon_x2 - icon_x1);
}

TEST_F(PanelLayoutManagerTest, FanLargeWindow) {
  gfx::Rect small_bounds(0, 0, 201, 201);
  gfx::Rect large_bounds(0, 0, 501, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(small_bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(large_bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(small_bounds));

  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  int window_x1 = w1->GetBoundsInRootWindow().CenterPoint().x();
  int window_x2 = w2->GetBoundsInRootWindow().CenterPoint().x();
  int window_x3 = w3->GetBoundsInRootWindow().CenterPoint().x();
  // The distances may not be equidistant with a large panel but the panels
  // should be in the correct order with respect to their midpoints.
  EXPECT_GT(window_x2, window_x1);
  EXPECT_GT(window_x3, window_x2);
}

TEST_F(PanelLayoutManagerTest, MinimizeRestorePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  // Activate the window, ensure callout is visible.
  wm::ActivateWindow(window.get());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
  // Minimize the panel, callout should be hidden.
  wm::GetWindowState(window.get())->Minimize();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsPanelCalloutVisible(window.get()));
  // Restore the panel; panel should not be activated by default but callout
  // should be visible.
  wm::GetWindowState(window.get())->Unminimize();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
  // Activate the window, ensure callout is visible.
  wm::ActivateWindow(window.get());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
}

TEST_F(PanelLayoutManagerTest, PanelMoveBetweenMultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Keep the displays wide so that shelves have enough space for launcher
  // buttons.
  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> p1_d1(CreatePanelWindow(gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<aura::Window> p2_d1(CreatePanelWindow(gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<aura::Window> p1_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));
  scoped_ptr<aura::Window> p2_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));

  ShelfView* shelf_view_1st = GetShelfView(Shelf::ForPrimaryDisplay());
  ShelfView* shelf_view_2nd =
      GetShelfView(Shelf::ForWindow(root_windows[1]));

  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());

  EXPECT_EQ(kShellWindowId_PanelContainer, p1_d1->parent()->id());
  EXPECT_EQ(kShellWindowId_PanelContainer, p2_d1->parent()->id());
  EXPECT_EQ(kShellWindowId_PanelContainer, p1_d2->parent()->id());
  EXPECT_EQ(kShellWindowId_PanelContainer, p2_d2->parent()->id());

  // Test a panel on 1st display.
  // Clicking on the same display has no effect.
  ClickShelfItemForWindow(shelf_view_1st, p1_d1.get());
  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_FALSE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d1->GetBoundsInScreen()));

  // Test if clicking on another display moves the panel to
  // that display.
  ClickShelfItemForWindow(shelf_view_2nd, p1_d1.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d1->GetBoundsInScreen()));

  // Test a panel on 2nd display.
  // Clicking on the same display has no effect.
  ClickShelfItemForWindow(shelf_view_2nd, p1_d2.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d2->GetBoundsInScreen()));

  // Test if clicking on another display moves the panel to
  // that display.
  ClickShelfItemForWindow(shelf_view_1st, p1_d2.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      p1_d2->GetBoundsInScreen()));

  // Test if clicking on a previously moved window moves the
  // panel back to the original display.
  ClickShelfItemForWindow(shelf_view_1st, p1_d1.get());
  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      p1_d1->GetBoundsInScreen()));
}

TEST_F(PanelLayoutManagerTest, PanelAttachPositionMultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // Keep the displays wide so that shelves have enough space for shelf buttons.
  // Use differently sized displays so the shelf is in a different
  // position on second display.
  UpdateDisplay("600x400,600x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> p1_d1(CreatePanelWindow(gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<aura::Window> p1_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));

  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());

  IsPanelAboveLauncherIcon(p1_d1.get());
  IsCalloutAboveLauncherIcon(p1_d1.get());
  IsPanelAboveLauncherIcon(p1_d2.get());
  IsCalloutAboveLauncherIcon(p1_d2.get());
}

TEST_F(PanelLayoutManagerTest, PanelAlignmentSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> p1_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());

  IsPanelAboveLauncherIcon(p1_d2.get());
  IsCalloutAboveLauncherIcon(p1_d2.get());

  SetAlignment(root_windows[1], SHELF_ALIGNMENT_RIGHT);
  IsPanelAboveLauncherIcon(p1_d2.get());
  IsCalloutAboveLauncherIcon(p1_d2.get());
  SetAlignment(root_windows[1], SHELF_ALIGNMENT_LEFT);
  IsPanelAboveLauncherIcon(p1_d2.get());
  IsCalloutAboveLauncherIcon(p1_d2.get());
  SetAlignment(root_windows[1], SHELF_ALIGNMENT_TOP);
  IsPanelAboveLauncherIcon(p1_d2.get());
  IsCalloutAboveLauncherIcon(p1_d2.get());
}

TEST_F(PanelLayoutManagerTest, AlignmentLeft) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_LEFT);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

TEST_F(PanelLayoutManagerTest, AlignmentRight) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_RIGHT);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

TEST_F(PanelLayoutManagerTest, AlignmentTop) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_TOP);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

// Tests that panels will hide and restore their state with the shelf visibility
// state. This ensures that entering full-screen mode will hide your panels
// until you leave it.
TEST_F(PanelLayoutManagerTest, PanelsHideAndRestoreWithShelf) {
  gfx::Rect bounds(0, 0, 201, 201);

  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3;
  // Minimize w2.
  wm::GetWindowState(w2.get())->Minimize();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());

  SetShelfVisibilityState(Shell::GetPrimaryRootWindow(), SHELF_HIDDEN);
  RunAllPendingInMessageLoop();

  // w3 is created while in full-screen mode, should only become visible when
  // we exit fullscreen mode.
  w3.reset(CreatePanelWindow(bounds));

  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  // While in full-screen mode, the panel windows should still be in the
  // switchable window list - http://crbug.com/313919.
  MruWindowTracker::WindowList switchable_window_list =
      Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList();
  EXPECT_EQ(3u, switchable_window_list.size());
  EXPECT_NE(switchable_window_list.end(),
      std::find(switchable_window_list.begin(), switchable_window_list.end(),
          w1.get()));
  EXPECT_NE(switchable_window_list.end(),
      std::find(switchable_window_list.begin(), switchable_window_list.end(),
          w2.get()));
  EXPECT_NE(switchable_window_list.end(),
      std::find(switchable_window_list.begin(), switchable_window_list.end(),
          w3.get()));

  SetShelfVisibilityState(Shell::GetPrimaryRootWindow(), SHELF_VISIBLE);
  RunAllPendingInMessageLoop();

  // Windows should be restored to their prior state.
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
}

// Verifies that touches along the attached edge of a panel do not
// target the panel itself.
TEST_F(PanelLayoutManagerTest, TouchHitTestPanel) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w(
      CreatePanelWindowWithDelegate(&delegate, gfx::Rect(0, 0, 200, 200)));
  ui::EventTarget* root = w->GetRootWindow();
  ui::EventTargeter* targeter = root->GetEventTargeter();

  // Note that the constants used in the touch locations below are
  // arbitrarily-selected small numbers which will ensure the point is
  // within the default extended region surrounding the panel. This value
  // is calculated as
  // kResizeOutsideBoundsSize * kResizeOutsideBoundsScaleForTouch
  // in src/ash/root_window_controller.cc.

  // Hit test outside the right edge with a bottom-aligned shelf.
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_BOTTOM);
  gfx::Rect bounds(w->bounds());
  ui::TouchEvent touch(ui::ET_TOUCH_PRESSED,
                       gfx::Point(bounds.right() + 3, bounds.y() + 2),
                       0, ui::EventTimeForNow());
  ui::EventTarget* target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_EQ(w.get(), target);

  // Hit test outside the bottom edge with a bottom-aligned shelf.
  touch.set_location(gfx::Point(bounds.x() + 6, bounds.bottom() + 5));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_NE(w.get(), target);

  // Hit test outside the bottom edge with a right-aligned shelf.
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_RIGHT);
  bounds = w->bounds();
  touch.set_location(gfx::Point(bounds.x() + 6, bounds.bottom() + 5));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_EQ(w.get(), target);

  // Hit test outside the right edge with a right-aligned shelf.
  touch.set_location(gfx::Point(bounds.right() + 3, bounds.y() + 2));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_NE(w.get(), target);

  // Hit test outside the top edge with a left-aligned shelf.
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_LEFT);
  bounds = w->bounds();
  touch.set_location(gfx::Point(bounds.x() + 4, bounds.y() - 6));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_EQ(w.get(), target);

  // Hit test outside the left edge with a left-aligned shelf.
  touch.set_location(gfx::Point(bounds.x() - 1, bounds.y() + 5));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_NE(w.get(), target);

  // Hit test outside the left edge with a top-aligned shelf.
  SetAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_TOP);
  bounds = w->bounds();
  touch.set_location(gfx::Point(bounds.x() - 1, bounds.y() + 5));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_EQ(w.get(), target);

  // Hit test outside the top edge with a top-aligned shelf.
  touch.set_location(gfx::Point(bounds.x() + 4, bounds.y() - 6));
  target = targeter->FindTargetForEvent(root, &touch);
  EXPECT_NE(w.get(), target);
}

INSTANTIATE_TEST_CASE_P(LtrRtl, PanelLayoutManagerTextDirectionTest,
                        testing::Bool());

}  // namespace ash
