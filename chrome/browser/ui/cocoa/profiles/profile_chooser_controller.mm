// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#import <Carbon/Carbon.h>  // kVK_Return.
#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/profiles/signin_view_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_mac.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kLargeImageSide = 88;
const int kSmallImageSide = 32;
const CGFloat kFixedMenuWidth = 250;

const CGFloat kVerticalSpacing = 16.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kHorizontalSpacing = 16.0;
const CGFloat kTitleFontSize = 15.0;
const CGFloat kTextFontSize = 12.0;
const CGFloat kProfileButtonHeight = 30;
const int kBezelThickness = 3;  // Width of the bezel on an NSButton.
const int kImageTitleSpacing = 10;
const int kBlueButtonHeight = 30;
const CGFloat kFocusRingLineWidth = 2;

// Fixed size for embedded sign in pages as defined in Gaia.
const CGFloat kFixedGaiaViewWidth = 360;

// Fixed size for the account removal view.
const CGFloat kFixedAccountRemovalViewWidth = 280;

// Fixed size for the switch user view.
const int kFixedSwitchUserViewWidth = 320;

// The tag number for the primary account.
const int kPrimaryProfileTag = -1;

gfx::Image CreateProfileImage(const gfx::Image& icon, int imageSize) {
  return profiles::GetSizedAvatarIcon(
      icon, true /* image is a square */, imageSize, imageSize);
}

// Updates the window size and position.
void SetWindowSize(NSWindow* window, NSSize size) {
  NSRect frame = [window frame];
  frame.origin.x += frame.size.width - size.width;
  frame.origin.y += frame.size.height - size.height;
  frame.size = size;
  [window setFrame:frame display:YES];
}

NSString* ElideEmail(const std::string& email, CGFloat width) {
  const base::string16 elidedEmail = gfx::ElideText(
      base::UTF8ToUTF16(email), gfx::FontList(), width, gfx::ELIDE_EMAIL);
  return base::SysUTF16ToNSString(elidedEmail);
}

NSString* ElideMessage(const base::string16& message, CGFloat width) {
  return base::SysUTF16ToNSString(
      gfx::ElideText(message, gfx::FontList(), width, gfx::ELIDE_TAIL));
}

// Builds a label with the given |title| anchored at |frame_origin|. Sets the
// text color to |text_color| if not null.
NSTextField* BuildLabel(NSString* title,
                        NSPoint frame_origin,
                        NSColor* text_color) {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setStringValue:title];
  [label setEditable:NO];
  [label setAlignment:NSLeftTextAlignment];
  [label setBezeled:NO];
  [label setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [label setDrawsBackground:NO];
  [label setFrameOrigin:frame_origin];
  [label sizeToFit];

  if (text_color)
    [[label cell] setTextColor:text_color];

  return label.autorelease();
}

// Builds an NSTextView that has the contents set to the specified |message|,
// with a non-underlined |link| inserted at |link_offset|. The view is anchored
// at the specified |frame_origin| and has a fixed |frame_width|.
NSTextView* BuildFixedWidthTextViewWithLink(
    id<NSTextViewDelegate> delegate,
    NSString* message,
    NSString* link,
    int link_offset,
    NSPoint frame_origin,
    CGFloat frame_width) {
  base::scoped_nsobject<HyperlinkTextView> text_view(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* link_color = skia::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());
  NSMutableString* finalMessage =
      [NSMutableString stringWithFormat:@"%@\n", message];
  [finalMessage insertString:link atIndex:link_offset];
  // Adds a padding row at the bottom, because |boundingRectWithSize| below cuts
  // off the last row sometimes.
  [text_view setMessage:finalMessage
               withFont:[NSFont labelFontOfSize:kTextFontSize]
           messageColor:[NSColor blackColor]];
  [text_view addLinkRange:NSMakeRange(link_offset, [link length])
                  withURL:nil
                linkColor:link_color];

  // Removes the underlining from the link.
  [text_view setLinkTextAttributes:nil];
  NSTextStorage* text_storage = [text_view textStorage];
  NSRange link_range = NSMakeRange(link_offset, [link length]);
  [text_storage addAttribute:NSUnderlineStyleAttributeName
                       value:[NSNumber numberWithInt:NSUnderlineStyleNone]
                       range:link_range];

  NSRect frame = [[text_view attributedString]
      boundingRectWithSize:NSMakeSize(frame_width, 0)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  frame.origin = frame_origin;
  [text_view setFrame:frame];
  [text_view setDelegate:delegate];
  return text_view.autorelease();
}

// Returns the native dialog background color.
NSColor* GetDialogBackgroundColor() {
  return skia::SkColorToCalibratedNSColor(
      ui::NativeThemeMac::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground));
}

// Builds a title card with one back button right aligned and one label center
// aligned.
NSView* BuildTitleCard(NSRect frame_rect,
                       const base::string16& message,
                       id back_button_target,
                       SEL back_button_action) {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:frame_rect]);

  base::scoped_nsobject<HoverImageButton> button(
      [[HoverImageButton alloc] initWithFrame:frame_rect]);
  [button setBordered:NO];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  [button setDefaultImage:rb->GetNativeImageNamed(IDR_BACK).ToNSImage()];
  [button setHoverImage:rb->GetNativeImageNamed(IDR_BACK_H).ToNSImage()];
  [button setPressedImage:rb->GetNativeImageNamed(IDR_BACK_P).ToNSImage()];
  [button setTarget:back_button_target];
  [button setAction:back_button_action];
  [button setFrameSize:NSMakeSize(kProfileButtonHeight, kProfileButtonHeight)];
  [button setFrameOrigin:NSMakePoint(kHorizontalSpacing, 0)];

  CGFloat max_label_width = frame_rect.size.width -
      (kHorizontalSpacing * 2 + kProfileButtonHeight) * 2;
  NSTextField* title_label = BuildLabel(
      ElideMessage(message, max_label_width),
      NSZeroPoint, nil);
  [title_label setAlignment:NSCenterTextAlignment];
  [title_label setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [title_label sizeToFit];
  CGFloat x_offset = (frame_rect.size.width - NSWidth([title_label frame])) / 2;
  CGFloat y_offset =
      (NSHeight([button frame]) - NSHeight([title_label frame])) / 2;
  [title_label setFrameOrigin:NSMakePoint(x_offset, y_offset)];

  [container addSubview:button];
  [container addSubview:title_label];
  CGFloat height = std::max(NSMaxY([title_label frame]),
                            NSMaxY([button frame])) + kVerticalSpacing;
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), height)];

  return container.autorelease();
}

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  return error_controller && error_controller->HasError();
}

std::string GetAuthErrorAccountId(Profile* profile) {
  const SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error_controller)
    return std::string();

  return error_controller->error_account_id();
}

}  // namespace

// Custom WebContentsDelegate that allows handling of hotkeys and suppresses
// the context menu.
class GaiaWebContentsDelegate : public content::WebContentsDelegate {
 public:
  GaiaWebContentsDelegate() {}
  ~GaiaWebContentsDelegate() override {}

 private:
  // content::WebContentsDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(GaiaWebContentsDelegate);
};

bool GaiaWebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void GaiaWebContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return;

  int chrome_command_id = [BrowserWindowUtils getCommandId:event];

  bool is_text_editing_command =
      (event.modifiers & blink::WebInputEvent::MetaKey) &&
      (event.windowsKeyCode == ui::VKEY_A ||
       event.windowsKeyCode == ui::VKEY_V);

  // TODO(guohui): maybe should add an accelerator for the back button.
  if (chrome_command_id == IDC_CLOSE_WINDOW || chrome_command_id == IDC_EXIT ||
      is_text_editing_command) {
    [[NSApp mainMenu] performKeyEquivalent:event.os_event];
  }
}

// Class that listens to changes to the OAuth2Tokens for the active profile,
// changes to the avatar menu model or browser close notifications.
class ActiveProfileObserverBridge : public AvatarMenuObserver,
                                    public content::NotificationObserver,
                                    public OAuth2TokenService::Observer {
 public:
  ActiveProfileObserverBridge(ProfileChooserController* controller,
                              Browser* browser)
      : controller_(controller),
        browser_(browser),
        token_observer_registered_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
                   content::NotificationService::AllSources());
    if (!browser_->profile()->IsGuestSession())
      AddTokenServiceObserver();
  }

  ~ActiveProfileObserverBridge() override { RemoveTokenServiceObserver(); }

 private:
  void AddTokenServiceObserver() {
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->AddObserver(this);
    token_observer_registered_ = true;
  }

  void RemoveTokenServiceObserver() {
    if (!token_observer_registered_)
      return;
    DCHECK(browser_->profile());
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
    DCHECK(oauth2_token_service);
    oauth2_token_service->RemoveObserver(this);
    token_observer_registered_ = false;
  }

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    // Tokens can only be added by adding an account through the inline flow,
    // which is started from the account management view. Refresh it to show the
    // update.
    profiles::BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
        viewMode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
      [controller_ initMenuContentsWithView:
          switches::IsEnableAccountConsistency() ?
              profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
              profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
    }
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    // Tokens can only be removed from the account management view. Refresh it
    // to show the update.
    if ([controller_ viewMode] == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
      [controller_ initMenuContentsWithView:
          profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
  }

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override {
    profiles::BubbleViewMode viewMode = [controller_ viewMode];
    if (viewMode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ||
        viewMode == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
      [controller_ initMenuContentsWithView:viewMode];
    }
  }

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_BROWSER_CLOSING, type);
    if (browser_ == content::Source<Browser>(source).ptr()) {
      RemoveTokenServiceObserver();
      // Clean up the bubble's WebContents (used by the Gaia embedded view), to
      // make sure the guest profile doesn't have any dangling host renderers.
      // This can happen if Chrome is quit using Command-Q while the bubble is
      // still open, which won't give the bubble a chance to be closed and
      // clean up the WebContents itself.
      [controller_ cleanUpEmbeddedViewContents];
    }
  }

  ProfileChooserController* controller_;  // Weak; owns this.
  Browser* browser_;  // Weak.
  content::NotificationRegistrar registrar_;

  // The observer can be removed both when closing the browser, and by just
  // closing the avatar bubble. However, in the case of closing the browser,
  // the avatar bubble will also be closed afterwards, resulting in a second
  // attempt to remove the observer. This ensures the observer is only
  // removed once.
  bool token_observer_registered_;

  DISALLOW_COPY_AND_ASSIGN(ActiveProfileObserverBridge);
};

