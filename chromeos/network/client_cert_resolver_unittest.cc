// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <pk11pub.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kWifiStub = "wifi_stub";
const char* kWifiSSID = "wifi_ssid";
const char* kUserProfilePath = "user_profile";
const char* kUserHash = "user_hash";

}  // namespace

class ClientCertResolverTest : public testing::Test,
                               public ClientCertResolver::Observer {
 public:
  ClientCertResolverTest()
      : network_properties_changed_count_(0),
        service_test_(nullptr),
        profile_test_(nullptr),
        cert_loader_(nullptr) {}
  ~ClientCertResolverTest() override {}

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));
    test_nsscertdb_->SetSlowTaskRunnerForTest(message_loop_.task_runner());

    DBusThreadManager::Initialize();
    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    profile_test_ =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    profile_test_->AddProfile(kUserProfilePath, kUserHash);
    base::RunLoop().RunUntilIdle();
    service_test_->ClearServices();
    base::RunLoop().RunUntilIdle();

    CertLoader::Initialize();
    cert_loader_ = CertLoader::Get();
    CertLoader::ForceHardwareBackedForTesting();
  }

  void TearDown() override {
    client_cert_resolver_->RemoveObserver(this);
    client_cert_resolver_.reset();
    test_clock_.reset();
    managed_config_handler_.reset();
    network_config_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  void StartCertLoader() {
    cert_loader_->StartWithNSSDB(test_nsscertdb_.get());
    if (test_client_cert_.get()) {
      int slot_id = 0;
      const std::string pkcs11_id =
          CertLoader::GetPkcs11IdAndSlotForCert(*test_client_cert_, &slot_id);
      test_cert_id_ = base::StringPrintf("%i:%s", slot_id, pkcs11_id.c_str());
    }
  }

  // Imports a client certificate. Its PKCS#11 ID is stored in |test_cert_id_|.
  // If |import_issuer| is true, also imports the CA cert (stored as PEM in
  // test_ca_cert_pem_) that issued the client certificate.
  void SetupTestCerts(bool import_issuer) {
    // Load a CA cert.
    net::CertificateList ca_cert_list = net::CreateCertificateListFromFile(
        net::GetTestCertsDirectory(), "client_1_ca.pem",
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(!ca_cert_list.empty());
    net::X509Certificate::GetPEMEncoded(ca_cert_list[0]->os_cert_handle(),
                                        &test_ca_cert_pem_);
    ASSERT_TRUE(!test_ca_cert_pem_.empty());

    if (import_issuer) {
      net::NSSCertDatabase::ImportCertFailureList failures;
      EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
          ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
      ASSERT_TRUE(failures.empty())
          << net::ErrorToString(failures[0].net_error);
    }

    // Import a client cert signed by that CA.
    test_client_cert_ =
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            "client_1.pem",
                                            "client_1.pk8",
                                            test_nssdb_.slot());
    ASSERT_TRUE(test_client_cert_.get());
  }

  void SetupNetworkHandlers() {
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_profile_handler_.reset(new NetworkProfileHandler());
    network_config_handler_.reset(new NetworkConfigurationHandler());
    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    client_cert_resolver_.reset(new ClientCertResolver());

    test_clock_.reset(new base::SimpleTestClock);
    test_clock_->SetNow(base::Time::Now());
    client_cert_resolver_->SetClockForTesting(test_clock_.get());

    network_profile_handler_->Init();
    network_config_handler_->Init(network_state_handler_.get(),
                                  nullptr /* network_device_handler */);
    managed_config_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        nullptr /* prohibited_technologies_handler */);
    // Run all notifications before starting the cert loader to reduce run time.
    base::RunLoop().RunUntilIdle();

    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_config_handler_.get());
    client_cert_resolver_->AddObserver(this);
    client_cert_resolver_->SetSlowTaskRunnerForTest(
        message_loop_.task_runner());
  }

  void SetupWifi() {
    service_test_->SetServiceProperties(kWifiStub,
                                        kWifiStub,
                                        kWifiSSID,
                                        shill::kTypeWifi,
                                        shill::kStateOnline,
                                        true /* visible */);
    // Set an arbitrary cert id, so that we can check afterwards whether we
    // cleared the property or not.
    service_test_->SetServiceProperty(
        kWifiStub, shill::kEapCertIdProperty, base::StringValue("invalid id"));
    profile_test_->AddService(kUserProfilePath, kWifiStub);

    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->AddManagerService(kWifiStub, true);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // with a certain Issuer CN. It will match the test client cert.
  void SetupPolicyMatchingIssuerCN() {
    const char* kTestPolicy =
        "[ { \"GUID\": \"wifi_stub\","
        "    \"Name\": \"wifi_stub\","
        "    \"Type\": \"WiFi\","
        "    \"WiFi\": {"
        "      \"Security\": \"WPA-EAP\","
        "      \"SSID\": \"wifi_ssid\","
        "      \"EAP\": {"
        "        \"Outer\": \"EAP-TLS\","
        "        \"ClientCertType\": \"Pattern\","
        "        \"ClientCertPattern\": {"
        "          \"Issuer\": {"
        "            \"CommonName\": \"B CA\""
        "          }"
        "        }"
        "      }"
        "    }"
        "} ]";

    std::string error;
    scoped_ptr<base::Value> policy_value = base::JSONReader::ReadAndReturnError(
        kTestPolicy, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = nullptr;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    managed_config_handler_->SetPolicy(
        onc::ONC_SOURCE_USER_POLICY, kUserHash, *policy,
        base::DictionaryValue() /* no global network config */);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // that is signed by the test CA cert (stored in |test_ca_cert_pem_|). In
  // particular it will match the test client cert.
  void SetupPolicyMatchingIssuerPEM() {
    const char* kTestPolicyTemplate =
        "[ { \"GUID\": \"wifi_stub\","
        "    \"Name\": \"wifi_stub\","
        "    \"Type\": \"WiFi\","
        "    \"WiFi\": {"
        "      \"Security\": \"WPA-EAP\","
        "      \"SSID\": \"wifi_ssid\","
        "      \"EAP\": {"
        "        \"Outer\": \"EAP-TLS\","
        "        \"ClientCertType\": \"Pattern\","
        "        \"ClientCertPattern\": {"
        "          \"IssuerCAPEMs\": [ \"%s\" ]"
        "        }"
        "      }"
        "    }"
        "} ]";
    std::string policy_json =
        base::StringPrintf(kTestPolicyTemplate, test_ca_cert_pem_.c_str());

    std::string error;
    scoped_ptr<base::Value> policy_value = base::JSONReader::ReadAndReturnError(
        policy_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = nullptr;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    managed_config_handler_->SetPolicy(
        onc::ONC_SOURCE_USER_POLICY,
        kUserHash,
        *policy,
        base::DictionaryValue() /* no global network config */);
  }

  void SetWifiState(const std::string& state) {
    ASSERT_TRUE(service_test_->SetServiceProperty(
        kWifiStub, shill::kStateProperty, base::StringValue(state)));
  }

  void GetClientCertProperties(std::string* pkcs11_id) {
    pkcs11_id->clear();
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kWifiStub);
    if (!properties)
      return;
    properties->GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                              pkcs11_id);
  }

  int network_properties_changed_count_;
  std::string test_cert_id_;
  scoped_ptr<base::SimpleTestClock> test_clock_;
  scoped_ptr<ClientCertResolver> client_cert_resolver_;

 private:
  // ClientCertResolver::Observer:
  void ResolveRequestCompleted(bool network_properties_changed) override {
    if (network_properties_changed)
      ++network_properties_changed_count_;
  }

  ShillServiceClient::TestInterface* service_test_;
  ShillProfileClient::TestInterface* profile_test_;
  CertLoader* cert_loader_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_config_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl> managed_config_handler_;
  base::MessageLoop message_loop_;
  scoped_refptr<net::X509Certificate> test_client_cert_;
  std::string test_ca_cert_pem_;
  crypto::ScopedTestNSSDB test_nssdb_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertResolverTest);
};

