// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/test/test_ink_drop_delegate.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/drag_controller.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/drag_drop_client.h"
#endif

using base::ASCIIToUTF16;

namespace views {
class InkDropDelegate;

namespace test {

// A MenuButton subclass that provides access to some MenuButton internals.
class TestMenuButton : public MenuButton {
 public:
  explicit TestMenuButton(MenuButtonListener* menu_button_listener)
      : MenuButton(base::string16(ASCIIToUTF16("button")),
                   menu_button_listener,
                   false) {}

  ~TestMenuButton() override {}

  // Accessors to protected MenuButton methods.
  void set_ink_drop_delegate(InkDropDelegate* ink_drop_delegate) {
    MenuButton::set_ink_drop_delegate(ink_drop_delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuButton);
};

class MenuButtonTest : public ViewsTestBase {
 public:
  MenuButtonTest() : widget_(nullptr), button_(nullptr) {}
  ~MenuButtonTest() override {}

  void TearDown() override {
    generator_.reset();
    if (widget_ && !widget_->IsClosed())
      widget_->Close();

    ViewsTestBase::TearDown();
  }

  Widget* widget() { return widget_; }
  TestMenuButton* button() { return button_; }
  ui::test::EventGenerator* generator() { return generator_.get(); }

 protected:
  // Creates a MenuButton with no button listener.
  void CreateMenuButtonWithNoListener() { CreateMenuButton(nullptr); }

  // Creates a MenuButton with a MenuButtonListener. In this case, when the
  // MenuButton is pushed, it notifies the MenuButtonListener to open a
  // drop-down menu.
  void CreateMenuButtonWithMenuButtonListener(
      MenuButtonListener* menu_button_listener) {
    CreateMenuButton(menu_button_listener);
  }

 private:
  void CreateMenuButton(MenuButtonListener* menu_button_listener) {
    CreateWidget();
    generator_.reset(new ui::test::EventGenerator(GetContext(),
        widget_->GetNativeWindow()));
    // Set initial mouse location in a consistent way so that the menu button we
    // are about to create initializes its hover state in a consistent manner.
    generator_->set_current_location(gfx::Point(10, 10));

    button_ = new TestMenuButton(menu_button_listener);
    button_->SetBoundsRect(gfx::Rect(0, 0, 200, 20));
    widget_->SetContentsView(button_);

    widget_->Show();
  }

  void CreateWidget() {
    DCHECK(!widget_);

    widget_ = new Widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
  }

  Widget* widget_;
  TestMenuButton* button_;
  scoped_ptr<ui::test::EventGenerator> generator_;

  DISALLOW_COPY_AND_ASSIGN(MenuButtonTest);
};

class TestButtonListener : public ButtonListener {
 public:
  TestButtonListener()
      : last_sender_(nullptr),
        last_sender_state_(Button::STATE_NORMAL),
        last_event_type_(ui::ET_UNKNOWN) {}
  ~TestButtonListener() override {}

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    last_sender_ = sender;
    CustomButton* custom_button = CustomButton::AsCustomButton(sender);
    DCHECK(custom_button);
    last_sender_state_ = custom_button->state();
    last_event_type_ = event.type();
  }

  Button* last_sender() { return last_sender_; }
  Button::ButtonState last_sender_state() { return last_sender_state_; }
  ui::EventType last_event_type() { return last_event_type_; }

 private:
  Button* last_sender_;
  Button::ButtonState last_sender_state_;
  ui::EventType last_event_type_;

  DISALLOW_COPY_AND_ASSIGN(TestButtonListener);
};

class TestMenuButtonListener : public MenuButtonListener {
 public:
  TestMenuButtonListener()
      : last_source_(nullptr), last_source_state_(Button::STATE_NORMAL) {}
  ~TestMenuButtonListener() override {}

  void OnMenuButtonClicked(MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override {
    last_source_ = source;
    CustomButton* custom_button = CustomButton::AsCustomButton(source);
    DCHECK(custom_button);
    last_source_state_ = custom_button->state();
  }

  View* last_source() { return last_source_; }
  Button::ButtonState last_source_state() { return last_source_state_; }

 private:
  View* last_source_;
  Button::ButtonState last_source_state_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuButtonListener);
};

// A MenuButtonListener that will acquire a PressedLock in the
// OnMenuButtonClicked() method and optionally release it as well.
class PressStateMenuButtonListener : public MenuButtonListener {
 public:
  explicit PressStateMenuButtonListener(bool release_lock)
      : menu_button_(nullptr), release_lock_(release_lock) {}