// Custom button cell that adds a left padding before the button image, and
// a custom spacing between the button image and title.
@interface CustomPaddingImageButtonCell : NSButtonCell {
 @private
  // Padding added to the left margin of the button.
  int leftMarginSpacing_;
  // Spacing between the cell image and title.
  int imageTitleSpacing_;
}

- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing;
@end

@implementation CustomPaddingImageButtonCell
- (id)initWithLeftMarginSpacing:(int)leftMarginSpacing
              imageTitleSpacing:(int)imageTitleSpacing {
  if ((self = [super init])) {
    leftMarginSpacing_ = leftMarginSpacing;
    imageTitleSpacing_ = imageTitleSpacing;
  }
  return self;
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  NSRect marginRect;
  NSDivideRect(frame, &marginRect, &frame, leftMarginSpacing_, NSMinXEdge);

  // The title frame origin isn't aware of the left margin spacing added
  // in -drawImage, so it must be added when drawing the title as well.
  if ([self imagePosition] == NSImageLeft)
    NSDivideRect(frame, &marginRect, &frame, imageTitleSpacing_, NSMinXEdge);

  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if ([self imagePosition] == NSImageLeft)
    frame.origin.x = leftMarginSpacing_;
  [super drawImage:image withFrame:frame inView:controlView];
}

- (NSSize)cellSize {
  NSSize buttonSize = [super cellSize];
  buttonSize.width += leftMarginSpacing_;
  if ([self imagePosition] == NSImageLeft)
    buttonSize.width += imageTitleSpacing_;
  return buttonSize;
}

- (NSFocusRingType)focusRingType {
  // This is taken care of by the custom drawing code.
  return NSFocusRingTypeNone;
}

- (void)drawWithFrame:(NSRect)frame inView:(NSView *)controlView {
  [super drawInteriorWithFrame:frame inView:controlView];

  // Focus ring.
  if ([self showsFirstResponder]) {
    NSRect focusRingRect =
        NSInsetRect(frame, kFocusRingLineWidth, kFocusRingLineWidth);
    // TODO(noms): When we are targetting 10.7, we should change this to use
    // -drawFocusRingMaskWithFrame instead.
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:1] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRect:focusRingRect];
    [path setLineWidth:kFocusRingLineWidth];
    [path stroke];
  }
}

@end

// A custom image view that has a transparent backround.
@interface TransparentBackgroundImageView : NSImageView
@end

@implementation TransparentBackgroundImageView
- (void)drawRect:(NSRect)dirtyRect {
  NSColor* backgroundColor = [NSColor colorWithCalibratedWhite:1 alpha:0.6f];
  [backgroundColor setFill];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceAtop);
  [super drawRect:dirtyRect];
}
@end

@interface CustomCircleImageCell : NSButtonCell
@end

@implementation CustomCircleImageCell
- (void)drawWithFrame:(NSRect)frame inView:(NSView *)controlView {
  // Display everything as a circle that spans the entire control.
  NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect:frame];
  [path addClip];
  [super drawImage:[self image] withFrame:frame inView:controlView];
}
@end

// A custom image control that shows a "Change" button when moused over.
@interface EditableProfilePhoto : HoverImageButton {
 @private
  AvatarMenu* avatarMenu_;  // Weak; Owned by ProfileChooserController.
  base::scoped_nsobject<TransparentBackgroundImageView> changePhotoImage_;
  ProfileChooserController* controller_;
}

- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller;

// Called when the "Change" button is clicked.
- (void)editPhoto:(id)sender;

@end

@implementation EditableProfilePhoto
- (id)initWithFrame:(NSRect)frameRect
         avatarMenu:(AvatarMenu*)avatarMenu
        profileIcon:(const gfx::Image&)profileIcon
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller {
  if ((self = [super initWithFrame:frameRect])) {
    avatarMenu_ = avatarMenu;
    controller_ = controller;

    [self setBordered:NO];

    base::scoped_nsobject<CustomCircleImageCell> cell(
        [[CustomCircleImageCell alloc] init]);
    [self setCell:cell.get()];

    [self setDefaultImage:CreateProfileImage(
        profileIcon, kLargeImageSide).ToNSImage()];
    [self setImagePosition:NSImageOnly];

    NSRect bounds = NSMakeRect(0, 0, kLargeImageSide, kLargeImageSide);
    if (editingAllowed) {
      [self setTarget:self];
      [self setAction:@selector(editPhoto:)];
      changePhotoImage_.reset([[TransparentBackgroundImageView alloc]
          initWithFrame:bounds]);
      [changePhotoImage_ setImage:ui::ResourceBundle::GetSharedInstance().
          GetNativeImageNamed(IDR_ICON_PROFILES_EDIT_CAMERA).AsNSImage()];
      [self addSubview:changePhotoImage_];

      // Hide the image until the button is hovered over.
      [changePhotoImage_ setHidden:YES];
    }

    // Set the image cell's accessibility strings to be the same as the
    // button's strings.
    [[self cell] accessibilitySetOverrideValue:l10n_util::GetNSString(
        editingAllowed ?
        IDS_PROFILES_NEW_AVATAR_MENU_CHANGE_PHOTO_ACCESSIBLE_NAME :
        IDS_PROFILES_NEW_AVATAR_MENU_PHOTO_ACCESSIBLE_NAME)
                                  forAttribute:NSAccessibilityTitleAttribute];
    [[self cell] accessibilitySetOverrideValue:
        editingAllowed ? NSAccessibilityButtonRole : NSAccessibilityImageRole
                                  forAttribute:NSAccessibilityRoleAttribute];
    [[self cell] accessibilitySetOverrideValue:
        NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil)
            forAttribute:NSAccessibilityRoleDescriptionAttribute];

    // The button and the cell should read the same thing.
    [self accessibilitySetOverrideValue:l10n_util::GetNSString(
        editingAllowed ?
        IDS_PROFILES_NEW_AVATAR_MENU_CHANGE_PHOTO_ACCESSIBLE_NAME :
        IDS_PROFILES_NEW_AVATAR_MENU_PHOTO_ACCESSIBLE_NAME)
                                  forAttribute:NSAccessibilityTitleAttribute];
    [self accessibilitySetOverrideValue:NSAccessibilityButtonRole
                                  forAttribute:NSAccessibilityRoleAttribute];
    [self accessibilitySetOverrideValue:
        NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil)
            forAttribute:NSAccessibilityRoleDescriptionAttribute];
  }
  return self;
}

- (void)editPhoto:(id)sender {
  avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
  [controller_
      postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE];
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  [changePhotoImage_ setHidden:([self hoverState] == kHoverStateNone)];
}

- (BOOL)canBecomeKeyView {
  return false;
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityActionNames {
  NSArray* parentActions = [super accessibilityActionNames];
  return [parentActions arrayByAddingObject:NSAccessibilityPressAction];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
  }

  [super accessibilityPerformAction:action];
}

@end

// A custom text control that turns into a textfield for editing when clicked.
@interface EditableProfileNameButton : HoverImageButton<NSTextFieldDelegate> {
 @private
  base::scoped_nsobject<NSTextField> profileNameTextField_;
  Profile* profile_;  // Weak.
  ProfileChooserController* controller_;
}

- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller;

// Called when the button is clicked.
- (void)showEditableView:(id)sender;

// Called when enter is pressed in the text field.
- (void)saveProfileName;

@end

