// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_channel_host.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_jpeg_decode_accelerator_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "url/gurl.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "content/public/common/sandbox_init.h"
#endif

using base::AutoLock;

namespace content {
namespace {

// Global atomic to generate unique transfer buffer IDs.
base::StaticAtomicSequenceNumber g_next_transfer_buffer_id;

}  // namespace

GpuChannelHost::StreamFlushInfo::StreamFlushInfo()
    : next_stream_flush_id(1),
      flushed_stream_flush_id(0),
      verified_stream_flush_id(0),
      flush_pending(false),
      route_id(MSG_ROUTING_NONE),
      put_offset(0),
      flush_count(0),
      flush_id(0) {}

GpuChannelHost::StreamFlushInfo::StreamFlushInfo(const StreamFlushInfo& other) =
    default;

GpuChannelHost::StreamFlushInfo::~StreamFlushInfo() {}

// static
scoped_refptr<GpuChannelHost> GpuChannelHost::Create(
    GpuChannelHostFactory* factory,
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    const IPC::ChannelHandle& channel_handle,
    base::WaitableEvent* shutdown_event,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  DCHECK(factory->IsMainThread());
  scoped_refptr<GpuChannelHost> host =
      new GpuChannelHost(factory, channel_id, gpu_info,
                         gpu_memory_buffer_manager);
  host->Connect(channel_handle, shutdown_event);
  return host;
}

GpuChannelHost::GpuChannelHost(
    GpuChannelHostFactory* factory,
    int channel_id,
    const gpu::GPUInfo& gpu_info,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : factory_(factory),
      channel_id_(channel_id),
      gpu_info_(gpu_info),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  next_image_id_.GetNext();
  next_route_id_.GetNext();
  next_stream_id_.GetNext();
}

void GpuChannelHost::Connect(const IPC::ChannelHandle& channel_handle,
                             base::WaitableEvent* shutdown_event) {
  DCHECK(factory_->IsMainThread());
  // Open a channel to the GPU process. We pass nullptr as the main listener
  // here since we need to filter everything to route it to the right thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  channel_ = IPC::SyncChannel::Create(channel_handle, IPC::Channel::MODE_CLIENT,
                                      nullptr, io_task_runner.get(), true,
                                      shutdown_event);

  sync_filter_ = channel_->CreateSyncMessageFilter();

  channel_filter_ = new MessageFilter();

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  scoped_ptr<IPC::Message> message(msg);
  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  // Currently we need to choose between two different mechanisms for sending.
  // On the main thread we use the regular channel Send() method, on another
  // thread we use SyncMessageFilter. We also have to be careful interpreting
  // IsMainThread() since it might return false during shutdown,
  // impl we are actually calling from the main thread (discard message then).
  //
  // TODO: Can we just always use sync_filter_ since we setup the channel
  //       without a main listener?
  if (factory_->IsMainThread()) {
    // channel_ is only modified on the main thread, so we don't need to take a
    // lock here.
    if (!channel_) {
      DVLOG(1) << "GpuChannelHost::Send failed: Channel already destroyed";
      return false;
    }
    // http://crbug.com/125264
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    bool result = channel_->Send(message.release());
    if (!result)
      DVLOG(1) << "GpuChannelHost::Send failed: Channel::Send failed";
    return result;
  }

  bool result = sync_filter_->Send(message.release());
  return result;
}

uint32_t GpuChannelHost::OrderingBarrier(
    int32_t route_id,
    int32_t stream_id,
    int32_t put_offset,
    uint32_t flush_count,
    const std::vector<ui::LatencyInfo>& latency_info,
    bool put_offset_changed,
    bool do_flush) {
  AutoLock lock(context_lock_);
  StreamFlushInfo& flush_info = stream_flush_info_[stream_id];
  if (flush_info.flush_pending && flush_info.route_id != route_id)
    InternalFlush(&flush_info);

  if (put_offset_changed) {
    const uint32_t flush_id = flush_info.next_stream_flush_id++;
    flush_info.flush_pending = true;
    flush_info.route_id = route_id;
    flush_info.put_offset = put_offset;
    flush_info.flush_count = flush_count;
    flush_info.flush_id = flush_id;
    flush_info.latency_info.insert(flush_info.latency_info.end(),
                                   latency_info.begin(), latency_info.end());

    if (do_flush)
      InternalFlush(&flush_info);

    return flush_id;
  }
  return 0;
}

void GpuChannelHost::FlushPendingStream(int32_t stream_id) {
  AutoLock lock(context_lock_);
  auto flush_info_iter = stream_flush_info_.find(stream_id);
  if (flush_info_iter == stream_flush_info_.end())
    return;

  StreamFlushInfo& flush_info = flush_info_iter->second;
  if (flush_info.flush_pending)
    InternalFlush(&flush_info);
}

void GpuChannelHost::InternalFlush(StreamFlushInfo* flush_info) {
  context_lock_.AssertAcquired();
  DCHECK(flush_info);
  DCHECK(flush_info->flush_pending);
  DCHECK_LT(flush_info->flushed_stream_flush_id, flush_info->flush_id);
  Send(new GpuCommandBufferMsg_AsyncFlush(
      flush_info->route_id, flush_info->put_offset, flush_info->flush_count,
      flush_info->latency_info));
  flush_info->latency_info.clear();
  flush_info->flush_pending = false;

  flush_info->flushed_stream_flush_id = flush_info->flush_id;
}

