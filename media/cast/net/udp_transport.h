// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_UDP_TRANSPORT_H_
#define MEDIA_CAST_NET_UDP_TRANSPORT_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/diff_serv_code_point.h"
#include "net/udp/udp_socket.h"

namespace net {
class NetLog;
}  // namespace net

namespace media {
namespace cast {

// This class implements UDP transport mechanism for Cast.
class UdpTransport : public PacketSender {
 public:
  // Construct a UDP transport.
  // All methods must be called on |io_thread_proxy|.
  // |local_end_point| specifies the address and port to bind and listen
  // to incoming packets. If the value is 0.0.0.0:0 then a bind is not
  // performed.
  // |remote_end_point| specifies the address and port to send packets
  // to. If the value is 0.0.0.0:0 the the end point is set to the source
  // address of the first packet received.
  // |send_buffer_size| specifies the size of the socket send buffer.
  UdpTransport(
      net::NetLog* net_log,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_proxy,
      const net::IPEndPoint& local_end_point,
      const net::IPEndPoint& remote_end_point,
      const CastTransportStatusCallback& status_callback);
  ~UdpTransport() final;

  // Start receiving packets. Packets are submitted to |packet_receiver|.
  void StartReceiving(
      const PacketReceiverCallbackWithStatus& packet_receiver) final;
  void StopReceiving() final;

  // Set a new DSCP value to the socket. The value will be set right before
  // the next send.
  void SetDscp(net::DiffServCodePoint dscp);

  // Set UdpTransport options.
  // Possible keys are:
  //   "pacer_max_burst_size": int
  //        - Specifies how many pakcets to send per 10 ms, maximum.
  //   "send_buffer_min_size": int
  //        - Specifies the minimum socket send buffer size.
  //   "DSCP" (value ignored)
  //       - Turns DSCP on (higher IP Precedence and Type of Service).
  //   "disable_non_blocking_io" (value ignored)
  //       - Windows only.  Turns off non-blocking IO for the socket.
  //         Note: Non-blocking IO is, by default, enabled on all platforms.
  void SetUdpOptions(const base::DictionaryValue& options);

  // This has to be called before |StartReceiving()| to change the
  // |send_buffer_size_|. Calling |SetUdpOptions()| will automatically call it.
  void SetSendBufferSize(int32_t send_buffer_size);

#if defined(OS_WIN)
  // Switch to use non-blocking IO. Must be called before StartReceiving().
  void UseNonBlockingIO();
#endif

  // PacketSender implementations.
  bool SendPacket(PacketRef packet, const base::Closure& cb) final;
  int64_t GetBytesSent() final;

 private:
  // Requests and processes packets from |udp_socket_|.  This method is called
  // once with |length_or_status| set to net::ERR_IO_PENDING to start receiving
  // packets.  Thereafter, it is called with some other value as the callback
  // response from UdpSocket::RecvFrom().
  void ReceiveNextPacket(int length_or_status);

  // Schedule packet receiving, if needed.
  void ScheduleReceiveNextPacket();

  void OnSent(const scoped_refptr<net::IOBuffer>& buf,
              PacketRef packet,
              const base::Closure& cb,
              int result);

  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy_;
  const net::IPEndPoint local_addr_;
  net::IPEndPoint remote_addr_;
  scoped_ptr<net::UDPSocket> udp_socket_;
  bool send_pending_;
  bool receive_pending_;
  bool client_connected_;
  net::DiffServCodePoint next_dscp_value_;
  scoped_ptr<Packet> next_packet_;
  scoped_refptr<net::WrappedIOBuffer> recv_buf_;
  net::IPEndPoint recv_addr_;
  PacketReceiverCallbackWithStatus packet_receiver_;
  int32_t send_buffer_size_;
  const CastTransportStatusCallback status_callback_;
  int bytes_sent_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<UdpTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UdpTransport);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_UDP_TRANSPORT_H_