@implementation EditableProfileNameButton
- (id)initWithFrame:(NSRect)frameRect
            profile:(Profile*)profile
        profileName:(NSString*)profileName
     editingAllowed:(BOOL)editingAllowed
     withController:(ProfileChooserController*)controller {
  if ((self = [super initWithFrame:frameRect])) {
    profile_ = profile;
    controller_ = controller;

    CGFloat availableWidth = frameRect.size.width;
    NSSize textSize = [profileName sizeWithAttributes:@{
      NSFontAttributeName : [self font]
    }];

    if (editingAllowed) {
      // Show an "edit" pencil icon when hovering over. In the default state,
      // we need to create an empty placeholder of the correct size, so that
      // the text doesn't jump around when the hovered icon appears.
      ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
      NSImage* hoverImage = rb->GetNativeImageNamed(
          IDR_ICON_PROFILES_EDIT_HOVER).AsNSImage();

      // In order to center the button title, we need to add a left padding of
      // the same width as the pencil icon.
      base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
          [[CustomPaddingImageButtonCell alloc]
              initWithLeftMarginSpacing:[hoverImage size].width
                      imageTitleSpacing:0]);
      [self setCell:cell.get()];

      NSImage* placeholder = [[NSImage alloc] initWithSize:[hoverImage size]];
      [self setDefaultImage:placeholder];
      [self setHoverImage:hoverImage];
      [self setAlternateImage:
          rb->GetNativeImageNamed(IDR_ICON_PROFILES_EDIT_PRESSED).AsNSImage()];
      [self setImagePosition:NSImageRight];
      [self setTarget:self];
      [self setAction:@selector(showEditableView:)];

      // We need to subtract the width of the bezel from the frame rect, so that
      // the textfield can take the exact same space as the button.
      frameRect.size.height -= 2 * kBezelThickness;
      frameRect.origin = NSMakePoint(0, kBezelThickness);
      profileNameTextField_.reset(
          [[NSTextField alloc] initWithFrame:frameRect]);
      [profileNameTextField_ setStringValue:profileName];
      [profileNameTextField_ setFont:[NSFont labelFontOfSize:kTitleFontSize]];
      [profileNameTextField_ setEditable:YES];
      [profileNameTextField_ setDrawsBackground:YES];
      [profileNameTextField_ setBezeled:YES];
      [profileNameTextField_ setAlignment:NSCenterTextAlignment];
      [[profileNameTextField_ cell] setWraps:NO];
      [[profileNameTextField_ cell] setLineBreakMode:
          NSLineBreakByTruncatingTail];
      [[profileNameTextField_ cell] setUsesSingleLineMode:YES];
      [self addSubview:profileNameTextField_];
      [profileNameTextField_ setDelegate:self];

      // Hide the textfield until the user clicks on the button.
      [profileNameTextField_ setHidden:YES];

      [[self cell] accessibilitySetOverrideValue:l10n_util::GetNSStringF(
          IDS_PROFILES_NEW_AVATAR_MENU_EDIT_NAME_ACCESSIBLE_NAME,
          base::SysNSStringToUTF16(profileName))
                                    forAttribute:NSAccessibilityTitleAttribute];

      // Recompute the available width for the name since the icon takes space.
      availableWidth -= [hoverImage size].width * 2;
      // The profileNameTextField_ might size the text differently.
      textSize = [profileName sizeWithAttributes:@{
        NSFontAttributeName : [profileNameTextField_ font]
      }];
    }

    if (textSize.width > availableWidth)
      [self setToolTip:profileName];

    [[self cell] accessibilitySetOverrideValue:NSAccessibilityButtonRole
                                  forAttribute:NSAccessibilityRoleAttribute];
    [[self cell]
        accessibilitySetOverrideValue:NSAccessibilityRoleDescription(
                                          NSAccessibilityButtonRole, nil)
                         forAttribute:NSAccessibilityRoleDescriptionAttribute];

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTitleFontSize]];
    [self setAlignment:NSCenterTextAlignment];
    [[self cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setTitle:profileName];
  }
  return self;
}

- (void)saveProfileName {
  base::string16 newProfileName =
      base::SysNSStringToUTF16([profileNameTextField_ stringValue]);

  // Empty profile names are not allowed, and do nothing.
  base::TrimWhitespace(newProfileName, base::TRIM_ALL, &newProfileName);
  if (!newProfileName.empty()) {
    profiles::UpdateProfileName(profile_, newProfileName);
    [controller_
        postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME];
    [profileNameTextField_ setHidden:YES];
  }
}

- (void)showEditableView:(id)sender {
  [profileNameTextField_ setHidden:NO];
  [[self window] makeFirstResponder:profileNameTextField_];
}

- (BOOL)canBecomeKeyView {
  return false;
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector {
  if (commandSelector == @selector(insertTab:) ||
      commandSelector == @selector(insertNewline:)) {
    [self saveProfileName];
    return YES;
  }
  return NO;
}

@end

// A custom button that allows for setting a background color when hovered over.
@interface BackgroundColorHoverButton : HoverImageButton {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
  base::scoped_nsobject<NSColor> hoverColor_;
}
@end

@implementation BackgroundColorHoverButton

- (id)initWithFrame:(NSRect)frameRect
  imageTitleSpacing:(int)imageTitleSpacing
    backgroundColor:(NSColor*)backgroundColor {
  if ((self = [super initWithFrame:frameRect])) {
    backgroundColor_.reset([backgroundColor retain]);
    // Use a color from Aura, since this button is not trying to look like a
    // native control.
    SkColor hoverColor = ui::GetAuraColor(
        ui::NativeTheme::kColorId_ButtonHoverBackgroundColor, nullptr);
    hoverColor_.reset([skia::SkColorToSRGBNSColor(hoverColor) retain]);

    [self setBordered:NO];
    [self setFont:[NSFont labelFontOfSize:kTextFontSize]];
    [self setButtonType:NSMomentaryChangeButton];

    base::scoped_nsobject<CustomPaddingImageButtonCell> cell(
        [[CustomPaddingImageButtonCell alloc]
            initWithLeftMarginSpacing:kHorizontalSpacing
                    imageTitleSpacing:imageTitleSpacing]);
    [cell setLineBreakMode:NSLineBreakByTruncatingTail];
    [self setCell:cell.get()];
  }
  return self;
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];
  bool isHighlighted = ([self hoverState] != kHoverStateNone);

  NSColor* backgroundColor = isHighlighted ? hoverColor_ : backgroundColor_;
  [[self cell] setBackgroundColor:backgroundColor];
}

-(void)keyDown:(NSEvent*)event {
  // Since there is no default button in the bubble, it is safe to activate
  // all buttons on Enter as well, and be consistent with the Windows
  // implementation.
  if ([event keyCode] == kVK_Return)
    [self performClick:self];
  else
    [super keyDown:event];
}

- (BOOL)canBecomeKeyView {
  return YES;
}

@end

// A custom view with the given background color.
@interface BackgroundColorView : NSView {
 @private
  base::scoped_nsobject<NSColor> backgroundColor_;
}
@end

@implementation BackgroundColorView
- (id)initWithFrame:(NSRect)frameRect
          withColor:(NSColor*)color {
  if ((self = [super initWithFrame:frameRect]))
    backgroundColor_.reset([color retain]);
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  [backgroundColor_ setFill];
  NSRectFill(dirtyRect);
  [super drawRect:dirtyRect];
}
@end

// A custom dummy button that is used to clear focus from the bubble's controls.
@interface DummyWindowFocusButton : NSButton
@end

@implementation DummyWindowFocusButton
// Ignore accessibility, as this is a placeholder button.
- (BOOL)accessibilityIsIgnored {
  return YES;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  return nil;
}

- (BOOL)canBecomeKeyView {
  return NO;
}

@end

@interface ProfileChooserController ()
// Adds an horizontal separator to |container| at |yOffset| and returns the
// yOffset corresponding to after the separator.
- (CGFloat)addSeparatorToContainer:(NSView*)container
                         atYOffset:(CGFloat)yOffset;

// Builds the right-click profile switcher.
- (void)buildFastUserSwitcherViewWithProfiles:(NSMutableArray*)otherProfiles
                                    atYOffset:(CGFloat)yOffset
                                  inContainer:(NSView*)container;

// Builds the regular profile chooser view.
- (void)buildProfileChooserViewWithProfileView:(NSView*)currentProfileView
                                  tutorialView:(NSView*)tutorialView
                                     atYOffset:(CGFloat)yOffset
                                   inContainer:(NSView*)container
                                   displayLock:(bool)displayLock;

// Builds the profile chooser view.
- (NSView*)buildProfileChooserView;

- (NSView*)buildTutorialViewIfNeededForItem:(const AvatarMenu::Item&)item;

// Builds a tutorial card with a title label using |titleMessage|, a content
// label using |contentMessage|, a link using |linkMessage|, and a button using
// |buttonMessage|. If |stackButton| is YES, places the button above the link.
// Otherwise places both on the same row with the link left aligned and button
// right aligned. On click, the link would execute |linkAction|, and the button
// would execute |buttonAction|. It sets |tutorialMode_| to the given |mode|.
- (NSView*)tutorialViewWithMode:(profiles::TutorialMode)mode
                   titleMessage:(NSString*)titleMessage
                 contentMessage:(NSString*)contentMessage
                    linkMessage:(NSString*)linkMessage
                  buttonMessage:(NSString*)buttonMessage
                    stackButton:(BOOL)stackButton
                 hasCloseButton:(BOOL)hasCloseButton
                     linkAction:(SEL)linkAction
                   buttonAction:(SEL)buttonAction;

// Builds a tutorial card to introduce an upgrade user to the new avatar menu if
// needed. |tutorial_shown| indicates if the tutorial has already been shown in
// the previous active view. |avatar_item| refers to the current profile.
- (NSView*)buildWelcomeUpgradeTutorialView:(const AvatarMenu::Item&)item;

// Builds a tutorial card to inform the user about right-click user switching if
// needed.
- (NSView*)buildRightClickTutorialView;

// Builds a tutorial card to have the user confirm the last Chrome signin,
// Chrome sync will be delayed until the user either dismisses the tutorial, or
// configures sync through the "Settings" link.
- (NSView*)buildSigninConfirmationView;

// Builds a tutorial card to show the last signin error.
- (NSView*)buildSigninErrorView;

// Creates the main profile card for the profile |item| at the top of
// the bubble.
- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item;

// Creates the possible links for the main profile card with profile |item|.
- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect;

// Creates the disclaimer text for supervised users, telling them that the
// manager can view their history etc.
- (NSView*)createSupervisedUserDisclaimerView;

// Creates a main profile card for the guest user.
- (NSView*)createGuestProfileView;

// Creates an item for the profile |itemIndex| that is used in the fast profile
// switcher in the middle of the bubble.
- (NSButton*)createOtherProfileView:(int)itemIndex;

// Creates the "Not you" and Lock option buttons.
- (NSView*)createOptionsViewWithRect:(NSRect)rect
                         displayLock:(BOOL)displayLock;

// Creates the account management view for the active profile.
- (NSView*)createCurrentProfileAccountsView:(NSRect)rect;

// Creates the list of accounts for the active profile.
- (NSView*)createAccountsListWithRect:(NSRect)rect;

// Creates the Gaia sign-in/add account view.
- (NSView*)buildGaiaEmbeddedView;

// Creates the account removal view.
- (NSView*)buildAccountRemovalView;

