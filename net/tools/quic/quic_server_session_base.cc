// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_server_session_base.h"

#include "base/logging.h"
#include "net/quic/proto/cached_network_parameters.pb.h"
#include "net/quic/quic_bug_tracker.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_spdy_session.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

QuicServerSessionBase::QuicServerSessionBase(
    const QuicConfig& config,
    QuicConnection* connection,
    QuicServerSessionVisitor* visitor,
    const QuicCryptoServerConfig* crypto_config)
    : QuicSpdySession(connection, config),
      crypto_config_(crypto_config),
      visitor_(visitor),
      bandwidth_resumption_enabled_(false),
      bandwidth_estimate_sent_to_client_(QuicBandwidth::Zero()),
      last_scup_time_(QuicTime::Zero()),
      last_scup_packet_number_(0) {}

QuicServerSessionBase::~QuicServerSessionBase() {}

void QuicServerSessionBase::Initialize() {
  crypto_stream_.reset(CreateQuicCryptoServerStream(crypto_config_));
  QuicSpdySession::Initialize();
}

void QuicServerSessionBase::OnConfigNegotiated() {
  QuicSession::OnConfigNegotiated();

  if (!config()->HasReceivedConnectionOptions()) {
    return;
  }

  // Enable bandwidth resumption if peer sent correct connection options.
  const bool last_bandwidth_resumption =
      ContainsQuicTag(config()->ReceivedConnectionOptions(), kBWRE);
  const bool max_bandwidth_resumption =
      ContainsQuicTag(config()->ReceivedConnectionOptions(), kBWMX);
  bandwidth_resumption_enabled_ =
      last_bandwidth_resumption || max_bandwidth_resumption;

  // If the client has provided a bandwidth estimate from the same serving
  // region as this server, then decide whether to use the data for bandwidth
  // resumption.
  const CachedNetworkParameters* cached_network_params =
      crypto_stream_->PreviousCachedNetworkParams();
  if (cached_network_params != nullptr &&
      cached_network_params->serving_region() == serving_region_) {
    if (FLAGS_quic_log_received_parameters) {
      connection()->OnReceiveConnectionState(*cached_network_params);
    }

    if (bandwidth_resumption_enabled_) {
      // Only do bandwidth resumption if estimate is recent enough.
      const int64_t seconds_since_estimate =
          connection()->clock()->WallNow().ToUNIXSeconds() -
          cached_network_params->timestamp();
      if (seconds_since_estimate <= kNumSecondsPerHour) {
        connection()->ResumeConnectionState(*cached_network_params,
                                            max_bandwidth_resumption);
      }
    }
  }

  if (FLAGS_enable_quic_fec &&
      ContainsQuicTag(config()->ReceivedConnectionOptions(), kFHDR)) {
    // kFHDR config maps to FEC protection always for headers stream.
    // TODO(jri): Add crypto stream in addition to headers for kHDR.
    headers_stream()->set_fec_policy(FEC_PROTECT_ALWAYS);
  }
}

void QuicServerSessionBase::OnConnectionClosed(QuicErrorCode error,
                                               ConnectionCloseSource source) {
  QuicSession::OnConnectionClosed(error, source);
  // In the unlikely event we get a connection close while doing an asynchronous
  // crypto event, make sure we cancel the callback.
  if (crypto_stream_.get() != nullptr) {
    crypto_stream_->CancelOutstandingCallbacks();
  }
  visitor_->OnConnectionClosed(connection()->connection_id(), error);
}

void QuicServerSessionBase::OnWriteBlocked() {
  QuicSession::OnWriteBlocked();
  visitor_->OnWriteBlocked(connection());
}

