// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_factory.h"

#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/os_crypt/os_crypt_switches.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#include "chrome/browser/password_manager/password_store_win.h"
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_store_proxy_mac.h"
#include "crypto/apple_keychain.h"
#include "crypto/mock_apple_keychain.h"
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
// Don't do anything. We're going to use the default store.
#elif defined(USE_X11)
#include "base/nix/xdg_util.h"
#if defined(USE_GNOME_KEYRING)
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#endif
#if defined(USE_LIBSECRET)
#include "chrome/browser/password_manager/native_backend_libsecret.h"
#endif
#include "chrome/browser/password_manager/native_backend_kwallet_x.h"
#include "chrome/browser/password_manager/password_store_x.h"
#endif

using password_manager::PasswordStore;

namespace {

#if !defined(OS_CHROMEOS) && defined(USE_X11)
const LocalProfileId kInvalidLocalProfileId =
    static_cast<LocalProfileId>(0);
#endif

}  // namespace

// static
scoped_refptr<PasswordStore> PasswordStoreFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord())
    return nullptr;
  return make_scoped_refptr(static_cast<password_manager::PasswordStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get()));
}

// static
PasswordStoreFactory* PasswordStoreFactory::GetInstance() {
  return base::Singleton<PasswordStoreFactory>::get();
}

// static
void PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(
    Profile* profile) {
  scoped_refptr<PasswordStore> password_store =
      GetForProfile(profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!password_store)
    return;
  sync_driver::SyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  net::URLRequestContextGetter* request_context_getter =
      profile->GetRequestContext();

  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service, request_context_getter,
      profile->GetPath(), content::BrowserThread::GetMessageLoopProxyForThread(
                              content::BrowserThread::DB));
}

// static
void PasswordStoreFactory::TrimOrDeleteAffiliationCache(Profile* profile) {
  scoped_refptr<PasswordStore> password_store =
      GetForProfile(profile, ServiceAccessType::EXPLICIT_ACCESS);
  password_manager::TrimOrDeleteAffiliationCacheForStoreAndPath(
      password_store.get(), profile->GetPath(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::DB));
}

PasswordStoreFactory::PasswordStoreFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PasswordStore",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

PasswordStoreFactory::~PasswordStoreFactory() {}

#if !defined(OS_CHROMEOS) && defined(USE_X11)
LocalProfileId PasswordStoreFactory::GetLocalProfileId(
    PrefService* prefs) const {
  LocalProfileId id =
      prefs->GetInteger(password_manager::prefs::kLocalProfileId);
  if (id == kInvalidLocalProfileId) {
    // Note that there are many more users than this. Thus, by design, this is
    // not a unique id. However, it is large enough that it is very unlikely
    // that it would be repeated twice on a single machine. It is still possible
    // for that to occur though, so the potential results of it actually
    // happening should be considered when using this value.
    static const int kLocalProfileIdMask = (1 << 24) - 1;
    do {
      id = base::RandInt(0, kLocalProfileIdMask);
      // TODO(mdm): scan other profiles to make sure they are not using this id?
    } while (id == kInvalidLocalProfileId);
    prefs->SetInteger(password_manager::prefs::kLocalProfileId, id);
  }
  return id;
}
#endif

scoped_refptr<RefcountedKeyedService>
PasswordStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_WIN)
  password_manager_util_win::DelayReportOsPassword();
#endif
  Profile* profile = static_cast<Profile*>(context);

  scoped_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(profile->GetPath()));

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner(
      base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::DB));

  scoped_refptr<PasswordStore> ps;
#if defined(OS_WIN)
  ps = new PasswordStoreWin(main_thread_runner, db_thread_runner,
                            login_db.Pass(),
                            WebDataServiceFactory::GetPasswordWebDataForProfile(
                                profile, ServiceAccessType::EXPLICIT_ACCESS));
#elif defined(OS_MACOSX)
  scoped_ptr<crypto::AppleKeychain> keychain(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          os_crypt::switches::kUseMockKeychain)
          ? new crypto::MockAppleKeychain()
          : new crypto::AppleKeychain());
  ps = new PasswordStoreProxyMac(main_thread_runner, std::move(keychain),
                                 std::move(login_db), profile->GetPrefs());
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // For now, we use PasswordStoreDefault. We might want to make a native
  // backend for PasswordStoreX (see below) in the future though.
  ps = new password_manager::PasswordStoreDefault(
      main_thread_runner, db_thread_runner, std::move(login_db));