// Create a view that shows various options for an upgrade user who is not
// the same person as the currently signed in user.
- (NSView*)buildSwitchUserView;

// Creates a button with |text|, an icon given by |imageResourceId| and with
// |action|.
- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
                          action:(SEL)action;

// Creates a generic link button with |title| and an |action| positioned at
// |frameOrigin|.
- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action;

// Creates an email account button with |title| and a remove icon. If
// |reauthRequired| is true, the button also displays a warning icon. |tag|
// indicates which account the button refers to.
- (NSButton*)accountButtonWithRect:(NSRect)rect
                         accountId:(const std::string&)accountId
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired;

- (bool)shouldShowGoIncognito;
@end

@implementation ProfileChooserController
- (profiles::BubbleViewMode) viewMode {
  return viewMode_;
}

- (void)setTutorialMode:(profiles::TutorialMode)tutorialMode {
  tutorialMode_ = tutorialMode;
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool alwaysCreate = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], alwaysCreate,
                               ProfileMetrics::SWITCH_PROFILE_ICON);
}

- (IBAction)showUserManager:(id)sender {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  [self postActionPerformed:
      ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER];
}

- (IBAction)exitGuest:(id)sender {
  DCHECK(browser_->profile()->IsGuestSession());
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  profiles::CloseGuestProfileWindows();
}

- (IBAction)goIncognito:(id)sender {
  DCHECK([self shouldShowGoIncognito]);
  chrome::NewIncognitoWindow(browser_);
  [self postActionPerformed:
      ProfileMetrics::PROFILE_DESKTOP_MENU_GO_INCOGNITO];
}

- (IBAction)showAccountManagement:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)hideAccountManagement:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)lockProfile:(id)sender {
  profiles::LockProfile(browser_->profile());
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK];
}

- (void)showSigninUIForMode:(profiles::BubbleViewMode)mode {
  if (SigninViewController::ShouldShowModalSigninForMode(mode)) {
    browser_->ShowModalSigninWindow(mode, accessPoint_);
  } else {
    [self initMenuContentsWithView:mode];
  }
}

- (IBAction)showInlineSigninPage:(id)sender {
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN];
}

- (IBAction)addAccount:(id)sender {
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT];
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT];
}

- (IBAction)navigateBackFromSigninPage:(id)sender {
  std::string primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool hasAccountManagement = !primaryAccount.empty() &&
      switches::IsEnableAccountConsistency();
  [self initMenuContentsWithView:hasAccountManagement ?
      profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
      profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)showAccountRemovalView:(id)sender {
  DCHECK(!isGuestSession_);

  // Tag is either |kPrimaryProfileTag| for the primary account, or equal to the
  // index in |currentProfileAccounts_| for a secondary account.
  int tag = [sender tag];
  if (tag == kPrimaryProfileTag) {
    accountIdToRemove_ = SigninManagerFactory::GetForProfile(
        browser_->profile())->GetAuthenticatedAccountId();
  } else {
    DCHECK(ContainsKey(currentProfileAccounts_, tag));
    accountIdToRemove_ = currentProfileAccounts_[tag];
  }

  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL];
}

- (IBAction)showAccountReauthenticationView:(id)sender {
  DCHECK(!isGuestSession_);
  [self showSigninUIForMode:profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH];
}

- (IBAction)removeAccount:(id)sender {
  DCHECK(!accountIdToRemove_.empty());
  ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile())
      ->RevokeCredentials(accountIdToRemove_);
  [self postActionPerformed:ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT];
  accountIdToRemove_.clear();

  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];
}

- (IBAction)seeWhatsNew:(id)sender {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_TUTORIAL_OVERVIEW,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
      ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_WHATS_NEW);
}

- (IBAction)showSwitchUserView:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_SWITCH_USER];
  ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
      ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_NOT_YOU);
}

- (IBAction)showLearnMorePage:(id)sender {
  signin_ui_util::ShowSigninErrorLearnMorePage(browser_->profile());
}

- (IBAction)configureSyncSettings:(id)sender {
  tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
  LoginUIServiceFactory::GetForProfile(browser_->profile())->
      SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_SETTINGS);
}

- (IBAction)syncSettingsConfirmed:(id)sender {
  tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
  LoginUIServiceFactory::GetForProfile(browser_->profile())->
      SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_OK);
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (IBAction)disconnectProfile:(id)sender {
  chrome::ShowSettings(browser_);
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_DISCONNECT);
}

- (IBAction)navigateBackFromSwitchUserView:(id)sender {
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_BACK);
}

- (IBAction)dismissTutorial:(id)sender {
  // Never shows the upgrade tutorial again if manually closed.
  if (tutorialMode_ == profiles::TUTORIAL_MODE_WELCOME_UPGRADE) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown,
        signin_ui_util::kUpgradeWelcomeTutorialShowMax + 1);
  }

  if(tutorialMode_ == profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING) {
    PrefService* localState = g_browser_process->local_state();
    localState->SetBoolean(
        prefs::kProfileAvatarRightClickTutorialDismissed, true);
  }

  tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
  [self initMenuContentsWithView:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER];
}

- (void)windowWillClose:(NSNotification*)notification {
  if (tutorialMode_ == profiles::TUTORIAL_MODE_CONFIRM_SIGNIN) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }

  [super windowWillClose:notification];
}

- (void)moveDown:(id)sender {
  [[self window] selectNextKeyView:self];
}

- (void)moveUp:(id)sender {
  [[self window] selectPreviousKeyView:self];
}

- (void)cleanUpEmbeddedViewContents {
  webContents_.reset();
  webContentsDelegate_.reset();
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             viewMode:(profiles::BubbleViewMode)viewMode
         tutorialMode:(profiles::TutorialMode)tutorialMode
          serviceType:(signin::GAIAServiceType)serviceType
          accessPoint:(signin_metrics::AccessPoint)accessPoint {
  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    browser_ = browser;
    viewMode_ = viewMode;
    tutorialMode_ = tutorialMode;
    observer_.reset(new ActiveProfileObserverBridge(self, browser_));
    serviceType_ = serviceType;
    accessPoint_ = accessPoint;

    avatarMenu_.reset(new AvatarMenu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(),
        observer_.get(),
        browser_));
    avatarMenu_->RebuildMenu();

    // Guest profiles do not have a token service.
    isGuestSession_ = browser_->profile()->IsGuestSession();

    // If view mode is PROFILE_CHOOSER but there is an auth error, force
    // ACCOUNT_MANAGEMENT mode.
    if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
        HasAuthError(browser_->profile()) &&
        switches::IsEnableAccountConsistency() &&
        avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex()).
            signed_in) {
      viewMode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
    }

    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_NEW_AVATAR_MENU_ACCESSIBLE_NAME)
                             forAttribute:NSAccessibilityTitleAttribute];
    [window accessibilitySetOverrideValue:
        l10n_util::GetNSString(IDS_PROFILES_NEW_AVATAR_MENU_ACCESSIBLE_NAME)
                             forAttribute:NSAccessibilityHelpAttribute];

    [[self bubble] setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [[self bubble] setArrowLocation:info_bubble::kNoArrow];
    [[self bubble] setBackgroundColor:GetDialogBackgroundColor()];
    [self initMenuContentsWithView:viewMode_];
  }

  return self;
}

- (void)initMenuContentsWithView:(profiles::BubbleViewMode)viewToDisplay {
  if (browser_->profile()->IsSupervised() &&
      (viewToDisplay == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       viewToDisplay == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }
  viewMode_ = viewToDisplay;
  NSView* contentView = [[self window] contentView];
  [contentView setSubviews:[NSArray array]];
  NSView* subView;

  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      subView = [self buildGaiaEmbeddedView];
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      subView = [self buildAccountRemovalView];
      break;
    case profiles::BUBBLE_VIEW_MODE_SWITCH_USER:
      subView = [self buildSwitchUserView];
      break;
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
    case profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER:
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
      subView = [self buildProfileChooserView];
      break;
  }

  // Clears tutorial mode for all non-profile-chooser views.
  if (viewMode_ != profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER)
    tutorialMode_ = profiles::TUTORIAL_MODE_NONE;

  // Add a dummy, empty element so that we don't initially display any
  // focus rings.
  NSButton* dummyFocusButton =
     [[[DummyWindowFocusButton alloc] initWithFrame:NSZeroRect] autorelease];
  [dummyFocusButton setNextKeyView:subView];
  [[self window] makeFirstResponder:dummyFocusButton];

  [contentView addSubview:subView];
  [contentView addSubview:dummyFocusButton];
  SetWindowSize([self window],
      NSMakeSize(NSWidth([subView frame]), NSHeight([subView frame])));
}

- (CGFloat)addSeparatorToContainer:(NSView*)container
                         atYOffset:(CGFloat)yOffset {
  NSBox* separator = [self horizontalSeparatorWithFrame:NSMakeRect(
      0, yOffset, kFixedMenuWidth, 0)];
  [container addSubview:separator];
  return NSMaxY([separator frame]);
}