  ~PressStateMenuButtonListener() override {}

  void set_menu_button(MenuButton* menu_button) { menu_button_ = menu_button; }

  void OnMenuButtonClicked(MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override {
    pressed_lock_.reset(new MenuButton::PressedLock(menu_button_));
    if (release_lock_)
      pressed_lock_.reset();
  }

 private:
  MenuButton* menu_button_;

  scoped_ptr<MenuButton::PressedLock> pressed_lock_;

  // The |pressed_lock_| will be released when true.
  bool release_lock_;

  DISALLOW_COPY_AND_ASSIGN(PressStateMenuButtonListener);
};

// Basic implementation of a DragController, to test input behaviour for
// MenuButtons that can be dragged.
class TestDragController : public DragController {
 public:
  TestDragController() {}
  ~TestDragController() override {}

  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {}

  int GetDragOperationsForView(View* sender, const gfx::Point& p) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }

  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragController);
};

#if defined(USE_AURA)
// Basic implementation of a DragDropClient, tracking the state of the drag
// operation. While dragging addition mouse events are consumed, preventing the
// target view from receiving them.
class TestDragDropClient : public aura::client::DragDropClient,
                           public ui::EventHandler {
 public:
  TestDragDropClient();
  ~TestDragDropClient() override;

  // aura::client::DragDropClient:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragUpdate(aura::Window* target, const ui::LocatedEvent& event) override;
  void Drop(aura::Window* target, const ui::LocatedEvent& event) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  // True while receiving ui::LocatedEvents for drag operations.
  bool drag_in_progress_;

  // Target window where drag operations are occuring.
  aura::Window* target_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

TestDragDropClient::TestDragDropClient()
    : drag_in_progress_(false), target_(nullptr) {
}

TestDragDropClient::~TestDragDropClient() {
}

int TestDragDropClient::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  if (IsDragDropInProgress())
    return ui::DragDropTypes::DRAG_NONE;
  drag_in_progress_ = true;
  target_ = root_window;
  return operation;
}

void TestDragDropClient::DragUpdate(aura::Window* target,
                                    const ui::LocatedEvent& event) {
}

void TestDragDropClient::Drop(aura::Window* target,
                              const ui::LocatedEvent& event) {
  drag_in_progress_ = false;
}

void TestDragDropClient::DragCancel() {
  drag_in_progress_ = false;
}

bool TestDragDropClient::IsDragDropInProgress() {
  return drag_in_progress_;
}

void TestDragDropClient::OnMouseEvent(ui::MouseEvent* event) {
  if (!IsDragDropInProgress())
    return;
  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      DragUpdate(target_, *event);
      event->StopPropagation();
      break;
    case ui::ET_MOUSE_RELEASED:
      Drop(target_, *event);
      event->StopPropagation();
      break;
    default:
      break;
  }
}
#endif  // defined(USE_AURA)

class TestShowSiblingButtonListener : public MenuButtonListener {
 public:
  TestShowSiblingButtonListener() {}
  ~TestShowSiblingButtonListener() override {}

  void OnMenuButtonClicked(MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override {
    // The MenuButton itself doesn't set the PRESSED state during Activate() or
    // OnMenuButtonClicked(). That should be handled by the MenuController or,
    // if no menu is shown, the listener.
    EXPECT_EQ(Button::STATE_HOVERED, static_cast<MenuButton*>(source)->state());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestShowSiblingButtonListener);
};

// Tests if the listener is notified correctly when a mouse click happens on a
// MenuButton that has a MenuButtonListener.
TEST_F(MenuButtonTest, ActivateDropDownOnMouseClick) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);

  generator()->ClickLeftButton();

  // Check that MenuButton has notified the listener, while it was in pressed
  // state.
  EXPECT_EQ(button(), menu_button_listener.last_source());
  EXPECT_EQ(Button::STATE_HOVERED, menu_button_listener.last_source_state());
}

