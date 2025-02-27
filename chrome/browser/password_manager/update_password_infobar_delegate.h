// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/password_manager/password_manager_infobar_delegate.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace content {
class WebContents;
}

// An infobar delegate which asks the user if the password should be updated for
// a set of saved credentials for a site.  If several such sets are present, the
// user can choose which one to update.  PasswordManager displays this infobar
// when the user signs into the site with a new password for a known username
// or fills in a password change form.
class UpdatePasswordInfoBarDelegate : public PasswordManagerInfoBarDelegate {
 public:
  static void Create(
      content::WebContents* web_contents,
      scoped_ptr<password_manager::PasswordFormManager> form_to_update);

  ~UpdatePasswordInfoBarDelegate() override;

  base::string16 GetBranding() const;
  bool is_smartlock_branding_enabled() const {
    return is_smartlock_branding_enabled_;
  }

  // Returns whether the user has multiple saved credentials, of which the
  // infobar affects just one. In this case the infobar should clarify which
  // credential is being affected.
  bool ShowMultipleAccounts() const;

  const std::vector<const autofill::PasswordForm*>& GetCurrentForms() const;

  // Returns the username of the saved credentials in the case when there is
  // only one credential pair stored.
  base::string16 get_username_for_single_account() {
    return passwords_state_.form_manager()
        ->pending_credentials()
        .username_value;
  }

 private:
  UpdatePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      scoped_ptr<password_manager::PasswordFormManager> form_to_update,
      bool is_smartlock_branding_enabled);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  ManagePasswordsState passwords_state_;
  base::string16 branding_;
  bool is_smartlock_branding_enabled_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePasswordInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_