scoped_ptr<CommandBufferProxyImpl> GpuChannelHost::CreateViewCommandBuffer(
    int32_t surface_id,
    CommandBufferProxyImpl* share_group,
    int32_t stream_id,
    GpuStreamPriority stream_priority,
    const std::vector<int32_t>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  DCHECK(!share_group || (stream_id == share_group->stream_id()));
  TRACE_EVENT1("gpu", "GpuChannelHost::CreateViewCommandBuffer", "surface_id",
               surface_id);

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->route_id() : MSG_ROUTING_NONE;
  init_params.stream_id = stream_id;
  init_params.stream_priority = stream_priority;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;

  int32_t route_id = GenerateRouteID();

  gfx::GLSurfaceHandle surface_handle = factory_->GetSurfaceHandle(surface_id);
  DCHECK(!surface_handle.is_null());

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "125248 BrowserGpuChannelHostFactory::CreateViewCommandBuffer"));

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).

  bool succeeded = false;
  if (!Send(new GpuChannelMsg_CreateViewCommandBuffer(
          surface_handle, init_params, route_id, &succeeded))) {
    LOG(ERROR) << "Failed to send GpuChannelMsg_CreateViewCommandBuffer.";
    return nullptr;
  }

  if (!succeeded) {
    LOG(ERROR)
        << "GpuChannelMsg_CreateOffscreenCommandBuffer returned failure.";
    return nullptr;
  }

  scoped_ptr<CommandBufferProxyImpl> command_buffer =
      make_scoped_ptr(new CommandBufferProxyImpl(this, route_id, stream_id));
  AddRoute(route_id, command_buffer->AsWeakPtr());

  return command_buffer;
}

scoped_ptr<CommandBufferProxyImpl> GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    CommandBufferProxyImpl* share_group,
    int32_t stream_id,
    GpuStreamPriority stream_priority,
    const std::vector<int32_t>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  DCHECK(!share_group || (stream_id == share_group->stream_id()));
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateOffscreenCommandBuffer");

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->route_id() : MSG_ROUTING_NONE;
  init_params.stream_id = stream_id;
  init_params.stream_priority = stream_priority;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;

  int32_t route_id = GenerateRouteID();

  bool succeeded = false;
  if (!Send(new GpuChannelMsg_CreateOffscreenCommandBuffer(
          size, init_params, route_id, &succeeded))) {
    LOG(ERROR) << "Failed to send GpuChannelMsg_CreateOffscreenCommandBuffer.";
    return nullptr;
  }

  if (!succeeded) {
    LOG(ERROR)
        << "GpuChannelMsg_CreateOffscreenCommandBuffer returned failure.";
    return nullptr;
  }

  scoped_ptr<CommandBufferProxyImpl> command_buffer =
      make_scoped_ptr(new CommandBufferProxyImpl(this, route_id, stream_id));
  AddRoute(route_id, command_buffer->AsWeakPtr());

  return command_buffer;
}

scoped_ptr<media::JpegDecodeAccelerator> GpuChannelHost::CreateJpegDecoder(
    media::JpegDecodeAccelerator::Client* client) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateJpegDecoder");

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  int32_t route_id = GenerateRouteID();
  scoped_ptr<GpuJpegDecodeAcceleratorHost> decoder(
      new GpuJpegDecodeAcceleratorHost(this, route_id, io_task_runner));
  if (!decoder->Initialize(client)) {
    return nullptr;
  }

  // The reply message of jpeg decoder should run on IO thread.
  io_task_runner->PostTask(FROM_HERE,
                           base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                                      channel_filter_.get(), route_id,
                                      decoder->GetReceiver(), io_task_runner));

  return std::move(decoder);
}

void GpuChannelHost::DestroyCommandBuffer(
    CommandBufferProxyImpl* command_buffer) {
  TRACE_EVENT0("gpu", "GpuChannelHost::DestroyCommandBuffer");

  int32_t route_id = command_buffer->route_id();
  int32_t stream_id = command_buffer->stream_id();
  Send(new GpuChannelMsg_DestroyCommandBuffer(route_id));
  RemoveRoute(route_id);

  AutoLock lock(context_lock_);
  StreamFlushInfo& flush_info = stream_flush_info_[stream_id];
  if (flush_info.flush_pending && flush_info.route_id == route_id)
    flush_info.flush_pending = false;
}

void GpuChannelHost::DestroyChannel() {
  DCHECK(factory_->IsMainThread());
  AutoLock lock(context_lock_);
  channel_.reset();
}

void GpuChannelHost::AddRoute(
    int route_id, base::WeakPtr<IPC::Listener> listener) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(FROM_HERE,
                           base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                                      channel_filter_.get(), route_id, listener,
                                      base::ThreadTaskRunnerHandle::Get()));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      factory_->GetIOThreadTaskRunner();
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(&GpuChannelHost::MessageFilter::RemoveRoute,
                            channel_filter_.get(), route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    base::SharedMemoryHandle source_handle) {
  if (IsLost())
    return base::SharedMemory::NULLHandle();

  return base::SharedMemory::DuplicateHandle(source_handle);
}

