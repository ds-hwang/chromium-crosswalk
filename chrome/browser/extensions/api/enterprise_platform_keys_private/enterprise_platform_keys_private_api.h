// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dkrahn): Clean up this private API once all clients have been migrated
// to use the public API. crbug.com/588339.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "extensions/browser/extension_function.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class PrefService;
class Profile;

namespace chromeos {
class CryptohomeClient;
}

namespace cryptohome {
class AsyncMethodCaller;
}

namespace policy {
class EnterpriseInstallAttributes;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

// A callback for challenge key operations. If the operation succeeded,
// |success| is true and |data| is the challenge response. Otherwise, |success|
// is false and |data| is an error message.
using ChallengeKeyCallback =
    base::Callback<void(bool success, const std::string& data)>;

class EPKPChallengeKeyBase {
 public:
  static const char kChallengeBadBase64Error[];
  static const char kDevicePolicyDisabledError[];
  static const char kExtensionNotWhitelistedError[];
  static const char kResponseBadBase64Error[];
  static const char kSignChallengeFailedError[];
  static const char kUserNotManaged[];

 protected:
  enum PrepareKeyResult {
    PREPARE_KEY_OK = 0,
    PREPARE_KEY_DBUS_ERROR,
    PREPARE_KEY_USER_REJECTED,
    PREPARE_KEY_GET_CERTIFICATE_FAILED,
    PREPARE_KEY_RESET_REQUIRED
  };

  EPKPChallengeKeyBase();
  EPKPChallengeKeyBase(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);
  virtual ~EPKPChallengeKeyBase();

  // Returns a trusted value from CroSettings indicating if the device
  // attestation is enabled.
  void GetDeviceAttestationEnabled(
      const base::Callback<void(bool)>& callback) const;

  // Returns true if the device is enterprise managed.
  bool IsEnterpriseDevice() const;

  // Returns true if the extension is white-listed in the user policy.
  bool IsExtensionWhitelisted() const;

  // Returns true if the user is enterprise managed.
  bool IsUserManaged() const;

  // Returns the enterprise domain the device is enrolled to.
  std::string GetEnterpriseDomain() const;

  // Returns the user email.
  std::string GetUserEmail() const;

  // Returns the enterprise virtual device ID.
  std::string GetDeviceId() const;

  // Prepares the key for signing. It will first check if the key exists. If
  // the key does not exist, it will call AttestationFlow::GetCertificate() to
  // get a new one. If require_user_consent is true, it will explicitly ask for
  // user consent before calling GetCertificate().
  void PrepareKey(
      chromeos::attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      chromeos::attestation::AttestationCertificateProfile certificate_profile,
      bool require_user_consent,
      const base::Callback<void(PrepareKeyResult)>& callback);

  chromeos::CryptohomeClient* cryptohome_client_;
  cryptohome::AsyncMethodCaller* async_caller_;
  chromeos::attestation::AttestationFlow* attestation_flow_;
  scoped_ptr<chromeos::attestation::AttestationFlow> default_attestation_flow_;
  ChallengeKeyCallback callback_;
  Profile* profile_;
  std::string extension_id_;

 private:
  // Holds the context of a PrepareKey() operation.
  struct PrepareKeyContext {
    PrepareKeyContext(
        chromeos::attestation::AttestationKeyType key_type,
        const std::string& user_id,
        const std::string& key_name,
        chromeos::attestation::AttestationCertificateProfile
            certificate_profile,
        bool require_user_consent,
        const base::Callback<void(PrepareKeyResult)>& callback);
    ~PrepareKeyContext();

    chromeos::attestation::AttestationKeyType key_type;
    const std::string user_id;
    const std::string key_name;
    chromeos::attestation::AttestationCertificateProfile certificate_profile;
    bool require_user_consent;
    const base::Callback<void(PrepareKeyResult)> callback;
  };

  void IsAttestationPreparedCallback(
      const PrepareKeyContext& context,
      chromeos::DBusMethodCallStatus status,
      bool result);
  void DoesKeyExistCallback(
      const PrepareKeyContext& context,
      chromeos::DBusMethodCallStatus status,
      bool result);
  void AskForUserConsent(const base::Callback<void(bool)>& callback) const;
  void AskForUserConsentCallback(
      const PrepareKeyContext& context,
      bool result);
  void GetCertificateCallback(
      const base::Callback<void(PrepareKeyResult)>& callback,
      bool success,
      const std::string& pem_certificate_chain);