// Builds the fast user switcher view in |container| at |yOffset| and populates
// it with the entries for every profile in |otherProfiles|. Returns the new
// yOffset after adding the elements.
- (void)buildFastUserSwitcherViewWithProfiles:(NSMutableArray*)otherProfiles
                                    atYOffset:(CGFloat)yOffset
                                  inContainer:(NSView*)container {
  // Other profiles switcher. The profiles have already been sorted
  // by their y-coordinate, so they can be added in the existing order.
  for (NSView* otherProfileView in otherProfiles) {
   [otherProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
   [container addSubview:otherProfileView];
   yOffset = NSMaxY([otherProfileView frame]);

   yOffset = [self addSeparatorToContainer:container atYOffset: yOffset];
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
}

- (void)buildProfileChooserViewWithProfileView:(NSView*)currentProfileView
                                  tutorialView:(NSView*)tutorialView
                                     atYOffset:(CGFloat)yOffset
                                   inContainer:(NSView*)container
                                   displayLock:(bool)displayLock {
  // Option buttons.
  NSRect rect = NSMakeRect(0, yOffset, kFixedMenuWidth, 0);
  NSView* optionsView = [self createOptionsViewWithRect:rect
                                            displayLock:displayLock];
  [container addSubview:optionsView];
  rect.origin.y = NSMaxY([optionsView frame]);

  NSBox* separator = [self horizontalSeparatorWithFrame:rect];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // For supervised users, add the disclaimer text.
  if (browser_->profile()->IsSupervised()) {
    yOffset += kSmallVerticalSpacing;
    NSView* disclaimerContainer = [self createSupervisedUserDisclaimerView];
    [disclaimerContainer setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:disclaimerContainer];
    yOffset = NSMaxY([disclaimerContainer frame]);
    yOffset += kSmallVerticalSpacing;

    yOffset = [self addSeparatorToContainer:container atYOffset: yOffset];
  }

  if (viewMode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    NSView* currentProfileAccountsView = [self createCurrentProfileAccountsView:
        NSMakeRect(0, yOffset, kFixedMenuWidth, 0)];
    [container addSubview:currentProfileAccountsView];
    yOffset = NSMaxY([currentProfileAccountsView frame]);

    yOffset = [self addSeparatorToContainer:container atYOffset: yOffset];
  }

  // Active profile card.
  if (currentProfileView) {
    yOffset += kVerticalSpacing;
    [currentProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:currentProfileView];
    yOffset = NSMaxY([currentProfileView frame]) + kVerticalSpacing;
  }

  if (tutorialView) {
    [tutorialView setFrameOrigin:NSMakePoint(0, yOffset)];
    [container addSubview:tutorialView];
    yOffset = NSMaxY([tutorialView frame]);
    //TODO(mlerman): update UMA stats for the new tutorials.
  } else {
    tutorialMode_ = profiles::TUTORIAL_MODE_NONE;
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
}

- (NSView*)buildProfileChooserView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  NSView* tutorialView = nil;
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);
  // Local and guest profiles cannot lock their profile.
  bool displayLock = false;
  bool isFastProfileChooser =
      viewMode_ == profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER;
  if (isFastProfileChooser) {
    // The user is using right-click switching, no need to tell them about it.
    PrefService* localState = g_browser_process->local_state();
    localState->SetBoolean(
        prefs::kProfileAvatarRightClickTutorialDismissed, true);
  }

  // Loop over the profiles in reverse, so that they are sorted by their
  // y-coordinate, and separate them into active and "other" profiles.
  for (int i = avatarMenu_->GetNumberOfItems() - 1; i >= 0; --i) {
    const AvatarMenu::Item& item = avatarMenu_->GetItemAt(i);
    if (item.active) {
      if (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) {
        tutorialView = [self buildTutorialViewIfNeededForItem:item];
      }
      currentProfileView = [self createCurrentProfileView:item];
      displayLock = item.signed_in &&
          profiles::IsLockAvailable(browser_->profile());
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }
  if (!currentProfileView)  // Guest windows don't have an active profile.
    currentProfileView = [self createGuestProfileView];

  // |yOffset| is the next position at which to draw in |container|
  // coordinates. Add a pixel offset so that the bottom option buttons don't
  // overlap the bubble's rounded corners.
  CGFloat yOffset = 1;

  if (isFastProfileChooser) {
    [self buildFastUserSwitcherViewWithProfiles:otherProfiles.get()
                                      atYOffset:yOffset
                                    inContainer:container.get()];
  } else {
    [self buildProfileChooserViewWithProfileView:currentProfileView
                                    tutorialView:tutorialView
                                       atYOffset:yOffset
                                     inContainer:container.get()
                                     displayLock:displayLock];
  }

  return container.autorelease();
}

- (NSView*)buildSigninConfirmationView {
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_VIEW);

  NSString* titleMessage = l10n_util::GetNSString(
      IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_TITLE);
  NSString* contentMessage = l10n_util::GetNSString(
      IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_CONTENT_TEXT);
  NSString* linkMessage = l10n_util::GetNSString(
      IDS_PROFILES_SYNC_SETTINGS_LINK);
  NSString* buttonMessage = l10n_util::GetNSString(
      IDS_PROFILES_TUTORIAL_OK_BUTTON);
  return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_CONFIRM_SIGNIN
                       titleMessage:titleMessage
                     contentMessage:contentMessage
                        linkMessage:linkMessage
                      buttonMessage:buttonMessage
                        stackButton:NO
                     hasCloseButton:NO
                         linkAction:@selector(configureSyncSettings:)
                       buttonAction:@selector(syncSettingsConfirmed:)];
}

- (NSView*)buildSigninErrorView {
  NSString* titleMessage = l10n_util::GetNSString(
      IDS_PROFILES_ERROR_TUTORIAL_TITLE);
  LoginUIService* loginUiService =
      LoginUIServiceFactory::GetForProfile(browser_->profile());
  NSString* contentMessage =
      base::SysUTF16ToNSString(loginUiService->GetLastLoginResult());
  NSString* linkMessage = l10n_util::GetNSString(
      IDS_PROFILES_PROFILE_TUTORIAL_LEARN_MORE);
  return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_CONFIRM_SIGNIN
                       titleMessage:titleMessage
                     contentMessage:contentMessage
                        linkMessage:linkMessage
                      buttonMessage:nil
                        stackButton:NO
                     hasCloseButton:YES
                         linkAction:@selector(showLearnMorePage:)
                       buttonAction:nil];
}

- (NSView*)buildWelcomeUpgradeTutorialView:(const AvatarMenu::Item&)item {
  ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
      ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_VIEW);

  NSString* titleMessage = l10n_util::GetNSString(
      IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_TITLE);
  NSString* contentMessage = l10n_util::GetNSString(
      IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_CONTENT_TEXT);
  // For local profiles, the "Not you" link doesn't make sense.
  NSString* linkMessage = item.signed_in ?
      ElideMessage(
          l10n_util::GetStringFUTF16(IDS_PROFILES_NOT_YOU, item.name),
          kFixedMenuWidth - 2 * kHorizontalSpacing) :
      nil;
  NSString* buttonMessage = l10n_util::GetNSString(
      IDS_PROFILES_TUTORIAL_WHATS_NEW_BUTTON);
  return [self tutorialViewWithMode:profiles::TUTORIAL_MODE_WELCOME_UPGRADE
                       titleMessage:titleMessage
                     contentMessage:contentMessage
                        linkMessage:linkMessage
                      buttonMessage:buttonMessage
                        stackButton:YES
                     hasCloseButton:YES
                         linkAction:@selector(showSwitchUserView:)
                       buttonAction:@selector(seeWhatsNew:)];
}

- (NSView*)buildRightClickTutorialView {
  NSString* titleMessage = l10n_util::GetNSString(
      IDS_PROFILES_RIGHT_CLICK_TUTORIAL_TITLE);
  NSString* contentMessage = l10n_util::GetNSString(
      IDS_PROFILES_RIGHT_CLICK_TUTORIAL_CONTENT_TEXT);
  NSString* buttonMessage = l10n_util::GetNSString(
      IDS_PROFILES_TUTORIAL_OK_BUTTON);

  return
      [self tutorialViewWithMode:profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING
                             titleMessage:titleMessage
                           contentMessage:contentMessage
                              linkMessage:nil
                            buttonMessage:buttonMessage
                              stackButton:NO
                           hasCloseButton:NO
                               linkAction:nil
                             buttonAction:@selector(dismissTutorial:)];
}

- (NSView*)buildTutorialViewIfNeededForItem:(const AvatarMenu::Item&)item {
  if (tutorialMode_ == profiles::TUTORIAL_MODE_CONFIRM_SIGNIN)
    return [self buildSigninConfirmationView];

  if (tutorialMode_ == profiles::TUTORIAL_MODE_SHOW_ERROR)
    return [self buildSigninErrorView];

  if (profiles::ShouldShowWelcomeUpgradeTutorial(
      browser_->profile(), tutorialMode_)) {
    if (tutorialMode_ != profiles::TUTORIAL_MODE_WELCOME_UPGRADE) {
      Profile* profile = browser_->profile();
      const int showCount = profile->GetPrefs()->GetInteger(
          prefs::kProfileAvatarTutorialShown);
      profile->GetPrefs()->SetInteger(
          prefs::kProfileAvatarTutorialShown, showCount + 1);
    }

    return [self buildWelcomeUpgradeTutorialView:item];
  }

  if (profiles::ShouldShowRightClickTutorial(browser_->profile()))
    return [self buildRightClickTutorialView];

  return nil;
}

