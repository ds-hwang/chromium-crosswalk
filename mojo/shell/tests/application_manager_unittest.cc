// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_manager.h"

#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/package_manager/package_manager.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "mojo/shell/tests/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {

const char kTestURLString[] = "test:testService";
const char kTestAURLString[] = "test:TestA";
const char kTestBURLString[] = "test:TestB";

struct TestContext {
  TestContext() : num_impls(0), num_loader_deletes(0) {}
  std::string last_test_string;
  int num_impls;
  int num_loader_deletes;
};

void QuitClosure(const Identity& expected,
                 bool* value,
                 const Identity& actual) {
  if (expected == actual) {
    *value = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

class TestServiceImpl : public TestService {
 public:
  TestServiceImpl(TestContext* context, InterfaceRequest<TestService> request)
      : context_(context), binding_(this, std::move(request)) {
    ++context_->num_impls;
  }

  ~TestServiceImpl() override {
    --context_->num_impls;
    if (!base::MessageLoop::current()->is_running())
      return;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  // TestService implementation:
  void Test(const String& test_string,
            const Callback<void()>& callback) override {
    context_->last_test_string = test_string;
    callback.Run();
  }

 private:
  TestContext* context_;
  StrongBinding<TestService> binding_;
};

class TestClient {
 public:
  explicit TestClient(TestServicePtr service)
      : service_(std::move(service)), quit_after_ack_(false) {}

  void AckTest() {
    if (quit_after_ack_)
      base::MessageLoop::current()->QuitWhenIdle();
  }

  void Test(const std::string& test_string) {
    quit_after_ack_ = true;
    service_->Test(test_string,
                   base::Bind(&TestClient::AckTest, base::Unretained(this)));
  }

 private:
  TestServicePtr service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

class TestApplicationLoader : public ApplicationLoader,
                              public ShellClient,
                              public InterfaceFactory<TestService> {
 public:
  explicit TestApplicationLoader(TestContext* context)
      : context_(context), num_loads_(0) {}

  ~TestApplicationLoader() override {
    ++context_->num_loader_deletes;
    shell_connection_.reset();
  }

  int num_loads() const { return num_loads_; }
  const GURL& last_requestor_url() const { return last_requestor_url_; }

 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override {
    ++num_loads_;
    shell_connection_.reset(new ShellConnection(this, std::move(request)));
  }

  // mojo::ShellClient implementation.
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<TestService>(this);
    last_requestor_url_ = GURL(connection->GetRemoteApplicationURL());
    return true;
  }

  // InterfaceFactory<TestService> implementation.
  void Create(Connection* connection,
              InterfaceRequest<TestService> request) override {
    new TestServiceImpl(context_, std::move(request));
  }

  scoped_ptr<ShellConnection> shell_connection_;
  TestContext* context_;
  int num_loads_;
  GURL last_requestor_url_;

  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

class ClosingApplicationLoader : public ApplicationLoader {
 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override {
  }
};

class TesterContext {
 public:
  explicit TesterContext(base::MessageLoop* loop)
      : num_b_calls_(0),
        num_c_calls_(0),
        num_a_deletes_(0),
        num_b_deletes_(0),
        num_c_deletes_(0),
        tester_called_quit_(false),
        a_called_quit_(false),
        loop_(loop) {}

  void IncrementNumBCalls() {
    base::AutoLock lock(lock_);
    num_b_calls_++;
  }

  void IncrementNumCCalls() {
    base::AutoLock lock(lock_);
    num_c_calls_++;
  }

  void IncrementNumADeletes() {
    base::AutoLock lock(lock_);
    num_a_deletes_++;
  }

  void IncrementNumBDeletes() {
    base::AutoLock lock(lock_);
    num_b_deletes_++;
  }

  void IncrementNumCDeletes() {
    base::AutoLock lock(lock_);
    num_c_deletes_++;
  }

  void set_tester_called_quit() {
    base::AutoLock lock(lock_);
    tester_called_quit_ = true;
  }

  void set_a_called_quit() {
    base::AutoLock lock(lock_);
    a_called_quit_ = true;
  }

  int num_b_calls() {
    base::AutoLock lock(lock_);
    return num_b_calls_;
  }
  int num_c_calls() {
    base::AutoLock lock(lock_);
    return num_c_calls_;
  }
  int num_a_deletes() {
    base::AutoLock lock(lock_);
    return num_a_deletes_;
  }
  int num_b_deletes() {
    base::AutoLock lock(lock_);
    return num_b_deletes_;
  }
  int num_c_deletes() {
    base::AutoLock lock(lock_);
    return num_c_deletes_;
  }
  bool tester_called_quit() {
    base::AutoLock lock(lock_);
    return tester_called_quit_;
  }
  bool a_called_quit() {
    base::AutoLock lock(lock_);
    return a_called_quit_;
  }

  void QuitSoon() {
    loop_->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

 private:
  // lock_ protects all members except for loop_ which must be unchanged for the
  // lifetime of this class.
  base::Lock lock_;
  int num_b_calls_;
  int num_c_calls_;
  int num_a_deletes_;
  int num_b_deletes_;
  int num_c_deletes_;
  bool tester_called_quit_;
  bool a_called_quit_;

  base::MessageLoop* loop_;
};

// Used to test that the requestor url will be correctly passed.
class TestAImpl : public TestA {
 public:
  TestAImpl(Connector* connector,
            TesterContext* test_context,
            InterfaceRequest<TestA> request,
            InterfaceFactory<TestC>* factory)
      : test_context_(test_context), binding_(this, std::move(request)) {
    connection_ = connector->Connect(kTestBURLString);
    connection_->AddInterface<TestC>(factory);
    connection_->GetInterface(&b_);
  }

  ~TestAImpl() override {
    test_context_->IncrementNumADeletes();
    if (base::MessageLoop::current()->is_running())
      Quit();
  }

 private:
  void CallB() override {
    b_->B(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void CallCFromB() override {
    b_->CallC(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void Quit() {
    base::MessageLoop::current()->QuitWhenIdle();
    test_context_->set_a_called_quit();
    test_context_->QuitSoon();
  }

  scoped_ptr<Connection> connection_;
  TesterContext* test_context_;
  TestBPtr b_;
  StrongBinding<TestA> binding_;
};

class TestBImpl : public TestB {
 public:
  TestBImpl(Connection* connection,
            TesterContext* test_context,
            InterfaceRequest<TestB> request)
      : test_context_(test_context), binding_(this, std::move(request)) {
    connection->GetInterface(&c_);
  }

  ~TestBImpl() override {
    test_context_->IncrementNumBDeletes();
    if (base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->QuitWhenIdle();
    test_context_->QuitSoon();
  }

 private:
  void B(const Callback<void()>& callback) override {
    test_context_->IncrementNumBCalls();
    callback.Run();
  }

  void CallC(const Callback<void()>& callback) override {
    test_context_->IncrementNumBCalls();
    c_->C(callback);
  }

  TesterContext* test_context_;
  TestCPtr c_;
  StrongBinding<TestB> binding_;
};

class TestCImpl : public TestC {
 public:
  TestCImpl(Connection* connection,
            TesterContext* test_context,
            InterfaceRequest<TestC> request)
      : test_context_(test_context), binding_(this, std::move(request)) {}

  ~TestCImpl() override { test_context_->IncrementNumCDeletes(); }

 private:
  void C(const Callback<void()>& callback) override {
    test_context_->IncrementNumCCalls();
    callback.Run();
  }

  TesterContext* test_context_;
  StrongBinding<TestC> binding_;
};

class Tester : public ShellClient,
               public ApplicationLoader,
               public InterfaceFactory<TestA>,
               public InterfaceFactory<TestB>,
               public InterfaceFactory<TestC> {
 public:
  Tester(TesterContext* context, const std::string& requestor_url)
      : context_(context), requestor_url_(requestor_url) {}
  ~Tester() override {}

 private:
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override {
    app_.reset(new ShellConnection(this, std::move(request)));
  }

  bool AcceptConnection(Connection* connection) override {
    if (!requestor_url_.empty() &&
        requestor_url_ != connection->GetRemoteApplicationURL()) {
      context_->set_tester_called_quit();
      context_->QuitSoon();
      base::MessageLoop::current()->QuitWhenIdle();
      return false;
    }
    // If we're coming from A, then add B, otherwise A.
    if (connection->GetRemoteApplicationURL() == kTestAURLString)
      connection->AddInterface<TestB>(this);
    else
      connection->AddInterface<TestA>(this);
    return true;
  }

  void Create(Connection* connection,
              InterfaceRequest<TestA> request) override {
    a_bindings_.push_back(
        new TestAImpl(app_->connector(), context_, std::move(request), this));
  }

  void Create(Connection* connection,
              InterfaceRequest<TestB> request) override {
    new TestBImpl(connection, context_, std::move(request));
  }

  void Create(Connection* connection,
              InterfaceRequest<TestC> request) override {
    new TestCImpl(connection, context_, std::move(request));
  }

  TesterContext* context_;
  scoped_ptr<ShellConnection> app_;
  std::string requestor_url_;
  ScopedVector<TestAImpl> a_bindings_;
};

void OnConnect(base::RunLoop* loop, uint32_t instance_id) {
  loop->Quit();
}

class ApplicationManagerTest : public testing::Test {
 public:
  ApplicationManagerTest() : tester_context_(&loop_) {}

  ~ApplicationManagerTest() override {}

  void SetUp() override {
    application_manager_.reset(
        new ApplicationManager(nullptr, nullptr, true, nullptr));
    test_loader_ = new TestApplicationLoader(&context_);
    application_manager_->set_default_loader(
        scoped_ptr<ApplicationLoader>(test_loader_));

    TestServicePtr service_proxy;
    ConnectToInterface(GURL(kTestURLString), &service_proxy);
    test_client_.reset(new TestClient(std::move(service_proxy)));
  }

  void TearDown() override {
    test_client_.reset();
    application_manager_.reset();
  }

  void AddLoaderForURL(const GURL& url, const std::string& requestor_url) {
    application_manager_->SetLoaderForURL(
        make_scoped_ptr(new Tester(&tester_context_, requestor_url)), url);
  }

  bool HasRunningInstanceForURL(const GURL& url) {
    ApplicationManager::TestAPI manager_test_api(application_manager_.get());
    return manager_test_api.HasRunningInstanceForURL(url);
  }

 protected:
  template <typename Interface>
  void ConnectToInterface(const GURL& url, InterfacePtr<Interface>* ptr) {
    base::RunLoop loop;
    mojom::InterfaceProviderPtr remote_interfaces;
    scoped_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(CreateShellIdentity());
    params->set_target(Identity(url));
    params->set_remote_interfaces(GetProxy(&remote_interfaces));
    params->set_connect_callback(
        base::Bind(&OnConnect, base::Unretained(&loop)));
    application_manager_->Connect(std::move(params));
    loop.Run();

    mojo::GetInterface(remote_interfaces.get(), ptr);
  }

  base::ShadowingAtExitManager at_exit_;
  TestApplicationLoader* test_loader_;
  TesterContext tester_context_;
  TestContext context_;
  base::MessageLoop loop_;
  scoped_ptr<TestClient> test_client_;
  scoped_ptr<ApplicationManager> application_manager_;
  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerTest);
};

TEST_F(ApplicationManagerTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(ApplicationManagerTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasRunningInstanceForURL(GURL(kTestURLString)));
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset();
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_TRUE(HasRunningInstanceForURL(GURL(kTestURLString)));
}

TEST_F(ApplicationManagerTest, Deletes) {
  {
    ApplicationManager am(nullptr, nullptr, true, nullptr);
    TestApplicationLoader* default_loader =
        new TestApplicationLoader(&context_);
    TestApplicationLoader* url_loader1 = new TestApplicationLoader(&context_);
    TestApplicationLoader* url_loader2 = new TestApplicationLoader(&context_);
    am.set_default_loader(scoped_ptr<ApplicationLoader>(default_loader));
    am.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader1),
                       GURL("test:test1"));
    am.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader2),
                       GURL("test:test1"));
  }
  EXPECT_EQ(3, context_.num_loader_deletes);
}

// Test for SetLoaderForURL() & set_default_loader().
TEST_F(ApplicationManagerTest, SetLoaders) {
  TestApplicationLoader* default_loader = new TestApplicationLoader(&context_);
  TestApplicationLoader* url_loader = new TestApplicationLoader(&context_);
  application_manager_->set_default_loader(
      scoped_ptr<ApplicationLoader>(default_loader));
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(url_loader), GURL("test:test1"));

  // test::test1 should go to url_loader.
  TestServicePtr test_service;
  ConnectToInterface(GURL("test:test1"), &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // http::test1 should go to default loader.
  ConnectToInterface(GURL("http:test1"), &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, default_loader->num_loads());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads.
// TODO(beng): these tests are disabled due to the new async connect flow.
//             they should be re-written as shell apptests.
TEST_F(ApplicationManagerTest, DISABLED_ACallB) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only a can load b.
  AddLoaderForURL(GURL(kTestBURLString), kTestAURLString);

  TestAPtr a;
  ConnectToInterface(GURL(kTestAURLString), &a);
  a->CallB();
  loop_.Run();
  EXPECT_EQ(1, tester_context_.num_b_calls());
  EXPECT_TRUE(tester_context_.a_called_quit());
}

// A calls B which calls C.
TEST_F(ApplicationManagerTest, DISABLED_BCallC) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only a can load b.
  AddLoaderForURL(GURL(kTestBURLString), kTestAURLString);

  TestAPtr a;
  ConnectToInterface(GURL(kTestAURLString), &a);
  a->CallCFromB();
  loop_.Run();

  EXPECT_EQ(1, tester_context_.num_b_calls());
  EXPECT_EQ(1, tester_context_.num_c_calls());
  EXPECT_TRUE(tester_context_.a_called_quit());
}

// Confirm that a service impl will be deleted if the app that connected to
// it goes away.
TEST_F(ApplicationManagerTest, DISABLED_BDeleted) {
  AddLoaderForURL(GURL(kTestAURLString), std::string());
  AddLoaderForURL(GURL(kTestBURLString), std::string());

  TestAPtr a;
  ConnectToInterface(GURL(kTestAURLString), &a);

  a->CallB();
  loop_.Run();

  // Kills the a app.
  application_manager_->SetLoaderForURL(scoped_ptr<ApplicationLoader>(),
                                        GURL(kTestAURLString));
  loop_.Run();

  EXPECT_EQ(1, tester_context_.num_b_deletes());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads, and that it can be rejected.
TEST_F(ApplicationManagerTest, DISABLED_ANoLoadB) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only c can load b, so this will fail.
  AddLoaderForURL(GURL(kTestBURLString), "test:TestC");

  TestAPtr a;
  ConnectToInterface(GURL(kTestAURLString), &a);
  a->CallB();
  loop_.Run();
  EXPECT_EQ(0, tester_context_.num_b_calls());

  EXPECT_FALSE(tester_context_.a_called_quit());
  EXPECT_TRUE(tester_context_.tester_called_quit());
}

TEST_F(ApplicationManagerTest, NoServiceNoLoad) {
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // There is no TestC service implementation registered with
  // ApplicationManager, so this cannot succeed (but also shouldn't crash).
  TestCPtr c;
  ConnectToInterface(GURL(kTestAURLString), &c);
  c.set_connection_error_handler(
      []() { base::MessageLoop::current()->QuitWhenIdle(); });

  loop_.Run();
  EXPECT_TRUE(c.encountered_error());
}

TEST_F(ApplicationManagerTest, TestEndApplicationClosure) {
  ClosingApplicationLoader* loader = new ClosingApplicationLoader();
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(loader), GURL("test:test"));

  bool called = false;
  scoped_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(CreateShellIdentity());
  params->set_target(
      Identity(GURL("test:test"), "", mojom::Connector::kUserRoot));
  application_manager_->SetInstanceQuitCallback(
      base::Bind(&QuitClosure, params->target(), &called));
  application_manager_->Connect(std::move(params));
  loop_.Run();
  EXPECT_TRUE(called);
}

TEST_F(ApplicationManagerTest, SameIdentityShouldNotCauseDuplicateLoad) {
  // 1 because ApplicationManagerTest connects once at startup.
  EXPECT_EQ(1, test_loader_->num_loads());

  TestServicePtr test_service;
  ConnectToInterface(GURL("test:foo"), &test_service);
  EXPECT_EQ(2, test_loader_->num_loads());

  // Exactly the same URL as above.
  ConnectToInterface(GURL("test:foo"), &test_service);
  EXPECT_EQ(2, test_loader_->num_loads());

  // A different identity because the domain is different.
  ConnectToInterface(GURL("test:bar"), &test_service);
  EXPECT_EQ(3, test_loader_->num_loads());
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