TEST_F(ClientCertResolverTest, NoMatchingCertificates) {
  SetupTestCerts(false /* do not import the issuer */);
  StartCertLoader();
  SetupWifi();
  base::RunLoop().RunUntilIdle();
  network_properties_changed_count_ = 0;
  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM();
  base::RunLoop().RunUntilIdle();

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
}

TEST_F(ClientCertResolverTest, MatchIssuerCNWithoutIssuerInstalled) {
  SetupTestCerts(false /* do not import the issuer */);
  SetupWifi();
  base::RunLoop().RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerCN();
  base::RunLoop().RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartCertLoader();
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveOnCertificatesLoaded) {
  SetupTestCerts(true /* import issuer */);
  SetupWifi();
  base::RunLoop().RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM();
  base::RunLoop().RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartCertLoader();
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveAfterPolicyApplication) {
  SetupTestCerts(true /* import issuer */);
  SetupWifi();
  base::RunLoop().RunUntilIdle();
  StartCertLoader();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Policy application will trigger the ClientCertResolver.
  network_properties_changed_count_ = 0;
  SetupPolicyMatchingIssuerPEM();
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ExpiringCertificate) {
  SetupTestCerts(true /* import issuer */);
  SetupWifi();
  base::RunLoop().RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM();
  base::RunLoop().RunUntilIdle();

  StartCertLoader();
  base::RunLoop().RunUntilIdle();

  SetWifiState(shill::kStateOnline);
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);

  // Verify that, after the certificate expired and the network disconnection
  // happens, no client certificate was configured.
  test_clock_->SetNow(base::Time::Max());
  SetWifiState(shill::kStateOffline);
  base::RunLoop().RunUntilIdle();
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
}

}  // namespace chromeos
