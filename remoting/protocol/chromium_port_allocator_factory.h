// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/port_allocator_factory.h"

namespace remoting {
namespace protocol {

class ChromiumPortAllocatorFactory : public PortAllocatorFactory {
 public:
  ChromiumPortAllocatorFactory();
  ~ChromiumPortAllocatorFactory() override;

   // PortAllocatorFactory interface.
  scoped_ptr<cricket::PortAllocator> CreatePortAllocator(
      scoped_refptr<TransportContext> transport_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMIUM_PORT_ALLOCATOR_FACTORY_H_
