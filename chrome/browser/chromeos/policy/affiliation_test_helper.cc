// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace affiliation_test_helper {

const char kFakeRefreshToken[] = "fake-refresh-token";
const char kEnterpriseUser[] = "testuser@example.com";

void SetUserKeys(policy::UserPolicyBuilder* user_policy) {
  std::string username = user_policy->policy_data().username();
  base::FilePath user_keys_dir;
  ASSERT_TRUE(PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &user_keys_dir));
  const std::string sanitized_username =
      chromeos::CryptohomeClient::GetStubSanitizedUsername(username);
  const base::FilePath user_key_file =
      user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
  std::vector<uint8_t> user_key_bits;
  ASSERT_TRUE(user_policy->GetSigningKey()->ExportPublicKey(&user_key_bits));
  ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
  ASSERT_EQ(base::WriteFile(user_key_file,
                            reinterpret_cast<const char*>(user_key_bits.data()),
                            user_key_bits.size()),
            static_cast<int>(user_key_bits.size()));
}

void SetDeviceAffiliationID(
    policy::DevicePolicyCrosTestHelper* test_helper,
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    const std::set<std::string>& device_affiliation_ids) {
  test_helper->InstallOwnerKey();
  test_helper->MarkAsEnterpriseOwned();

  policy::DevicePolicyBuilder* device_policy = test_helper->device_policy();
  for (const auto& device_affiliation_id : device_affiliation_ids) {
    device_policy->policy_data().add_device_affiliation_ids(
        device_affiliation_id);
  }
  device_policy->SetDefaultSigningKey();
  device_policy->Build();

  fake_session_manager_client->set_device_policy(device_policy->GetBlob());
  fake_session_manager_client->OnPropertyChangeComplete(true);
}

void SetUserAffiliationIDs(
    policy::UserPolicyBuilder* user_policy,
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    const std::string& user_email,
    const std::set<std::string>& user_affiliation_ids) {
  user_policy->policy_data().set_username(user_email);
  SetUserKeys(user_policy);
  for (const auto& user_affiliation_id : user_affiliation_ids) {
    user_policy->policy_data().add_user_affiliation_ids(user_affiliation_id);
  }
  user_policy->Build();
  fake_session_manager_client->set_user_policy(user_email,
                                               user_policy->GetBlob());
}

void PreLoginUser(const std::string& user_id) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(new base::StringValue(user_id));
  chromeos::StartupUtils::MarkOobeCompleted();
}

void LoginUser(const std::string& user_id) {
  chromeos::test::UserSessionManagerTestApi session_manager_test_api(
      chromeos::UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  chromeos::UserContext user_context(AccountId::FromUserEmail(user_id));
  user_context.SetGaiaID("gaia-id-" + user_id);
  user_context.SetKey(chromeos::Key("password"));
  if (user_id == kEnterpriseUser) {
    user_context.SetRefreshToken(kFakeRefreshToken);
  }
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  CHECK(controller);
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  controller->Login(user_context, chromeos::SigninSpecifics());
  observer.Wait();

  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end(); ++it) {
    if ((*it)->email() == user_context.GetAccountId().GetUserEmail())
      return;
  }
  ADD_FAILURE() << user_id << " was not added via PreLoginUser()";
}

void AppendCommandLineSwitchesForLoginManager(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  // LoginManager tests typically don't stand up a policy test server but
  // instead inject policies directly through a SessionManagerClient. So allow
  // policy fetches to fail - this is expected.
  command_line->AppendSwitch(
      chromeos::switches::kAllowFailedPolicyFetchForTest);
}

}  // namespace affiliation_test_helper

}  // namespace policy
