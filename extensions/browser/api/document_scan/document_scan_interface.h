// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace extensions {

namespace api {

class DocumentScanInterface {
 public:
  struct ScannerDescription {
    ScannerDescription();
    ScannerDescription(const ScannerDescription& other);
    ~ScannerDescription();
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string scanner_type;
    std::string image_mime_type;
  };

  enum ScanMode { kScanModeColor, kScanModeGray, kScanModeLineart };

  typedef base::Callback<void(
      const std::vector<ScannerDescription>& scanner_descriptions,
      const std::string& error)> ListScannersResultsCallback;

  typedef base::Callback<void(const std::string& scanned_image,
                              const std::string& mime_type,
                              const std::string& error)> ScanResultsCallback;

  virtual ~DocumentScanInterface();

  virtual void Scan(const std::string& scanner_name,
                    ScanMode mode,
                    int resolution_dpi,
                    const ScanResultsCallback& callback) = 0;
  virtual void ListScanners(const ListScannersResultsCallback& callback) = 0;

  // Creates a platform-specific DocumentScanInterface instance.
  static DocumentScanInterface* CreateInstance();

 protected:
  DocumentScanInterface();
};

}  // namespace api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_H_
