// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_mojo_app.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"

namespace content {

const char kTestMojoAppUrl[] = "system:content_mojo_test";

TestMojoApp::TestMojoApp() : service_binding_(this) {
}

TestMojoApp::~TestMojoApp() {
}

bool TestMojoApp::AcceptConnection(mojo::Connection* connection) {
  requestor_url_ = GURL(connection->GetRemoteApplicationURL());
  connection->AddInterface<TestMojoService>(this);
  return true;
}

void TestMojoApp::Create(mojo::Connection* connection,
                         mojo::InterfaceRequest<TestMojoService> request) {
  DCHECK(!service_binding_.is_bound());
  service_binding_.Bind(std::move(request));
}

void TestMojoApp::DoSomething(const DoSomethingCallback& callback) {
  callback.Run();
  base::MessageLoop::current()->QuitWhenIdle();
}

void TestMojoApp::GetRequestorURL(const GetRequestorURLCallback& callback) {
  callback.Run(requestor_url_.spec());
}

}  // namespace content
