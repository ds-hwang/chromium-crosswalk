// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_TEST_MULTIPROCESS_TEST_HELPER_H_
#define MOJO_EDK_TEST_MULTIPROCESS_TEST_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/multiprocess_func_list.h"

namespace mojo {

namespace edk {
class PlatformChannelPair;

namespace test {

class MultiprocessTestHelper {
 public:
  using HandlerCallback = base::Callback<void(ScopedMessagePipeHandle)>;

  MultiprocessTestHelper();
  ~MultiprocessTestHelper();

  // Start a child process and run the "main" function "named" |test_child_name|
  // declared using |MOJO_MULTIPROCESS_TEST_CHILD_MAIN()| or
  // |MOJO_MULTIPROCESS_TEST_CHILD_TEST()| (below).
  ScopedMessagePipeHandle StartChild(const std::string& test_child_name);

  // Like |StartChild()|, but appends an extra switch (with ASCII value) to the
  // command line. (The switch must not already be present in the default
  // command line.)
  ScopedMessagePipeHandle StartChildWithExtraSwitch(
      const std::string& test_child_name,
      const std::string& switch_string,
      const std::string& switch_value);

  // Wait for the child process to terminate.
  // Returns the exit code of the child process. Note that, though it's declared
  // to be an |int|, the exit code is subject to mangling by the OS. E.g., we
  // usually return -1 on error in the child (e.g., if |test_child_name| was not
  // found), but this is mangled to 255 on Linux. You should only rely on codes
  // 0-127 being preserved, and -1 being outside the range 0-127.
  int WaitForChildShutdown();

  // Like |WaitForChildShutdown()|, but returns true on success (exit code of 0)
  // and false otherwise. You probably want to do something like
  // |EXPECT_TRUE(WaitForChildTestShutdown());|.
  bool WaitForChildTestShutdown();

  // Used by macros in mojo/edk/test/mojo_test_base.h to support multiprocess
  // test client initialization.
  static void ChildSetup();
  static int RunClientMain(const base::Callback<int(MojoHandle)>& main);
  static int RunClientTestMain(const base::Callback<void(MojoHandle)>& main);

  // For use (and only valid) in the child process:
  static std::string primordial_pipe_token;

 private:
  // Valid after |StartChild()| and before |WaitForChildShutdown()|.
  base::Process test_child_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MultiprocessTestHelper);
};

}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_TEST_MULTIPROCESS_TEST_HELPER_H_