void QuicServerSessionBase::OnCongestionWindowChange(QuicTime now) {
  if (!bandwidth_resumption_enabled_) {
    return;
  }
  // Only send updates when the application has no data to write.
  if (HasDataToWrite()) {
    return;
  }

  // If not enough time has passed since the last time we sent an update to the
  // client, or not enough packets have been sent, then return early.
  const QuicSentPacketManager& sent_packet_manager =
      connection()->sent_packet_manager();
  int64_t srtt_ms =
      sent_packet_manager.GetRttStats()->smoothed_rtt().ToMilliseconds();
  int64_t now_ms = now.Subtract(last_scup_time_).ToMilliseconds();
  int64_t packets_since_last_scup =
      connection()->packet_number_of_last_sent_packet() -
      last_scup_packet_number_;
  if (now_ms < (kMinIntervalBetweenServerConfigUpdatesRTTs * srtt_ms) ||
      now_ms < kMinIntervalBetweenServerConfigUpdatesMs ||
      packets_since_last_scup < kMinPacketsBetweenServerConfigUpdates) {
    return;
  }

  // If the bandwidth recorder does not have a valid estimate, return early.
  const QuicSustainedBandwidthRecorder& bandwidth_recorder =
      sent_packet_manager.SustainedBandwidthRecorder();
  if (!bandwidth_recorder.HasEstimate()) {
    return;
  }

  // The bandwidth recorder has recorded at least one sustained bandwidth
  // estimate. Check that it's substantially different from the last one that
  // we sent to the client, and if so, send the new one.
  QuicBandwidth new_bandwidth_estimate = bandwidth_recorder.BandwidthEstimate();

  int64_t bandwidth_delta =
      std::abs(new_bandwidth_estimate.ToBitsPerSecond() -
               bandwidth_estimate_sent_to_client_.ToBitsPerSecond());

  // Define "substantial" difference as a 50% increase or decrease from the
  // last estimate.
  bool substantial_difference =
      bandwidth_delta >
      0.5 * bandwidth_estimate_sent_to_client_.ToBitsPerSecond();
  if (!substantial_difference) {
    return;
  }

  bandwidth_estimate_sent_to_client_ = new_bandwidth_estimate;
  DVLOG(1) << "Server: sending new bandwidth estimate (KBytes/s): "
           << bandwidth_estimate_sent_to_client_.ToKBytesPerSecond();

  // Include max bandwidth in the update.
  QuicBandwidth max_bandwidth_estimate =
      bandwidth_recorder.MaxBandwidthEstimate();
  int32_t max_bandwidth_timestamp = bandwidth_recorder.MaxBandwidthTimestamp();

  // Fill the proto before passing it to the crypto stream to send.
  CachedNetworkParameters cached_network_params;
  cached_network_params.set_bandwidth_estimate_bytes_per_second(
      bandwidth_estimate_sent_to_client_.ToBytesPerSecond());
  cached_network_params.set_max_bandwidth_estimate_bytes_per_second(
      max_bandwidth_estimate.ToBytesPerSecond());
  cached_network_params.set_max_bandwidth_timestamp_seconds(
      max_bandwidth_timestamp);
  cached_network_params.set_min_rtt_ms(
      sent_packet_manager.GetRttStats()->min_rtt().ToMilliseconds());
  cached_network_params.set_previous_connection_state(
      bandwidth_recorder.EstimateRecordedDuringSlowStart()
          ? CachedNetworkParameters::SLOW_START
          : CachedNetworkParameters::CONGESTION_AVOIDANCE);
  cached_network_params.set_timestamp(
      connection()->clock()->WallNow().ToUNIXSeconds());
  if (!serving_region_.empty()) {
    cached_network_params.set_serving_region(serving_region_);
  }

  crypto_stream_->SendServerConfigUpdate(&cached_network_params);

  connection()->OnSendConnectionState(cached_network_params);

  last_scup_time_ = now;
  last_scup_packet_number_ = connection()->packet_number_of_last_sent_packet();
}

bool QuicServerSessionBase::ShouldCreateIncomingDynamicStream(QuicStreamId id) {
  if (!connection()->connected()) {
    QUIC_BUG << "ShouldCreateIncomingDynamicStream called when disconnected";
    return false;
  }

  if (id % 2 == 0) {
    DVLOG(1) << "Invalid incoming even stream_id:" << id;
    connection()->SendConnectionCloseWithDetails(
        QUIC_INVALID_STREAM_ID, "Client created even numbered stream");
    return false;
  }
  return true;
}

bool QuicServerSessionBase::ShouldCreateOutgoingDynamicStream() {
  if (!connection()->connected()) {
    QUIC_BUG << "ShouldCreateOutgoingDynamicStream called when disconnected";
    return false;
  }
  if (!crypto_stream_->encryption_established()) {
    QUIC_BUG << "Encryption not established so no outgoing stream created.";
    return false;
  }
  if (GetNumOpenOutgoingStreams() >= max_open_outgoing_streams()) {
    VLOG(1) << "No more streams should be created. "
            << "Already " << GetNumOpenOutgoingStreams() << " open.";
    return false;
  }
  return true;
}

QuicCryptoServerStreamBase* QuicServerSessionBase::GetCryptoStream() {
  return crypto_stream_.get();
}

}  // namespace net