- (NSView*)tutorialViewWithMode:(profiles::TutorialMode)mode
                   titleMessage:(NSString*)titleMessage
                 contentMessage:(NSString*)contentMessage
                    linkMessage:(NSString*)linkMessage
                  buttonMessage:(NSString*)buttonMessage
                    stackButton:(BOOL)stackButton
                 hasCloseButton:(BOOL)hasCloseButton
                     linkAction:(SEL)linkAction
                   buttonAction:(SEL)buttonAction {
  tutorialMode_ = mode;

  NSColor* tutorialBackgroundColor =
      skia::SkColorToSRGBNSColor(profiles::kAvatarTutorialBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:NSMakeRect(0, 0, kFixedMenuWidth, 0)
          withColor:tutorialBackgroundColor]);
  CGFloat availableWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  // Adds links and buttons at the bottom.
  base::scoped_nsobject<NSButton> tutorialOkButton;
  if (buttonMessage) {
    tutorialOkButton.reset([[HoverButton alloc] initWithFrame:NSZeroRect]);
    [tutorialOkButton setTitle:buttonMessage];
    [tutorialOkButton setBezelStyle:NSRoundedBezelStyle];
    [tutorialOkButton setTarget:self];
    [tutorialOkButton setAction:buttonAction];
    [tutorialOkButton setAlignment:NSCenterTextAlignment];
    [tutorialOkButton sizeToFit];
  }

  NSButton* learnMoreLink = nil;
  if (linkMessage) {
    learnMoreLink = [self linkButtonWithTitle:linkMessage
                                  frameOrigin:NSZeroPoint
                                       action:linkAction];
    [[learnMoreLink cell] setTextColor:[NSColor whiteColor]];
  }

  if (stackButton) {
    if (linkMessage) {
      [learnMoreLink setFrameOrigin:NSMakePoint(
          (kFixedMenuWidth - NSWidth([learnMoreLink frame])) / 2, yOffset)];
    }
    [tutorialOkButton setFrameSize:NSMakeSize(
        availableWidth, NSHeight([tutorialOkButton frame]))];
    [tutorialOkButton setFrameOrigin:NSMakePoint(
        kHorizontalSpacing,
        yOffset + (learnMoreLink ? NSHeight([learnMoreLink frame]) : 0))];
  } else {
    if (buttonMessage) {
      NSSize buttonSize = [tutorialOkButton frame].size;
      const CGFloat kTopBottomTextPadding = 6;
      const CGFloat kLeftRightTextPadding = 15;
      buttonSize.width += 2 * kLeftRightTextPadding;
      buttonSize.height += 2 * kTopBottomTextPadding;
      [tutorialOkButton setFrameSize:buttonSize];
      CGFloat buttonXOffset = kFixedMenuWidth -
          NSWidth([tutorialOkButton frame]) - kHorizontalSpacing;
      [tutorialOkButton setFrameOrigin:NSMakePoint(buttonXOffset, yOffset)];
    }

    if (linkMessage) {
      CGFloat linkYOffset = yOffset;
      if (buttonMessage) {
        linkYOffset += (NSHeight([tutorialOkButton frame]) -
                        NSHeight([learnMoreLink frame])) / 2;
      }
      [learnMoreLink setFrameOrigin:NSMakePoint(
          kHorizontalSpacing, linkYOffset)];
    }
  }

  if (buttonMessage) {
    [container addSubview:tutorialOkButton];
    yOffset = NSMaxY([tutorialOkButton frame]);
  }

  if (linkMessage) {
    [container addSubview:learnMoreLink];
    yOffset = std::max(NSMaxY([learnMoreLink frame]), yOffset);
  }

  yOffset += kVerticalSpacing;

  // Adds body content.
  NSTextField* contentLabel = BuildLabel(
      contentMessage,
      NSMakePoint(kHorizontalSpacing, yOffset),
      skia::SkColorToSRGBNSColor(profiles::kAvatarTutorialContentTextColor));
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kSmallVerticalSpacing;

  // Adds title.
  NSTextField* titleLabel =
      BuildLabel(titleMessage,
                 NSMakePoint(kHorizontalSpacing, yOffset),
                 [NSColor whiteColor] /* text_color */);
  [titleLabel setFont:[NSFont labelFontOfSize:kTitleFontSize]];

  if (hasCloseButton) {
    base::scoped_nsobject<HoverImageButton> closeButton(
        [[HoverImageButton alloc] initWithFrame:NSZeroRect]);
    [closeButton setBordered:NO];

    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    NSImage* closeImage = rb->GetNativeImageNamed(IDR_CLOSE_1).ToNSImage();
    CGFloat closeImageWidth = [closeImage size].width;
    [closeButton setDefaultImage:closeImage];
    [closeButton setHoverImage:
        rb->GetNativeImageNamed(IDR_CLOSE_1_H).ToNSImage()];
    [closeButton setPressedImage:
        rb->GetNativeImageNamed(IDR_CLOSE_1_P).ToNSImage()];
    [closeButton setTarget:self];
    [closeButton setAction:@selector(dismissTutorial:)];
    [closeButton setFrameSize:[closeImage size]];
    [closeButton setFrameOrigin:NSMakePoint(
        kFixedMenuWidth - kHorizontalSpacing - closeImageWidth, yOffset)];
    [container addSubview:closeButton];

    [titleLabel setFrameSize:NSMakeSize(
        availableWidth - closeImageWidth - kHorizontalSpacing, 0)];
  } else {
    [titleLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  }

  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleLabel];
  [container addSubview:titleLabel];
  yOffset = NSMaxY([titleLabel frame]) + kVerticalSpacing;

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  [container setFrameOrigin:NSZeroPoint];
  return container.autorelease();
}

- (NSView*)createCurrentProfileView:(const AvatarMenu::Item&)item {
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);

  CGFloat xOffset = kHorizontalSpacing;
  CGFloat yOffset = 0;
  CGFloat availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;
  CGFloat maxAvailableTextWidth = kFixedMenuWidth - kHorizontalSpacing;

  // Profile options. This can be a link to the accounts view, the profile's
  // username for signed in users, or a "Sign in" button for local profiles.
  SigninManagerBase* signinManager =
      SigninManagerFactory::GetForProfile(
          browser_->profile()->GetOriginalProfile());
  if (!isGuestSession_ && signinManager->IsSigninAllowed()) {
    NSView* linksContainer = [self
        createCurrentProfileLinksForItem:item
                                    rect:NSMakeRect(xOffset, yOffset,
                                                    maxAvailableTextWidth, 0)];
    [container addSubview:linksContainer];
    yOffset = NSMaxY([linksContainer frame]);
  }

  // Profile name, centered.
  bool editingAllowed = !isGuestSession_ &&
                        !browser_->profile()->IsLegacySupervised();
  base::scoped_nsobject<EditableProfileNameButton> profileName(
      [[EditableProfileNameButton alloc]
          initWithFrame:NSMakeRect(xOffset,
                                   yOffset,
                                   availableTextWidth,
                                   kProfileButtonHeight)
                profile:browser_->profile()
            profileName:base::SysUTF16ToNSString(
                            profiles::GetAvatarNameForProfile(
                                browser_->profile()->GetPath()))
         editingAllowed:editingAllowed
         withController:self]);

  [container addSubview:profileName];
  yOffset = NSMaxY([profileName frame]) + 4;  // Adds a small vertical padding.

  // Profile icon, centered.
  xOffset = (kFixedMenuWidth - kLargeImageSide) / 2;
  base::scoped_nsobject<EditableProfilePhoto> iconView(
      [[EditableProfilePhoto alloc]
          initWithFrame:NSMakeRect(xOffset, yOffset,
                                   kLargeImageSide, kLargeImageSide)
             avatarMenu:avatarMenu_.get()
            profileIcon:item.icon
         editingAllowed:!isGuestSession_
         withController:self]);

  [container addSubview:iconView];
  yOffset = NSMaxY([iconView frame]);

  if (browser_->profile()->IsSupervised()) {
    base::scoped_nsobject<NSImageView> supervisedIcon(
        [[NSImageView alloc] initWithFrame:NSZeroRect]);
    int imageId = browser_->profile()->IsChild()
        ? IDR_ICON_PROFILES_MENU_CHILD
        : IDR_ICON_PROFILES_MENU_LEGACY_SUPERVISED;
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    [supervisedIcon setImage:rb->GetNativeImageNamed(imageId).ToNSImage()];
    NSSize size = [[supervisedIcon image] size];
    [supervisedIcon setFrameSize:size];
    NSRect parentFrame = [iconView frame];
    [supervisedIcon setFrameOrigin:NSMakePoint(NSMaxX(parentFrame) - size.width,
                                               NSMinY(parentFrame))];
    [container addSubview:supervisedIcon];
  }

  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileLinksForItem:(const AvatarMenu::Item&)item
                                       rect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  // Don't double-apply the left margin to the sub-views.
  rect.origin.x = 0;

  // Adds right padding.
  const CGFloat kRightPadding = kHorizontalSpacing;
  rect.size.width -= kRightPadding;

  // The available links depend on the type of profile that is active.
  if (item.signed_in) {
    // Signed in profiles with no authentication errors do not have a clickable
    // email link.
    NSButton* link = nil;
    if (switches::IsEnableAccountConsistency()) {
      NSString* linkTitle = l10n_util::GetNSString(
          viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ?
              IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON :
              IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      SEL linkSelector =
          (viewMode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) ?
          @selector(showAccountManagement:) : @selector(hideAccountManagement:);
      link = [self linkButtonWithTitle:linkTitle
                           frameOrigin:rect.origin
                                action:linkSelector];
    } else {
      link = [self linkButtonWithTitle:base::SysUTF16ToNSString(item.username)
                           frameOrigin:rect.origin
                                action:nil];
      if (HasAuthError(browser_->profile())) {
        [link setImage:ui::ResourceBundle::GetSharedInstance().
            GetNativeImageNamed(IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).
            ToNSImage()];
        [link setImagePosition:NSImageRight];
        [link setTarget:self];
        [link setAction:@selector(showAccountReauthenticationView:)];
        [link setTag:kPrimaryProfileTag];
        [[link cell]
            accessibilitySetOverrideValue:l10n_util::GetNSStringF(
            IDS_PROFILES_ACCOUNT_BUTTON_AUTH_ERROR_ACCESSIBLE_NAME,
            item.username)
                             forAttribute:NSAccessibilityTitleAttribute];
      } else {
        [link setEnabled:NO];
      }
    }
    // -linkButtonWithTitle sizeToFit's the link. We can use the height, but
    // need to re-stretch the width so that the link can be centered correctly
    // in the view.
    rect.size.height = [link frame].size.height;
    [link setAlignment:NSCenterTextAlignment];
    [link setFrame:rect];
    [container addSubview:link];
    [container setFrameSize:rect.size];
  } else {
    rect.size.height = kBlueButtonHeight;
    NSButton* signinButton = [[BlueLabelButton alloc] initWithFrame:rect];

    // Manually elide the button text so that the contents fit inside the bubble
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    [signinButton setTitle:elidedButtonText];
    [signinButton setTarget:self];
    [signinButton setAction:@selector(showInlineSigninPage:)];
    [container addSubview:signinButton];

    // Sign-in promo text.
    NSTextField* promo = BuildLabel(
        l10n_util::GetNSString(IDS_PROFILES_SIGNIN_PROMO),
        NSMakePoint(0, NSMaxY([signinButton frame]) + kVerticalSpacing),
        nil);
    if (kRightPadding >= 8)
      rect.size.width += 8;  // Re-stretch a little bit to fit promo text.
    DCHECK(kRightPadding >= 8);
    [promo setFrameSize:NSMakeSize(rect.size.width, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:promo];
    [container addSubview:promo];

    [container setFrameSize:NSMakeSize(
        rect.size.width,
        NSMaxY([promo frame]) + 4)];  // Adds a small vertical padding.
    content::RecordAction(
        base::UserMetricsAction("Signin_Impression_FromAvatarBubbleSignin"));
  }

  return container.autorelease();
}

- (NSView*)createSupervisedUserDisclaimerView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);

  int yOffset = 0;
  int availableTextWidth = kFixedMenuWidth - 2 * kHorizontalSpacing;

  NSTextField* disclaimer = BuildLabel(
      base::SysUTF16ToNSString(avatarMenu_->GetSupervisedUserInformation()),
      NSMakePoint(kHorizontalSpacing, yOffset), nil);
  [disclaimer setFrameSize:NSMakeSize(availableTextWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:disclaimer];
  yOffset = NSMaxY([disclaimer frame]);

  [container addSubview:disclaimer];
  [container setFrameSize:NSMakeSize(kFixedMenuWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)createGuestProfileView {
  gfx::Image guestIcon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guestItem(std::string::npos, /* menu_index, not used */
                             std::string::npos, /* profile_index, not used */
                             guestIcon);
  guestItem.active = true;
  guestItem.name = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_PROFILES_GUEST_PROFILE_NAME));

  return [self createCurrentProfileView:guestItem];
}

