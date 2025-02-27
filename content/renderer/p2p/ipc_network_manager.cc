// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include <string>
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/common/content_switches.h"
#include "jingle/glue/utils.h"
#include "net/base/ip_address_number.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "third_party/webrtc/base/socketaddress.h"

namespace content {

namespace {

rtc::AdapterType ConvertConnectionTypeToAdapterType(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
        return rtc::ADAPTER_TYPE_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
        return rtc::ADAPTER_TYPE_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
        return rtc::ADAPTER_TYPE_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
        return rtc::ADAPTER_TYPE_CELLULAR;
    default:
        return rtc::ADAPTER_TYPE_UNKNOWN;
  }
}

}  // namespace

IpcNetworkManager::IpcNetworkManager(NetworkListManager* network_list_manager)
    : network_list_manager_(network_list_manager),
      start_count_(0),
      network_list_received_(false),
      weak_factory_(this) {
  network_list_manager_->AddNetworkListObserver(this);
}

IpcNetworkManager::~IpcNetworkManager() {
  DCHECK(!start_count_);
  network_list_manager_->RemoveNetworkListObserver(this);
}

void IpcNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&IpcNetworkManager::SendNetworksChangedSignal,
                              weak_factory_.GetWeakPtr()));
  }
  ++start_count_;
}

void IpcNetworkManager::StopUpdating() {
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void IpcNetworkManager::OnNetworkListChanged(
    const net::NetworkInterfaceList& list,
    const net::IPAddressNumber& default_ipv4_local_address,
    const net::IPAddressNumber& default_ipv6_local_address) {
  // Update flag if network list received for the first time.
  if (!network_list_received_)
    network_list_received_ = true;

  // Default addresses should be set only when they are in the filtered list of
  // network addresses.
  bool use_default_ipv4_address = false;
  bool use_default_ipv6_address = false;

  // rtc::Network uses these prefix_length to compare network
  // interfaces discovered.
  std::vector<rtc::Network*> networks;
  for (net::NetworkInterfaceList::const_iterator it = list.begin();
       it != list.end(); it++) {
    rtc::IPAddress ip_address =
        jingle_glue::IPAddressNumberToIPAddress(it->address);
    DCHECK(!ip_address.IsNil());

    rtc::IPAddress prefix = rtc::TruncateIP(ip_address, it->prefix_length);
    scoped_ptr<rtc::Network> network(
        new rtc::Network(it->name, it->name, prefix, it->prefix_length,
                         ConvertConnectionTypeToAdapterType(it->type)));
    network->set_default_local_address_provider(this);

    rtc::InterfaceAddress iface_addr;
    if (it->address.size() == net::kIPv4AddressSize) {
      use_default_ipv4_address |= (default_ipv4_local_address == it->address);
      iface_addr = rtc::InterfaceAddress(ip_address);
    } else {
      DCHECK(it->address.size() == net::kIPv6AddressSize);
      iface_addr = rtc::InterfaceAddress(ip_address, it->ip_address_attributes);

      // Only allow non-private, non-deprecated IPv6 addresses which don't
      // contain MAC.
      if (rtc::IPIsMacBased(iface_addr) ||
          (it->ip_address_attributes & net::IP_ADDRESS_ATTRIBUTE_DEPRECATED) ||
          rtc::IPIsPrivate(iface_addr)) {
        continue;
      }

      use_default_ipv6_address |= (default_ipv6_local_address == it->address);
    }
    network->AddIP(iface_addr);
    networks.push_back(network.release());
  }

  // Update the default local addresses.
  rtc::IPAddress ipv4_default;
  rtc::IPAddress ipv6_defualt;
  if (use_default_ipv4_address) {
    ipv4_default =
        jingle_glue::IPAddressNumberToIPAddress(default_ipv4_local_address);
  }
  if (use_default_ipv6_address) {
    ipv6_defualt =
        jingle_glue::IPAddressNumberToIPAddress(default_ipv6_local_address);
  }
  set_default_local_addresses(ipv4_default, ipv6_defualt);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowLoopbackInPeerConnection)) {
    std::string name_v4("loopback_ipv4");
    rtc::IPAddress ip_address_v4(INADDR_LOOPBACK);
    rtc::Network* network_v4 = new rtc::Network(
        name_v4, name_v4, ip_address_v4, 32, rtc::ADAPTER_TYPE_UNKNOWN);
    network_v4->set_default_local_address_provider(this);
    network_v4->AddIP(ip_address_v4);
    networks.push_back(network_v4);

    rtc::IPAddress ipv6_default_address;
    // Only add IPv6 loopback if we can get default local address for IPv6. If
    // we can't, it means that we don't have IPv6 enabled on this machine and
    // bind() to the IPv6 loopback address will fail.
    if (GetDefaultLocalAddress(AF_INET6, &ipv6_default_address)) {
      DCHECK(!ipv6_default_address.IsNil());
      std::string name_v6("loopback_ipv6");
      rtc::IPAddress ip_address_v6(in6addr_loopback);
      rtc::Network* network_v6 = new rtc::Network(
          name_v6, name_v6, ip_address_v6, 64, rtc::ADAPTER_TYPE_UNKNOWN);
      network_v6->set_default_local_address_provider(this);
      network_v6->AddIP(ip_address_v6);
      networks.push_back(network_v6);
    }
  }

  bool changed = false;
  NetworkManager::Stats stats;
  MergeNetworkList(networks, &changed, &stats);
  if (changed)
    SignalNetworksChanged();

  // Send interface counts to UMA.
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4Interfaces",
                           stats.ipv4_network_count);
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6Interfaces",
                           stats.ipv6_network_count);
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