// Test that the MenuButton stays pressed while there are any PressedLocks.
TEST_F(MenuButtonTest, ButtonStateForMenuButtonsWithPressedLocks) {
  CreateMenuButtonWithNoListener();

  // Move the mouse over the button; the button should be in a hovered state.
  generator()->MoveMouseTo(gfx::Point(10, 10));
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  // Introduce a PressedLock, which should make the button pressed.
  scoped_ptr<MenuButton::PressedLock> pressed_lock1(
      new MenuButton::PressedLock(button()));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  // Even if we move the mouse outside of the button, it should remain pressed.
  generator()->MoveMouseTo(gfx::Point(300, 10));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  // Creating a new lock should obviously keep the button pressed.
  scoped_ptr<MenuButton::PressedLock> pressed_lock2(
      new MenuButton::PressedLock(button()));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  // The button should remain pressed while any locks are active.
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());

  // Reseting the final lock should return the button's state to normal...
  pressed_lock2.reset();
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());

  // ...And it should respond to mouse movement again.
  generator()->MoveMouseTo(gfx::Point(10, 10));
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  // Test that the button returns to the appropriate state after the press; if
  // the mouse ends over the button, the button should be hovered.
  pressed_lock1.reset(new MenuButton::PressedLock(button()));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  // If the button is disabled before the pressed lock, it should be disabled
  // after the pressed lock.
  button()->SetState(Button::STATE_DISABLED);
  pressed_lock1.reset(new MenuButton::PressedLock(button()));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_DISABLED, button()->state());

  generator()->MoveMouseTo(gfx::Point(300, 10));

  // Edge case: the button is disabled, a pressed lock is added, and then the
  // button is re-enabled. It should be enabled after the lock is removed.
  pressed_lock1.reset(new MenuButton::PressedLock(button()));
  EXPECT_EQ(Button::STATE_PRESSED, button()->state());
  button()->SetState(Button::STATE_NORMAL);
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
}

// Test that if a sibling menu is shown, the original menu button releases its
// PressedLock.
TEST_F(MenuButtonTest, PressedStateWithSiblingMenu) {
  TestShowSiblingButtonListener listener;
  CreateMenuButtonWithMenuButtonListener(&listener);

  // Move the mouse over the button; the button should be in a hovered state.
  generator()->MoveMouseTo(gfx::Point(10, 10));
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());
  generator()->ClickLeftButton();
  // Test is continued in TestShowSiblingButtonListener::OnMenuButtonClicked().
}

// Test that the MenuButton does not become pressed if it can be dragged, until
// a release occurs.
TEST_F(MenuButtonTest, DraggableMenuButtonActivatesOnRelease) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  TestDragController drag_controller;
  button()->set_drag_controller(&drag_controller);

  generator()->PressLeftButton();
  EXPECT_EQ(nullptr, menu_button_listener.last_source());

  generator()->ReleaseLeftButton();
  EXPECT_EQ(button(), menu_button_listener.last_source());
  EXPECT_EQ(Button::STATE_HOVERED, menu_button_listener.last_source_state());
}

TEST_F(MenuButtonTest, InkDropStateForMenuButtonActivationsWithoutListener) {
  CreateMenuButtonWithNoListener();
  TestInkDropDelegate ink_drop_delegate;
  ink_drop_delegate.OnAction(InkDropState::ACTION_PENDING);
  button()->set_ink_drop_delegate(&ink_drop_delegate);
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_delegate.state());
}

TEST_F(MenuButtonTest,
       InkDropStateForMenuButtonActivationsWithListenerThatDoesntAcquireALock) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::QUICK_ACTION, ink_drop_delegate.state());
}

TEST_F(
    MenuButtonTest,
    InkDropStateForMenuButtonActivationsWithListenerThatDontReleaseAllLocks) {
  PressStateMenuButtonListener menu_button_listener(false);
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  menu_button_listener.set_menu_button(button());
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());

  // Prevent the button from accessing invalid memory during clean up.
  button()->set_ink_drop_delegate(nullptr);
}

TEST_F(MenuButtonTest,
       InkDropStateForMenuButtonActivationsWithListenerThatReleaseAllLocks) {
  PressStateMenuButtonListener menu_button_listener(true);
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  menu_button_listener.set_menu_button(button());
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop_delegate.state());
}

