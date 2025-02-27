// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

namespace {

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset) {
    DCHECK_GT(message_->data_num_bytes(), offset_);
  }

  MessageView(MessageView&& other) { *this = std::move(other); }

  MessageView& operator=(MessageView&& other) {
    message_ = std::move(other.message_);
    offset_ = other.offset_;
    return *this;
  }

  ~MessageView() {}

  const void* data() const {
    return static_cast<const char*>(message_->data()) + offset_;
  }

  size_t data_num_bytes() const { return message_->data_num_bytes() - offset_; }

  size_t data_offset() const { return offset_; }
  void advance_data_offset(size_t num_bytes) {
    DCHECK_GE(message_->data_num_bytes(), offset_ + num_bytes);
    offset_ += num_bytes;
  }

  Channel::MessagePtr TakeChannelMessage() { return std::move(message_); }

 private:
  Channel::MessagePtr message_;
  size_t offset_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelWin : public Channel,
                   public base::MessageLoop::DestructionObserver,
                   public base::MessageLoopForIO::IOHandler {
 public:
  ChannelWin(Delegate* delegate,
             ScopedPlatformHandle handle,
             scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate),
        self_(this),
        handle_(std::move(handle)),
        io_task_runner_(io_task_runner) {
    sentinel_ = ~reinterpret_cast<uintptr_t>(this);
    CHECK(handle_.is_valid());
    memset(&read_context_, 0, sizeof(read_context_));
    read_context_.handler = this;

    memset(&write_context_, 0, sizeof(write_context_));
    write_context_.handler = this;
  }

  void Start() override {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChannelWin::StartOnIOThread, this));
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChannelWin::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;

      bool write_now = !delay_writes_ && outgoing_messages_.empty();
      outgoing_messages_.emplace_back(std::move(message), 0);

      if (write_now && !WriteNoLock(outgoing_messages_.front()))
        reject_writes_ = write_error = true;
    }
    if (write_error) {
      // Do not synchronously invoke OnError(). Write() may have been called by
      // the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ChannelWin::OnError, this));
    }
  }

  ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_handles,
      void** payload,
      size_t* payload_size) override {
    size_t handles_size = sizeof(PlatformHandle) * num_handles;
    if (handles_size > *payload_size)
      return nullptr;

    *payload_size -= handles_size;
    ScopedPlatformHandleVectorPtr handles(
        new PlatformHandleVector(num_handles));
    memcpy(handles->data(),
           static_cast<const char*>(*payload) + *payload_size, handles_size);
    return handles;
  }

 private:
  // May run on any thread.
  ~ChannelWin() override {
    // This is intentionally not 0. If another object is constructed on top of
    // this memory, it is likely to initialise values to 0. Using a non-zero
    // value lets us detect the difference between just destroying, and
    // re-allocating the memory.
    sentinel_ = UINTPTR_MAX;
  }

  void StartOnIOThread() {
    base::MessageLoop::current()->AddDestructionObserver(this);
    base::MessageLoopForIO::current()->RegisterIOHandler(
        handle_.get().handle, this);

    // Now that we have registered our IOHandler, we can start writing.
    {
      base::AutoLock lock(write_lock_);
      if (delay_writes_) {
        delay_writes_ = false;
        WriteNextNoLock();
      }
    }

    // Keep this alive in case we synchronously run shutdown.
    scoped_refptr<ChannelWin> keep_alive(this);
    ReadMore(0);
  }

  void ShutDownOnIOThread() {
    base::MessageLoop::current()->RemoveDestructionObserver(this);

    // BUG(crbug.com/583525): This function is expected to be called once, and
    // |handle_| should be valid at this point.
    CHECK(handle_.is_valid());
    CancelIo(handle_.get().handle);
    handle_.reset();

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    CheckValid();
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessageLoop::IOHandler:
  void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override {
    CheckValid();
    if (error != ERROR_SUCCESS) {
      OnError();
    } else if (context == &read_context_) {
      OnReadDone(static_cast<size_t>(bytes_transfered));
    } else {
      CHECK(context == &write_context_);
      OnWriteDone(static_cast<size_t>(bytes_transfered));
    }
    Release();  // Balancing reference taken after ReadFile / WriteFile.
  }

  void OnReadDone(size_t bytes_read) {
    if (bytes_read > 0) {
      size_t next_read_size = 0;
      if (OnReadComplete(bytes_read, &next_read_size)) {
        ReadMore(next_read_size);
      } else {
        OnError();
      }
    } else if (bytes_read == 0) {
      OnError();
    }
  }

  void OnWriteDone(size_t bytes_written) {
    if (bytes_written == 0)
      return;

    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);

      DCHECK(!outgoing_messages_.empty());

      MessageView& message_view = outgoing_messages_.front();
      message_view.advance_data_offset(bytes_written);
      if (message_view.data_num_bytes() == 0) {
        Channel::MessagePtr message = message_view.TakeChannelMessage();
        outgoing_messages_.pop_front();

        // Clear any handles so they don't get closed on destruction.
        ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
        if (handles)
          handles->clear();
      }

      if (!WriteNextNoLock())
        reject_writes_ = write_error = true;
    }
    if (write_error)
      OnError();
  }

  void ReadMore(size_t next_read_size_hint) {
    size_t buffer_capacity = next_read_size_hint;
    char* buffer = GetReadBuffer(&buffer_capacity);
    DCHECK_GT(buffer_capacity, 0u);

    BOOL ok = ReadFile(handle_.get().handle,
                       buffer,
                       static_cast<DWORD>(buffer_capacity),
                       NULL,
                       &read_context_.overlapped);

    if (ok || GetLastError() == ERROR_IO_PENDING) {
      AddRef();  // Will be balanced in OnIOCompleted
    } else {
      OnError();
    }
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(const MessageView& message_view) {
    BOOL ok = WriteFile(handle_.get().handle,
                        message_view.data(),
                        static_cast<DWORD>(message_view.data_num_bytes()),
                        NULL,
                        &write_context_.overlapped);

    if (ok || GetLastError() == ERROR_IO_PENDING) {
      AddRef();  // Will be balanced in OnIOCompleted.
      return true;
    }
    return false;
  }

  bool WriteNextNoLock() {
    if (outgoing_messages_.empty())
      return true;
    return WriteNoLock(outgoing_messages_.front());
  }

  void CheckValid() const {
    CHECK_EQ(reinterpret_cast<uintptr_t>(this), ~sentinel_);
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  ScopedPlatformHandle handle_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  base::MessageLoopForIO::IOContext read_context_;
  base::MessageLoopForIO::IOContext write_context_;

  // Protects |reject_writes_| and |outgoing_messages_|.
  base::Lock write_lock_;

  bool delay_writes_ = true;

  bool reject_writes_ = false;
  std::deque<MessageView> outgoing_messages_;

  // A value that is unlikely to be valid if this object is destroyed and the
  // memory overwritten by something else. When this is valid, its value will be
  // ~|this|.
  // TODO(amistry): Remove before M50 branch point.
  uintptr_t sentinel_;

  DISALLOW_COPY_AND_ASSIGN(ChannelWin);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelWin(delegate, std::move(platform_handle), io_task_runner);
}

}  // namespace edk
}  // namespace mojo