- (NSButton*)createOtherProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);

  NSRect rect = NSMakeRect(
      0, 0, kFixedMenuWidth, kBlueButtonHeight + kSmallVerticalSpacing);
  base::scoped_nsobject<BackgroundColorHoverButton> profileButton(
      [[BackgroundColorHoverButton alloc]
          initWithFrame:rect
      imageTitleSpacing:kImageTitleSpacing
        backgroundColor:GetDialogBackgroundColor()]);

  NSString* title = base::SysUTF16ToNSString(
      profiles::GetProfileSwitcherTextForItem(item));
  [profileButton setTitle:title];

  // Use the low-res, small default avatars in the fast user switcher, like
  // we do in the menu bar.
  gfx::Image itemIcon;
  bool isRectangle;
  AvatarMenu::GetImageForMenuButton(item.profile_path, &itemIcon, &isRectangle);

  [profileButton setDefaultImage:CreateProfileImage(
      itemIcon, kSmallImageSide).ToNSImage()];
  [profileButton setImagePosition:NSImageLeft];
  [profileButton setAlignment:NSLeftTextAlignment];
  [profileButton setBordered:NO];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];

  NSSize textSize = [[profileButton title] sizeWithAttributes:@{
    NSFontAttributeName : [profileButton font]
  }];

  CGFloat availableWidth = rect.size.width - kSmallImageSide -
                           kImageTitleSpacing - kHorizontalSpacing;

  if (std::ceil(textSize.width) > availableWidth)
    [profileButton setToolTip:[profileButton title]];

  return profileButton.autorelease();
}

- (NSView*)createOptionsViewWithRect:(NSRect)rect
                         displayLock:(BOOL)displayLock {
  NSRect viewRect = NSMakeRect(0, 0,
                               rect.size.width,
                               kBlueButtonHeight + kSmallVerticalSpacing);
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  if (displayLock) {
    NSButton* lockButton =
        [self hoverButtonWithRect:viewRect
                             text:l10n_util::GetNSString(
                                  IDS_PROFILES_PROFILE_SIGNOUT_BUTTON)
                  imageResourceId:IDR_ICON_PROFILES_MENU_LOCK
                           action:@selector(lockProfile:)];
    [container addSubview:lockButton];
    viewRect.origin.y = NSMaxY([lockButton frame]);

    NSBox* separator = [self horizontalSeparatorWithFrame:viewRect];
    [container addSubview:separator];
    viewRect.origin.y = NSMaxY([separator frame]);
  }

  if ([self shouldShowGoIncognito]) {
    NSButton* goIncognitoButton =
        [self hoverButtonWithRect:viewRect
                             text:l10n_util::GetNSString(
                                  IDS_PROFILES_GO_INCOGNITO_BUTTON)
                  imageResourceId:IDR_ICON_PROFILES_MENU_INCOGNITO
                           action:@selector(goIncognito:)];
    viewRect.origin.y = NSMaxY([goIncognitoButton frame]);
    [container addSubview:goIncognitoButton];

    NSBox* separator = [self horizontalSeparatorWithFrame:viewRect];
    [container addSubview:separator];
    viewRect.origin.y = NSMaxY([separator frame]);
  }

  NSString* text = isGuestSession_ ?
      l10n_util::GetNSString(IDS_PROFILES_EXIT_GUEST) :
      l10n_util::GetNSString(IDS_PROFILES_SWITCH_USERS_BUTTON);
  NSButton* switchUsersButton =
      [self hoverButtonWithRect:viewRect
                           text:text
                imageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
                         action:isGuestSession_? @selector(exitGuest:) :
                                                 @selector(showUserManager:)];
  viewRect.origin.y = NSMaxY([switchUsersButton frame]);
  [container addSubview:switchUsersButton];

  [container setFrameSize:NSMakeSize(rect.size.width, viewRect.origin.y)];
  return container.autorelease();
}

- (NSView*)createCurrentProfileAccountsView:(NSRect)rect {
  const CGFloat kAccountButtonHeight = 34;

  const AvatarMenu::Item& item =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());
  DCHECK(item.signed_in);

  NSColor* backgroundColor = skia::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<NSView> container([[BackgroundColorView alloc]
      initWithFrame:rect
          withColor:backgroundColor]);

  rect.origin.y = 0;
  if (!browser_->profile()->IsSupervised()) {
    // Manually elide the button text so the contents fit inside the bubble.
    // This is needed because the BlueLabelButton cell resets the style on
    // every call to -cellSize, which prevents setting a custom lineBreakMode.
    NSString* elidedButtonText = base::SysUTF16ToNSString(gfx::ElideText(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, item.name),
        gfx::FontList(), rect.size.width, gfx::ELIDE_TAIL));

    NSButton* addAccountsButton =
        [self linkButtonWithTitle:elidedButtonText
                      frameOrigin:NSMakePoint(
            kHorizontalSpacing, kSmallVerticalSpacing)
                           action:@selector(addAccount:)];
    [container addSubview:addAccountsButton];
    rect.origin.y += kAccountButtonHeight;
  }

  NSView* accountEmails = [self createAccountsListWithRect:NSMakeRect(
      0, rect.origin.y, rect.size.width, kAccountButtonHeight)];
  [container addSubview:accountEmails];

  [container setFrameSize:NSMakeSize(rect.size.width,
                                     NSMaxY([accountEmails frame]))];
  return container.autorelease();
}

- (NSView*)createAccountsListWithRect:(NSRect)rect {
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);
  currentProfileAccounts_.clear();

  Profile* profile = browser_->profile();
  std::string primaryAccount =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedAccountId();
  DCHECK(!primaryAccount.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primaryAccount);

  // If there is an account with an authentication error, it needs to be
  // badged with a warning icon.
  std::string errorAccountId = GetAuthErrorAccountId(profile);

  rect.origin.y = 0;
  for (size_t i = 0; i < accounts.size(); ++i) {
    // Save the original email address, as the button text could be elided.
    currentProfileAccounts_[i] = accounts[i];
    NSButton* accountButton =
        [self accountButtonWithRect:rect
                         accountId:accounts[i]
                                tag:i
                     reauthRequired:errorAccountId == accounts[i]];
    [container addSubview:accountButton];
    rect.origin.y = NSMaxY([accountButton frame]);
  }

  // The primary account should always be listed first.
  NSButton* accountButton =
      [self accountButtonWithRect:rect
                        accountId:primaryAccount
                              tag:kPrimaryProfileTag
                   reauthRequired:errorAccountId == primaryAccount];
  [container addSubview:accountButton];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]),
                                     NSMaxY([accountButton frame]))];
  return container.autorelease();
}