  policy::EnterpriseInstallAttributes* install_attributes_;
};

class EPKPChallengeMachineKey : public EPKPChallengeKeyBase {
 public:
  static const char kGetCertificateFailedError[];
  static const char kNonEnterpriseDeviceError[];

  EPKPChallengeMachineKey();
  EPKPChallengeMachineKey(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);
  ~EPKPChallengeMachineKey() override;

  // Asynchronously run the flow to challenge a machine key in the |caller|
  // context.
  void Run(scoped_refptr<UIThreadExtensionFunction> caller,
           const ChallengeKeyCallback& callback,
           const std::string& encoded_challenge);

  // Like |Run| but expects a Base64 |encoded_challenge|.
  void DecodeAndRun(scoped_refptr<UIThreadExtensionFunction> caller,
                    const ChallengeKeyCallback& callback,
                    const std::string& encoded_challenge);

 private:
  static const char kKeyName[];

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool enabled);
  void PrepareKeyCallback(const std::string& challenge,
                          PrepareKeyResult result);
  void SignChallengeCallback(bool success, const std::string& response);
};

class EPKPChallengeUserKey : public EPKPChallengeKeyBase {
 public:
  static const char kGetCertificateFailedError[];
  static const char kKeyRegistrationFailedError[];
  static const char kUserPolicyDisabledError[];

  EPKPChallengeUserKey();
  EPKPChallengeUserKey(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);
  ~EPKPChallengeUserKey() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Asynchronously run the flow to challenge a user key in the |caller|
  // context.
  void Run(scoped_refptr<UIThreadExtensionFunction> caller,
           const ChallengeKeyCallback& callback,
           const std::string& challenge,
           bool register_key);

  // Like |Run| but expects a Base64 |encoded_challenge|.
  void DecodeAndRun(scoped_refptr<UIThreadExtensionFunction> caller,
                    const ChallengeKeyCallback& callback,
                    const std::string& encoded_challenge,
                    bool register_key);

 private:
  static const char kKeyName[];

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool register_key,
                                           bool require_user_consent,
                                           bool enabled);
  void PrepareKeyCallback(const std::string& challenge,
                          bool register_key,
                          PrepareKeyResult result);
  void SignChallengeCallback(bool register_key,
                             bool success,
                             const std::string& response);
  void RegisterKeyCallback(const std::string& response,
                           bool success,
                           cryptohome::MountError return_code);

  bool IsRemoteAttestationEnabledForUser() const;
};

class EnterprisePlatformKeysPrivateChallengeMachineKeyFunction
    : public UIThreadExtensionFunction {
 public:
  EnterprisePlatformKeysPrivateChallengeMachineKeyFunction();
  explicit EnterprisePlatformKeysPrivateChallengeMachineKeyFunction(
      EPKPChallengeMachineKey* impl_for_testing);

 private:
  ~EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete. If the operation succeeded
  // |success| will be true and |data| will contain the challenge response data.
  // Otherwise |success| will be false and |data| is an error message.
  void OnChallengedKey(bool success, const std::string& data);

  scoped_ptr<EPKPChallengeMachineKey> default_impl_;
  EPKPChallengeMachineKey* impl_;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeMachineKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEMACHINEKEY);
};

class EnterprisePlatformKeysPrivateChallengeUserKeyFunction
    : public UIThreadExtensionFunction {
 public:
  EnterprisePlatformKeysPrivateChallengeUserKeyFunction();
  explicit EnterprisePlatformKeysPrivateChallengeUserKeyFunction(
      EPKPChallengeUserKey* impl_for_testing);

 private:
  ~EnterprisePlatformKeysPrivateChallengeUserKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete. If the operation succeeded
  // |success| will be true and |data| will contain the challenge response data.
  // Otherwise |success| will be false and |data| is an error message.
  void OnChallengedKey(bool success, const std::string& data);

  scoped_ptr<EPKPChallengeUserKey> default_impl_;
  EPKPChallengeUserKey* impl_;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeUserKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEUSERKEY);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
