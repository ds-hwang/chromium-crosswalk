// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/router.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

namespace {

class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(const base::WeakPtr<Router>& router)
      : router_(router), accept_was_invoked_(false) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Mojo application handled a message that was expecting a response
      // but did not send a response.
      if (router_) {
        // We raise an error to signal the calling application that an error
        // condition occurred. Without this the calling application would have
        // no way of knowing it should stop waiting for a response.
        router_->RaiseError();
      }
    }
  }

  // MessageReceiver implementation:
  bool Accept(Message* message) override {
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(kMessageIsResponse));

    bool result = false;

    if (router_)
      result = router_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsValid() override {
    return router_ && !router_->encountered_error() && router_->is_valid();
  }

 private:
  base::WeakPtr<Router> router_;
  bool accept_was_invoked_;
};

}  // namespace

// ----------------------------------------------------------------------------

Router::SyncResponseInfo::SyncResponseInfo(bool* in_response_received)
    : response_received(in_response_received) {}

Router::SyncResponseInfo::~SyncResponseInfo() {}

// ----------------------------------------------------------------------------

Router::HandleIncomingMessageThunk::HandleIncomingMessageThunk(Router* router)
    : router_(router) {
}

Router::HandleIncomingMessageThunk::~HandleIncomingMessageThunk() {
}

bool Router::HandleIncomingMessageThunk::Accept(Message* message) {
  return router_->HandleIncomingMessage(message);
}

// ----------------------------------------------------------------------------

Router::Router(ScopedMessagePipeHandle message_pipe,
               FilterChain filters,
               bool expects_sync_requests,
               const MojoAsyncWaiter* waiter)
    : thunk_(this),
      filters_(std::move(filters)),
      connector_(std::move(message_pipe),
                 Connector::SINGLE_THREADED_SEND,
                 waiter),
      incoming_receiver_(nullptr),
      next_request_id_(0),
      testing_mode_(false),
      pending_task_for_messages_(false),
      weak_factory_(this) {
  filters_.SetSink(&thunk_);
  if (expects_sync_requests)
    connector_.RegisterSyncHandleWatch();
  connector_.set_incoming_receiver(filters_.GetHead());
}

Router::~Router() {}

bool Router::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!message->has_flag(kMessageExpectsResponse));
  return connector_.Accept(message);
}

bool Router::AcceptWithResponder(Message* message, MessageReceiver* responder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message->has_flag(kMessageExpectsResponse));

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  message->set_request_id(request_id);
  if (!connector_.Accept(message))
    return false;

  if (!message->has_flag(kMessageIsSync)) {
    // We assume ownership of |responder|.
    async_responders_[request_id] = make_scoped_ptr(responder);
    return true;
  }

  if (!connector_.RegisterSyncHandleWatch())
    return false;

  bool response_received = false;
  scoped_ptr<MessageReceiver> sync_responder(responder);
  sync_responses_.insert(std::make_pair(
      request_id, make_scoped_ptr(new SyncResponseInfo(&response_received))));

  base::WeakPtr<Router> weak_self = weak_factory_.GetWeakPtr();
  bool result = connector_.RunSyncHandleWatch(&response_received);
  // Make sure that this instance hasn't been destroyed.
  if (weak_self) {
    DCHECK(ContainsKey(sync_responses_, request_id));
    auto iter = sync_responses_.find(request_id);
    DCHECK_EQ(&response_received, iter->second->response_received);
    if (result && response_received) {
      scoped_ptr<Message> response = std::move(iter->second->response);
      ignore_result(sync_responder->Accept(response.get()));
    }
    sync_responses_.erase(iter);

    connector_.UnregisterSyncHandleWatch();
  }

  // Return true means that we take ownership of |responder|.
  return true;
}

void Router::EnableTestingMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool Router::HandleIncomingMessage(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool during_sync_call =
      connector_.during_sync_handle_watcher_callback();
  if (!message->has_flag(kMessageIsSync) &&
      (during_sync_call || !pending_messages_.empty())) {
    scoped_ptr<Message> pending_message(new Message);
    message->MoveTo(pending_message.get());
    pending_messages_.push(std::move(pending_message));

    if (!pending_task_for_messages_) {
      pending_task_for_messages_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&Router::HandleQueuedMessages,
                                weak_factory_.GetWeakPtr()));
    }

    return true;
  }

  return HandleMessageInternal(message);
}

void Router::HandleQueuedMessages() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pending_task_for_messages_);

  base::WeakPtr<Router> weak_self = weak_factory_.GetWeakPtr();
  while (!pending_messages_.empty()) {
    scoped_ptr<Message> message(std::move(pending_messages_.front()));
    pending_messages_.pop();

    bool result = HandleMessageInternal(message.get());
    if (!weak_self)
      return;

    if (!result && !testing_mode_) {
      connector_.RaiseError();
      break;
    }
  }

  pending_task_for_messages_ = false;
}

bool Router::HandleMessageInternal(Message* message) {
  if (message->has_flag(kMessageExpectsResponse)) {
    if (!incoming_receiver_)
      return false;

    MessageReceiverWithStatus* responder =
        new ResponderThunk(weak_factory_.GetWeakPtr());
    bool ok = incoming_receiver_->AcceptWithResponder(message, responder);
    if (!ok)
      delete responder;
    return ok;

  } else if (message->has_flag(kMessageIsResponse)) {
    uint64_t request_id = message->request_id();

    if (message->has_flag(kMessageIsSync)) {
      auto it = sync_responses_.find(request_id);
      if (it == sync_responses_.end()) {
        DCHECK(testing_mode_);
        return false;
      }
      it->second->response.reset(new Message());
      message->MoveTo(it->second->response.get());
      *it->second->response_received = true;
      return true;
    }

    auto it = async_responders_.find(request_id);
    if (it == async_responders_.end()) {
      DCHECK(testing_mode_);
      return false;
    }
    scoped_ptr<MessageReceiver> responder = std::move(it->second);
    async_responders_.erase(it);
    return responder->Accept(message);
  } else {
    if (!incoming_receiver_)
      return false;

    return incoming_receiver_->Accept(message);
  }
}

// ----------------------------------------------------------------------------

}  // namespace internal
}  // namespace mojo