#elif defined(USE_X11)
  // On POSIX systems, we try to use the "native" password management system of
  // the desktop environment currently running, allowing GNOME Keyring in XFCE.
  // (In all cases we fall back on the basic store in case of failure.)
  base::nix::DesktopEnvironment desktop_env = GetDesktopEnvironment();
  base::nix::DesktopEnvironment used_desktop_env;
  std::string store_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPasswordStore);
  LinuxBackendUsed used_backend = PLAINTEXT;
  if (store_type == "kwallet") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE4;
  } else if (store_type == "kwallet5") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE5;
  } else if (store_type == "gnome") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_GNOME;
  } else if (store_type == "basic") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_OTHER;
  } else {
    // Detect the store to use automatically.
    used_desktop_env = desktop_env;
    const char* name = base::nix::GetDesktopEnvironmentName(desktop_env);
    VLOG(1) << "Password storage detected desktop environment: "
            << (name ? name : "(unknown)");
  }

  PrefService* prefs = profile->GetPrefs();
  LocalProfileId id = GetLocalProfileId(prefs);

  scoped_ptr<PasswordStoreX::NativeBackend> backend;
  if (used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    // KDE3 didn't use DBus, which our KWallet store uses.
    VLOG(1) << "Trying KWallet for password storage.";
    backend.reset(new NativeBackendKWallet(id, used_desktop_env));
    if (backend->Init()) {
      VLOG(1) << "Using KWallet for password storage.";
      used_backend = KWALLET;
    } else {
      backend.reset();
    }
  } else if (used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
             used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
             used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
#if defined(USE_LIBSECRET)
    VLOG(1) << "Trying libsecret for password storage.";
    backend.reset(new NativeBackendLibsecret(id));
    if (backend->Init()) {
      VLOG(1) << "Using libsecret keyring for password storage.";
      used_backend = LIBSECRET;
    } else {
      backend.reset();
    }
#endif  // defined(USE_LIBSECRET)
    if (!backend.get()) {
#if defined(USE_GNOME_KEYRING)
      VLOG(1) << "Trying GNOME keyring for password storage.";
      backend.reset(new NativeBackendGnome(id));
      if (backend->Init()) {
        VLOG(1) << "Using GNOME keyring for password storage.";
        used_backend = GNOME_KEYRING;
      } else {
        backend.reset();
      }
#endif  // defined(USE_GNOME_KEYRING)
    }
  }

  if (!backend.get()) {
    LOG(WARNING) << "Using basic (unencrypted) store for password storage. "
        "See "
        "https://chromium.googlesource.com/chromium/src/+/master/docs/linux_password_storage.md"
        " for more information about password storage options.";
  }

  ps = new PasswordStoreX(main_thread_runner, db_thread_runner,
                          std::move(login_db), backend.release());
  RecordBackendStatistics(desktop_env, store_type, used_backend);
#elif defined(USE_OZONE)
  ps = new password_manager::PasswordStoreDefault(
      main_thread_runner, db_thread_runner, std::move(login_db));
#else
  NOTIMPLEMENTED();
#endif
  DCHECK(ps);
  if (!ps->Init(
          sync_start_util::GetFlareForSyncableService(profile->GetPath()))) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }
  return ps;
}

void PasswordStoreFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if !defined(OS_CHROMEOS) && defined(USE_X11)
  // Notice that the preprocessor conditions above are exactly those that will
  // result in using PasswordStoreX in BuildServiceInstanceFor().
  registry->RegisterIntegerPref(password_manager::prefs::kLocalProfileId,
                                kInvalidLocalProfileId);
#endif
}

content::BrowserContext* PasswordStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

#if defined(USE_X11)
base::nix::DesktopEnvironment PasswordStoreFactory::GetDesktopEnvironment() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  return base::nix::GetDesktopEnvironment(env.get());
}

void PasswordStoreFactory::RecordBackendStatistics(
    base::nix::DesktopEnvironment desktop_env,
    const std::string& command_line_flag,
    LinuxBackendUsed used_backend) {
  LinuxBackendUsage usage = OTHER_PLAINTEXT;
  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    if (command_line_flag == "kwallet") {
      usage = used_backend == KWALLET ? KDE_KWALLETFLAG_KWALLET
                                      : KDE_KWALLETFLAG_PLAINTEXT;
    } else if (command_line_flag == "gnome") {
      usage = used_backend == PLAINTEXT
                  ? KDE_GNOMEFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? KDE_GNOMEFLAG_KEYRING
                                                   : KDE_GNOMEFLAG_LIBSECRET);
    } else if (command_line_flag == "basic") {
      usage = KDE_BASICFLAG_PLAINTEXT;
    } else {
      usage =
          used_backend == KWALLET ? KDE_NOFLAG_KWALLET : KDE_NOFLAG_PLAINTEXT;
    }
  } else if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
    if (command_line_flag == "kwallet") {
      usage = used_backend == KWALLET ? GNOME_KWALLETFLAG_KWALLET
                                      : GNOME_KWALLETFLAG_PLAINTEXT;
    } else if (command_line_flag == "gnome") {
      usage = used_backend == PLAINTEXT
                  ? GNOME_GNOMEFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? GNOME_GNOMEFLAG_KEYRING
                                                   : GNOME_GNOMEFLAG_LIBSECRET);
    } else if (command_line_flag == "basic") {
      usage = GNOME_BASICFLAG_PLAINTEXT;
    } else {
      usage = used_backend == PLAINTEXT
                  ? GNOME_NOFLAG_PLAINTEXT
                  : (used_backend == GNOME_KEYRING ? GNOME_NOFLAG_KEYRING
                                                   : GNOME_NOFLAG_LIBSECRET);
    }
  } else {
    // It is neither Gnome nor KDE environment.
    switch (used_backend) {
      case PLAINTEXT:
        usage = OTHER_PLAINTEXT;
        break;
      case KWALLET:
        usage = OTHER_KWALLET;
        break;
      case GNOME_KEYRING:
        usage = OTHER_KEYRING;
        break;
      case LIBSECRET:
        usage = OTHER_LIBSECRET;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LinuxBackendStatistics", usage,
                            MAX_BACKEND_USAGE_VALUE);
}
#endif
