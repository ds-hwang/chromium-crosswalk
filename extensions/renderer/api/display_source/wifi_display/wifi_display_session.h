// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_H_

#include <string>

#include "extensions/common/mojo/wifi_display_session_service.mojom.h"
#include "extensions/renderer/api/display_source/display_source_session.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace extensions {

class WiFiDisplaySession: public DisplaySourceSession,
                          public WiFiDisplaySessionServiceClient {
 public:
  explicit WiFiDisplaySession(
      const DisplaySourceSessionParams& params);
  ~WiFiDisplaySession() override;

 private:
  using DisplaySourceSession::CompletionCallback;
  // DisplaySourceSession overrides.
  void Start(const CompletionCallback& callback) override;
  void Terminate(const CompletionCallback& callback) override;

  // WiFiDisplaySessionServiceClient overrides.
  void OnConnected(const mojo::String& ip_address) override;
  void OnConnectRequestHandled(bool success,
                               const mojo::String& error) override;
  void OnTerminated() override;
  void OnDisconnectRequestHandled(bool success,
                                  const mojo::String& error) override;
  void OnError(int32_t type, const mojo::String& description) override;
  void OnMessage(const mojo::String& data) override;

  // A connection error handler for the mojo objects used in this class.
  void OnConnectionError();

  void RunStartCallback(bool success, const std::string& error = "");
  void RunTerminateCallback(bool success, const std::string& error = "");

 private:
  WiFiDisplaySessionServicePtr service_;
  mojo::Binding<WiFiDisplaySessionServiceClient> binding_;
  std::string ip_address_;
  DisplaySourceSessionParams params_;
  CompletionCallback start_completion_callback_;
  CompletionCallback teminate_completion_callback_;
  base::WeakPtrFactory<WiFiDisplaySession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplaySession);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_H_