TEST_F(MenuButtonTest, InkDropStateForMenuButtonsWithPressedLocks) {
  CreateMenuButtonWithNoListener();
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);

  scoped_ptr<MenuButton::PressedLock> pressed_lock1(
      new MenuButton::PressedLock(button()));

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());

  scoped_ptr<MenuButton::PressedLock> pressed_lock2(
      new MenuButton::PressedLock(button()));

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());

  pressed_lock1.reset();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());

  pressed_lock2.reset();
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop_delegate.state());
}

// Verifies only one ink drop animation is triggered when multiple PressedLocks
// are attached to a MenuButton.
TEST_F(MenuButtonTest, OneInkDropAnimationForReentrantPressedLocks) {
  CreateMenuButtonWithNoListener();
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);

  scoped_ptr<MenuButton::PressedLock> pressed_lock1(
      new MenuButton::PressedLock(button()));

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());
  ink_drop_delegate.OnAction(InkDropState::ACTION_PENDING);

  scoped_ptr<MenuButton::PressedLock> pressed_lock2(
      new MenuButton::PressedLock(button()));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop_delegate.state());
}

// Verifies the InkDropState is left as ACTIVATED if a PressedLock is active
// before another Activation occurs.
TEST_F(MenuButtonTest,
       InkDropStateForMenuButtonWithPressedLockBeforeActivation) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  TestInkDropDelegate ink_drop_delegate;
  button()->set_ink_drop_delegate(&ink_drop_delegate);
  MenuButton::PressedLock lock(button());

  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop_delegate.state());
}

#if defined(USE_AURA)

// Tests that the MenuButton does not become pressed if it can be dragged, and a
// DragDropClient is processing the events.
TEST_F(MenuButtonTest, DraggableMenuButtonDoesNotActivateOnDrag) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  TestDragController drag_controller;
  button()->set_drag_controller(&drag_controller);

  TestDragDropClient drag_client;
  SetDragDropClient(GetContext(), &drag_client);
  button()->PrependPreTargetHandler(&drag_client);

  generator()->DragMouseBy(10, 0);
  EXPECT_EQ(nullptr, menu_button_listener.last_source());
  EXPECT_EQ(Button::STATE_NORMAL, menu_button_listener.last_source_state());
}

#endif  // USE_AURA

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

// Tests if the listener is notified correctly when a gesture tap happens on a
// MenuButton that has a MenuButtonListener.
TEST_F(MenuButtonTest, ActivateDropDownOnGestureTap) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);

  // Move the mouse outside the menu button so that it doesn't impact the
  // button state.
  generator()->MoveMouseTo(400, 400);
  EXPECT_FALSE(button()->IsMouseHovered());

  generator()->GestureTapAt(gfx::Point(10, 10));

  // Check that MenuButton has notified the listener, while it was in pressed
  // state.
  EXPECT_EQ(button(), menu_button_listener.last_source());
  EXPECT_EQ(Button::STATE_HOVERED, menu_button_listener.last_source_state());

  // The button should go back to it's normal state since the gesture ended.
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
}

// Tests that the button enters a hovered state upon a tap down, before becoming
// pressed at activation.
TEST_F(MenuButtonTest, TouchFeedbackDuringTap) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  generator()->PressTouch();
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  generator()->ReleaseTouch();
  EXPECT_EQ(Button::STATE_HOVERED, menu_button_listener.last_source_state());
}

// Tests that a move event that exits the button returns it to the normal state,
// and that the button did not activate the listener.
TEST_F(MenuButtonTest, TouchFeedbackDuringTapCancel) {
  TestMenuButtonListener menu_button_listener;
  CreateMenuButtonWithMenuButtonListener(&menu_button_listener);
  generator()->PressTouch();
  EXPECT_EQ(Button::STATE_HOVERED, button()->state());

  generator()->MoveTouch(gfx::Point(10, 30));
  generator()->ReleaseTouch();
  EXPECT_EQ(Button::STATE_NORMAL, button()->state());
  EXPECT_EQ(nullptr, menu_button_listener.last_source());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

}  // namespace views
}  // namespace test
