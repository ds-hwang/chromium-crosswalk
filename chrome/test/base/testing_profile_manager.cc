// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include <stddef.h>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

const char kGuestProfileName[] = "Guest";
const char kSystemProfileName[] = "System";

namespace testing {

class ProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit ProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  Profile* CreateProfileHelper(const base::FilePath& file_path) override {
    return new TestingProfile(file_path);
  }
};

}  // namespace testing

TestingProfileManager::TestingProfileManager(TestingBrowserProcess* process)
    : called_set_up_(false),
      browser_process_(process),
      local_state_(process),
      profile_manager_(NULL) {
}

TestingProfileManager::~TestingProfileManager() {
  // Destroying this class also destroys the LocalState, so make sure the
  // associated ProfileManager is also destroyed.
  browser_process_->SetProfileManager(NULL);
}

bool TestingProfileManager::SetUp() {
  SetUpInternal();
  return called_set_up_;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& profile_name,
    scoped_ptr<syncable_prefs::PrefServiceSyncable> prefs,
    const base::string16& user_name,
    int avatar_id,
    const std::string& supervised_user_id,
    const TestingProfile::TestingFactories& factories) {
  DCHECK(called_set_up_);

  // Create a path for the profile based on the name.
  base::FilePath profile_path(profiles_dir_.path());
#if defined(OS_CHROMEOS)
  if (profile_name != chrome::kInitialProfile) {
    profile_path =
        profile_path.Append(chromeos::ProfileHelper::Get()->GetUserProfileDir(
            chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
                profile_name)));
  } else {
    profile_path = profile_path.AppendASCII(profile_name);
  }
#else
  profile_path = profile_path.AppendASCII(profile_name);
#endif

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(profile_path);
  builder.SetPrefService(std::move(prefs));
  builder.SetSupervisedUserId(supervised_user_id);

  for (TestingProfile::TestingFactories::const_iterator it = factories.begin();
       it != factories.end(); ++it) {
    builder.AddTestingFactory(it->first, it->second);
  }

  TestingProfile* profile = builder.Build().release();
  profile->set_profile_name(profile_name);
  profile_manager_->AddProfile(profile);  // Takes ownership.

  // Update the user metadata.
  ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  cache.SetAvatarIconOfProfileAtIndex(index, avatar_id);
  cache.SetSupervisedUserIdOfProfileAtIndex(index, supervised_user_id);
  // SetNameOfProfileAtIndex may reshuffle the list of profiles, so we do it
  // last.
  cache.SetNameOfProfileAtIndex(index, user_name);

  testing_profiles_.insert(std::make_pair(profile_name, profile));

  return profile;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(name,
                              scoped_ptr<syncable_prefs::PrefServiceSyncable>(),
                              base::UTF8ToUTF16(name), 0, std::string(),
                              TestingProfile::TestingFactories());
}

TestingProfile* TestingProfileManager::CreateGuestProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());

  // Add the guest profile to the profile manager, but not to the info cache.
  TestingProfile* profile = builder.Build().release();
  profile->set_profile_name(kGuestProfileName);

  // Set up a profile with an off the record profile.
  TestingProfile::Builder().BuildIncognito(profile);

  profile_manager_->AddProfile(profile);  // Takes ownership.
  profile_manager_->SetNonPersonalProfilePrefs(profile);

  testing_profiles_.insert(std::make_pair(kGuestProfileName, profile));

  return profile;
}

TestingProfile* TestingProfileManager::CreateSystemProfile() {
  DCHECK(called_set_up_);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(ProfileManager::GetSystemProfilePath());

  // Add the system profile to the profile manager, but not to the info cache.
  TestingProfile* profile = builder.Build().release();
  profile->set_profile_name(kSystemProfileName);

  profile_manager_->AddProfile(profile);  // Takes ownership.

  testing_profiles_.insert(std::make_pair(kSystemProfileName, profile));

  return profile;
}

void TestingProfileManager::DeleteTestingProfile(const std::string& name) {
  DCHECK(called_set_up_);

  TestingProfilesMap::iterator it = testing_profiles_.find(name);
  DCHECK(it != testing_profiles_.end());

  TestingProfile* profile = it->second;

  ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  cache.DeleteProfileFromCache(profile->GetPath());

  profile_manager_->profiles_info_.erase(profile->GetPath());

  testing_profiles_.erase(it);
}

void TestingProfileManager::DeleteAllTestingProfiles() {
  for (TestingProfilesMap::iterator it = testing_profiles_.begin();
       it != testing_profiles_.end(); ++it) {
    TestingProfile* profile = it->second;
    ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
    cache.DeleteProfileFromCache(profile->GetPath());
  }
  testing_profiles_.clear();
}


void TestingProfileManager::DeleteGuestProfile() {
  DCHECK(called_set_up_);

  TestingProfilesMap::iterator it = testing_profiles_.find(kGuestProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(ProfileManager::GetGuestProfilePath());
}

void TestingProfileManager::DeleteSystemProfile() {
  DCHECK(called_set_up_);

  TestingProfilesMap::iterator it = testing_profiles_.find(kSystemProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(
      ProfileManager::GetSystemProfilePath());
}

void TestingProfileManager::DeleteProfileInfoCache() {
  profile_manager_->profile_info_cache_.reset(NULL);
}

void TestingProfileManager::SetLoggedIn(bool logged_in) {
  profile_manager_->logged_in_ = logged_in;
}

void TestingProfileManager::UpdateLastUser(Profile* last_active) {
#if !defined(OS_ANDROID)
  profile_manager_->UpdateLastUser(last_active);
#endif
}

const base::FilePath& TestingProfileManager::profiles_dir() {
  DCHECK(called_set_up_);
  return profiles_dir_.path();
}

ProfileManager* TestingProfileManager::profile_manager() {
  DCHECK(called_set_up_);
  return profile_manager_;
}

ProfileInfoCache* TestingProfileManager::profile_info_cache() {
  DCHECK(called_set_up_);
  return &profile_manager_->GetProfileInfoCache();
}

ProfileAttributesStorage* TestingProfileManager::profile_attributes_storage() {
  return profile_info_cache();
}

void TestingProfileManager::SetUpInternal() {
  ASSERT_FALSE(browser_process_->profile_manager())
      << "ProfileManager already exists";

  // Set up the directory for profiles.
  ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());

  profile_manager_ = new testing::ProfileManager(profiles_dir_.path());
  browser_process_->SetProfileManager(profile_manager_);  // Takes ownership.

  profile_manager_->GetProfileInfoCache().
      set_disable_avatar_download_for_testing(true);
  called_set_up_ = true;
}