- (NSView*)buildGaiaEmbeddedView {
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat yOffset = 0;

  int messageId = -1;
  switch (viewMode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      messageId = IDS_PROFILES_GAIA_SIGNIN_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      messageId = IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      DCHECK(HasAuthError(browser_->profile()));
      messageId = IDS_PROFILES_GAIA_REAUTH_TITLE;
      break;
    default:
      NOTREACHED() << "Called with invalid mode=" << viewMode_;
      break;
  }

  webContentsDelegate_.reset(new GaiaWebContentsDelegate());
  webContents_ = SigninViewControllerDelegateMac::CreateGaiaWebContents(
      webContentsDelegate_.get(), viewMode_, browser_->profile(), accessPoint_);

  NSView* webview = webContents_->GetNativeView();

  [container addSubview:webview];
  yOffset = NSMaxY([webview frame]);

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedGaiaViewWidth, 0),
      l10n_util::GetStringUTF16(messageId),
      self /* backButtonTarget*/,
      @selector(navigateBackFromSigninPage:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedGaiaViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildAccountRemovalView {
  DCHECK(!accountIdToRemove_.empty());

  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedAccountRemovalViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = kVerticalSpacing;

  const std::string& primaryAccount = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool isPrimaryAccount = primaryAccount == accountIdToRemove_;

  // Adds "remove account" button at the bottom if needed.
  if (!isPrimaryAccount) {
    base::scoped_nsobject<NSButton> removeAccountButton(
        [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
    [removeAccountButton setTitle:l10n_util::GetNSString(
        IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON)];
    [removeAccountButton setTarget:self];
    [removeAccountButton setAction:@selector(removeAccount:)];
    [removeAccountButton sizeToFit];
    [removeAccountButton setAlignment:NSCenterTextAlignment];
    CGFloat xOffset = (kFixedAccountRemovalViewWidth -
        NSWidth([removeAccountButton frame])) / 2;
    [removeAccountButton setFrameOrigin:NSMakePoint(xOffset, yOffset)];
    [container addSubview:removeAccountButton];

    yOffset = NSMaxY([removeAccountButton frame]) + kVerticalSpacing;
  }

  NSView* contentView;
  NSPoint contentFrameOrigin = NSMakePoint(kHorizontalSpacing, yOffset);
  if (isPrimaryAccount) {
    std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                        accountIdToRemove_);
    std::vector<size_t> offsets;
    NSString* contentStr = l10n_util::GetNSStringF(
        IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
        base::UTF8ToUTF16(email), base::string16(), &offsets);
    NSString* linkStr = l10n_util::GetNSString(IDS_PROFILES_SETTINGS_LINK);
    contentView = BuildFixedWidthTextViewWithLink(self, contentStr, linkStr,
        offsets[1], contentFrameOrigin, availableWidth);
  } else {
    NSString* contentStr =
        l10n_util::GetNSString(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT);
    NSTextField* contentLabel = BuildLabel(contentStr, contentFrameOrigin, nil);
    [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
    contentView = contentLabel;
  }
  [container addSubview:contentView];
  yOffset = NSMaxY([contentView frame]) + kVerticalSpacing;

  // Adds the title card.
  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedAccountRemovalViewWidth,0),
      l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TITLE),
      self /* backButtonTarget*/,
      @selector(showAccountManagement:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedAccountRemovalViewWidth, yOffset)];
  return container.autorelease();
}

- (NSView*)buildSwitchUserView {
  ProfileMetrics::LogProfileNewAvatarMenuNotYou(
      ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_VIEW);
  base::scoped_nsobject<NSView> container(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  CGFloat availableWidth =
      kFixedSwitchUserViewWidth - 2 * kHorizontalSpacing;
  CGFloat yOffset = 0;
  NSRect viewRect = NSMakeRect(0, yOffset,
                               kFixedSwitchUserViewWidth,
                               kBlueButtonHeight + kSmallVerticalSpacing);

  const AvatarMenu::Item& avatarItem =
      avatarMenu_->GetItemAt(avatarMenu_->GetActiveProfileIndex());

  // Adds "Disconnect your Google Account" button at the bottom.
  NSButton* disconnectButton =
      [self hoverButtonWithRect:viewRect
                           text:l10n_util::GetNSString(
                                    IDS_PROFILES_DISCONNECT_BUTTON)
                imageResourceId:IDR_ICON_PROFILES_MENU_DISCONNECT
                         action:@selector(disconnectProfile:)];
  [container addSubview:disconnectButton];
  yOffset = NSMaxY([disconnectButton frame]);

  NSBox* separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // Adds "Add person" button.
  viewRect.origin.y = yOffset;
  NSButton* addPersonButton =
      [self hoverButtonWithRect:viewRect
                           text:l10n_util::GetNSString(
                                    IDS_PROFILES_ADD_PERSON_BUTTON)
                imageResourceId:IDR_ICON_PROFILES_MENU_AVATAR
                         action:@selector(showUserManager:)];
  [container addSubview:addPersonButton];
  yOffset = NSMaxY([addPersonButton frame]);

  separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]);

  // Adds the content text.
  base::string16 elidedName(gfx::ElideText(
      avatarItem.name, gfx::FontList(), availableWidth, gfx::ELIDE_TAIL));
  NSTextField* contentLabel = BuildLabel(
      l10n_util::GetNSStringF(IDS_PROFILES_NOT_YOU_CONTENT_TEXT, elidedName),
      NSMakePoint(kHorizontalSpacing, yOffset + kVerticalSpacing),
      nil);
  [contentLabel setFrameSize:NSMakeSize(availableWidth, 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:contentLabel];
  [container addSubview:contentLabel];
  yOffset = NSMaxY([contentLabel frame]) + kVerticalSpacing;

  // Adds the title card.
  separator = [self horizontalSeparatorWithFrame:
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth, 0)];
  [container addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  NSView* titleView = BuildTitleCard(
      NSMakeRect(0, yOffset, kFixedSwitchUserViewWidth,0),
      l10n_util::GetStringFUTF16(IDS_PROFILES_NOT_YOU, avatarItem.name),
      self /* backButtonTarget*/,
      @selector(navigateBackFromSwitchUserView:) /* backButtonAction */);
  [container addSubview:titleView];
  yOffset = NSMaxY([titleView frame]);

  [container setFrameSize:NSMakeSize(kFixedSwitchUserViewWidth, yOffset)];
  return container.autorelease();
}

// Called when clicked on the settings link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  chrome::ShowSettings(browser_);
  return YES;
}

- (NSButton*)hoverButtonWithRect:(NSRect)rect
                            text:(NSString*)text
                 imageResourceId:(int)imageResourceId
                          action:(SEL)action {
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc]
          initWithFrame:rect
      imageTitleSpacing:kImageTitleSpacing
        backgroundColor:GetDialogBackgroundColor()]);

  [button setTitle:text];
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* image = rb->GetNativeImageNamed(imageResourceId).ToNSImage();
  [button setDefaultImage:image];
  [button setHoverImage:image];
  [button setPressedImage:image];
  [button setImagePosition:NSImageLeft];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];
  [button setTarget:self];
  [button setAction:action];

  return button.autorelease();
}

- (NSButton*)linkButtonWithTitle:(NSString*)title
                     frameOrigin:(NSPoint)frameOrigin
                          action:(SEL)action {
  base::scoped_nsobject<NSButton> link(
      [[HyperlinkButtonCell buttonWithString:title] retain]);

  [[link cell] setTextColor:skia::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor())];
  [link setTitle:title];
  [link setBordered:NO];
  [link setFont:[NSFont labelFontOfSize:kTextFontSize]];
  [link setTarget:self];
  [link setAction:action];
  [link setFrameOrigin:frameOrigin];
  [link sizeToFit];

  return link.autorelease();
}

- (NSButton*)accountButtonWithRect:(NSRect)rect
                         accountId:(const std::string&)accountId
                               tag:(int)tag
                    reauthRequired:(BOOL)reauthRequired {
  // Get display email address for account.
  std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                      accountId);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  NSImage* deleteImage = rb->GetNativeImageNamed(IDR_CLOSE_1).ToNSImage();
  CGFloat deleteImageWidth = [deleteImage size].width;
  NSImage* warningImage = reauthRequired ? rb->GetNativeImageNamed(
      IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).ToNSImage() : nil;
  CGFloat warningImageWidth = [warningImage size].width;

  CGFloat availableTextWidth = rect.size.width - kHorizontalSpacing -
      warningImageWidth - deleteImageWidth;
  if (warningImage)
    availableTextWidth -= kHorizontalSpacing;

  NSColor* backgroundColor = skia::SkColorToCalibratedNSColor(
      profiles::kAvatarBubbleAccountsBackgroundColor);
  base::scoped_nsobject<BackgroundColorHoverButton> button(
      [[BackgroundColorHoverButton alloc] initWithFrame:rect
                                      imageTitleSpacing:0
                                        backgroundColor:backgroundColor]);
  [button setTitle:ElideEmail(email, availableTextWidth)];
  [button setAlignment:NSLeftTextAlignment];
  [button setBordered:NO];
  if (reauthRequired) {
    [button setDefaultImage:warningImage];
    [button setImagePosition:NSImageLeft];
    [button setTarget:self];
    [button setAction:@selector(showAccountReauthenticationView:)];
    [button setTag:tag];
  }

  // Delete button.
  if (!browser_->profile()->IsSupervised()) {
    NSRect buttonRect;
    NSDivideRect(rect, &buttonRect, &rect,
        deleteImageWidth + kHorizontalSpacing, NSMaxXEdge);
    buttonRect.origin.y = 0;

    base::scoped_nsobject<HoverImageButton> deleteButton(
        [[HoverImageButton alloc] initWithFrame:buttonRect]);
    [deleteButton setBordered:NO];
    [deleteButton setDefaultImage:deleteImage];
    [deleteButton setHoverImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_H).ToNSImage()];
    [deleteButton setPressedImage:rb->GetNativeImageNamed(
        IDR_CLOSE_1_P).ToNSImage()];
    [deleteButton setTarget:self];
    [deleteButton setAction:@selector(showAccountRemovalView:)];
    [deleteButton setTag:tag];

    [button addSubview:deleteButton];
  }

  return button.autorelease();
}

- (void)postActionPerformed:(ProfileMetrics::ProfileDesktopMenu)action {
  ProfileMetrics::LogProfileDesktopMenu(action, serviceType_);
  serviceType_ = signin::GAIA_SERVICE_TYPE_NONE;
}

- (bool)shouldShowGoIncognito {
  bool incognitoAvailable =
      IncognitoModePrefs::GetAvailability(browser_->profile()->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  return incognitoAvailable && !browser_->profile()->IsGuestSession();
}

@end