int32_t GpuChannelHost::ReserveTransferBufferId() {
  // 0 is a reserved value.
  return g_next_transfer_buffer_id.GetNext() + 1;
}

gfx::GpuMemoryBufferHandle GpuChannelHost::ShareGpuMemoryBufferToGpuProcess(
    const gfx::GpuMemoryBufferHandle& source_handle,
    bool* requires_sync_point) {
  switch (source_handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.handle = ShareToGpuProcess(source_handle.handle);
      handle.offset = source_handle.offset;
      handle.stride = source_handle.stride;
      *requires_sync_point = false;
      return handle;
    }
    case gfx::IO_SURFACE_BUFFER:
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      *requires_sync_point = true;
      return source_handle;
    default:
      NOTREACHED();
      return gfx::GpuMemoryBufferHandle();
  }
}

int32_t GpuChannelHost::ReserveImageId() {
  return next_image_id_.GetNext();
}

int32_t GpuChannelHost::GenerateRouteID() {
  return next_route_id_.GetNext();
}

int32_t GpuChannelHost::GenerateStreamID() {
  const int32_t stream_id = next_stream_id_.GetNext();
  DCHECK_NE(0, stream_id);
  DCHECK_NE(kDefaultStreamId, stream_id);
  return stream_id;
}

uint32_t GpuChannelHost::ValidateFlushIDReachedServer(int32_t stream_id,
                                                      bool force_validate) {
  // Store what flush ids we will be validating for all streams.
  base::hash_map<int32_t, uint32_t> validate_flushes;
  uint32_t flushed_stream_flush_id = 0;
  uint32_t verified_stream_flush_id = 0;
  {
    AutoLock lock(context_lock_);
    for (const auto& iter : stream_flush_info_) {
      const int32_t iter_stream_id = iter.first;
      const StreamFlushInfo& flush_info = iter.second;
      if (iter_stream_id == stream_id) {
        flushed_stream_flush_id = flush_info.flushed_stream_flush_id;
        verified_stream_flush_id = flush_info.verified_stream_flush_id;
      }

      if (flush_info.flushed_stream_flush_id >
          flush_info.verified_stream_flush_id) {
        validate_flushes.insert(
            std::make_pair(iter_stream_id, flush_info.flushed_stream_flush_id));
      }
    }
  }

  if (!force_validate && flushed_stream_flush_id == verified_stream_flush_id) {
    // Current stream has no unverified flushes.
    return verified_stream_flush_id;
  }

  if (Send(new GpuChannelMsg_Nop())) {
    // Update verified flush id for all streams.
    uint32_t highest_flush_id = 0;
    AutoLock lock(context_lock_);
    for (const auto& iter : validate_flushes) {
      const int32_t validated_stream_id = iter.first;
      const uint32_t validated_flush_id = iter.second;
      StreamFlushInfo& flush_info = stream_flush_info_[validated_stream_id];
      if (flush_info.verified_stream_flush_id < validated_flush_id) {
        flush_info.verified_stream_flush_id = validated_flush_id;
      }

      if (validated_stream_id == stream_id)
        highest_flush_id = flush_info.verified_stream_flush_id;
    }

    return highest_flush_id;
  }

  return 0;
}

uint32_t GpuChannelHost::GetHighestValidatedFlushID(int32_t stream_id) {
  AutoLock lock(context_lock_);
  StreamFlushInfo& flush_info = stream_flush_info_[stream_id];
  return flush_info.verified_stream_flush_id;
}

GpuChannelHost::~GpuChannelHost() {
#if DCHECK_IS_ON()
  AutoLock lock(context_lock_);
  DCHECK(!channel_)
      << "GpuChannelHost::DestroyChannel must be called before destruction.";
#endif
}

GpuChannelHost::MessageFilter::ListenerInfo::ListenerInfo() {}

GpuChannelHost::MessageFilter::ListenerInfo::ListenerInfo(
    const ListenerInfo& other) = default;

GpuChannelHost::MessageFilter::ListenerInfo::~ListenerInfo() {}

GpuChannelHost::MessageFilter::MessageFilter()
    : lost_(false) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {}

void GpuChannelHost::MessageFilter::AddRoute(
    int32_t route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(listeners_.find(route_id) == listeners_.end());
  DCHECK(task_runner);
  ListenerInfo info;
  info.listener = listener;
  info.task_runner = task_runner;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int32_t route_id) {
  listeners_.erase(route_id);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  auto it = listeners_.find(message.routing_id());
  if (it == listeners_.end())
    return false;

  const ListenerInfo& info = it->second;
  info.task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                 info.listener, message));
  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (const auto& kv : listeners_) {
    const ListenerInfo& info = kv.second;
    info.task_runner->PostTask(
        FROM_HERE, base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  listeners_.clear();
}

bool GpuChannelHost::MessageFilter::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

}  // namespace content
