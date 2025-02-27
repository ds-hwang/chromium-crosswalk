// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/common/features.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace password_manager {
enum class CredentialType;
struct CredentialInfo;
struct InteractionsStats;
class PasswordFormManager;
}

class AccountChooserPrompt;
class AutoSigninFirstRunPrompt;
class ManagePasswordsIconView;
class PasswordDialogController;
class PasswordDialogControllerImpl;

// Per-tab class to control the Omnibox password icon and bubble.
class ManagePasswordsUIController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsUIController>,
      public password_manager::PasswordStore::Observer,
      public PasswordsModelDelegate,
      public PasswordsClientUIDelegate {
 public:
  ~ManagePasswordsUIController() override;

  // PasswordsClientUIDelegate:
  void OnPasswordSubmitted(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) override;
  void OnUpdatePasswordSubmitted(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) override;
  bool OnChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback)
      override;
  void OnAutoSignin(ScopedVector<autofill::PasswordForm> local_forms) override;
  void OnPromptEnableAutoSignin() override;
  void OnAutomaticPasswordSave(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) override;
  void OnPasswordAutofilled(
      const autofill::PasswordFormMap& password_form_map,
      const GURL& origin,
      const std::vector<scoped_ptr<autofill::PasswordForm>>* federated_matches)
      override;

  // PasswordStore::Observer:
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // Set the state of the Omnibox icon, and possibly show the associated bubble
  // without user interaction.
  virtual void UpdateIconAndBubbleState(ManagePasswordsIconView* icon);

  bool IsAutomaticallyOpeningBubble() const {
    return bubble_status_ == SHOULD_POP_UP;
  }

  // PasswordsModelDelegate:
  const GURL& GetOrigin() const override;
  password_manager::ui::State GetState() const override;
  const autofill::PasswordForm& GetPendingPassword() const override;
  bool IsPasswordOverridden() const override;
  const std::vector<const autofill::PasswordForm*>& GetCurrentForms()
      const override;
  const std::vector<const autofill::PasswordForm*>& GetFederatedForms()
      const override;
  password_manager::InteractionsStats* GetCurrentInteractionStats() const
      override;
  void OnBubbleShown() override;
  void OnBubbleHidden() override;
  void OnNoInteractionOnUpdate() override;
  void OnNopeUpdateClicked() override;
  void NeverSavePassword() override;
  void SavePassword() override;
  void UpdatePassword(const autofill::PasswordForm& password_form) override;
  void ChooseCredential(
      autofill::PasswordForm form,
      password_manager::CredentialType credential_type) override;
  void NavigateToExternalPasswordManager() override;
  void NavigateToSmartLockHelpPage() override;
  void NavigateToPasswordManagerSettingsPage() override;
  void OnDialogHidden() override;

 protected:
  explicit ManagePasswordsUIController(
      content::WebContents* web_contents);

  // The pieces of saving and blacklisting passwords that interact with
  // FormManager, split off into internal functions for testing/mocking.
  virtual void SavePasswordInternal();
  virtual void UpdatePasswordInternal(
      const autofill::PasswordForm& password_form);
  virtual void NeverSavePasswordInternal();

  // Called when a PasswordForm is autofilled, when a new PasswordForm is
  // submitted, or when a navigation occurs to update the visibility of the
  // manage passwords icon and bubble.
  virtual void UpdateBubbleAndIconVisibility();

  // Called to create the account chooser dialog. Mocked in tests.
  virtual AccountChooserPrompt* CreateAccountChooser(
      PasswordDialogController* controller);

  // Called to create the account chooser dialog. Mocked in tests.
  virtual AutoSigninFirstRunPrompt* CreateAutoSigninPrompt(
      PasswordDialogController* controller);

  // Overwrites the client for |passwords_data_|.
  void set_client(password_manager::PasswordManagerClient* client) {
    passwords_data_.set_client(client);
  }

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void WasHidden() override;

 private:
  friend class content::WebContentsUserData<ManagePasswordsUIController>;

  enum BubbleStatus {
    NOT_SHOWN,
    // The bubble is to be popped up in the next call to
    // UpdateBubbleAndIconVisibility().
    SHOULD_POP_UP,
    SHOWN,
  };

  // Shows the password bubble without user interaction.
  void ShowBubbleWithoutUserInteraction();

  // Closes the account chooser gracefully so the callback is called. Then sets
  // the state to MANAGE_STATE.
  void DestroyAccountChooser();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // The wrapper around current state and data.
  ManagePasswordsState passwords_data_;

  // The controller for the blocking dialogs.
  scoped_ptr<PasswordDialogControllerImpl> dialog_controller_;

  BubbleStatus bubble_status_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsUIController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
