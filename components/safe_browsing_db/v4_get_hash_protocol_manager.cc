// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using base::Time;
using base::TimeDelta;

namespace {

// Enumerate parsing failures for histogramming purposes.  DO NOT CHANGE
// THE ORDERING OF THESE VALUES.
enum ParseResultType {
  // Error parsing the protocol buffer from a string.
  PARSE_FROM_STRING_ERROR = 0,

  // A match in the response had an unexpected THREAT_ENTRY_TYPE.
  UNEXPECTED_THREAT_ENTRY_TYPE_ERROR = 1,

  // A match in the response had an unexpected THREAT_TYPE.
  UNEXPECTED_THREAT_TYPE_ERROR = 2,

  // A match in the response had an unexpected PLATFORM_TYPE.
  UNEXPECTED_PLATFORM_TYPE_ERROR = 3,

  // A match in the response contained no metadata where metadata was
  // expected.
  NO_METADATA_ERROR = 4,

  // A match in the response contained a ThreatType that was inconsistent
  // with the other matches.
  INCONSISTENT_THREAT_TYPE_ERROR = 5,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  PARSE_RESULT_TYPE_MAX = 6
};

// Record parsing errors of a GetHash result.
void RecordParseGetHashResult(ParseResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.ParseV4HashResult", result_type,
                            PARSE_RESULT_TYPE_MAX);
}

}  // namespace

namespace safe_browsing {

const char kUmaV4HashResponseMetricName[] =
    "SafeBrowsing.GetV4HashHttpResponseOrErrorCode";

// The default V4GetHashProtocolManagerFactory.
class V4GetHashProtocolManagerFactoryImpl
    : public V4GetHashProtocolManagerFactory {
 public:
  V4GetHashProtocolManagerFactoryImpl() {}
  ~V4GetHashProtocolManagerFactoryImpl() override {}
  V4GetHashProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override {
    return new V4GetHashProtocolManager(request_context_getter, config);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(V4GetHashProtocolManagerFactoryImpl);
};

// V4GetHashProtocolManager implementation --------------------------------

// static
V4GetHashProtocolManagerFactory* V4GetHashProtocolManager::factory_ = NULL;

// static
V4GetHashProtocolManager* V4GetHashProtocolManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  if (!factory_)
    factory_ = new V4GetHashProtocolManagerFactoryImpl();
  return factory_->CreateProtocolManager(request_context_getter, config);
}

void V4GetHashProtocolManager::ResetGetHashErrors() {
  gethash_error_count_ = 0;
  gethash_back_off_mult_ = 1;
}

V4GetHashProtocolManager::V4GetHashProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config)
    : gethash_error_count_(0),
      gethash_back_off_mult_(1),
      next_gethash_time_(Time::FromDoubleT(0)),
      config_(config),
      request_context_getter_(request_context_getter),
      url_fetcher_id_(0) {
}

// static
void V4GetHashProtocolManager::RecordGetHashResult(ResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.GetV4HashResult", result_type,
                            GET_HASH_RESULT_MAX);
}

V4GetHashProtocolManager::~V4GetHashProtocolManager() {
  // Delete in-progress SafeBrowsing requests.
  STLDeleteContainerPairFirstPointers(hash_requests_.begin(),
                                      hash_requests_.end());
  hash_requests_.clear();
}

std::string V4GetHashProtocolManager::GetHashRequest(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<PlatformType>& platforms,
    ThreatType threat_type) {
  // Build the request. Client info and client states are not added to the
  // request protocol buffer. Client info is passed as params in the url.
  FindFullHashesRequest req;
  ThreatInfo* info = req.mutable_threat_info();
  info->add_threat_types(threat_type);
  info->add_threat_entry_types(URL_EXPRESSION);
  for (const PlatformType p : platforms) {
    info->add_platform_types(p);
  }
  for (const SBPrefix& prefix : prefixes) {
    std::string hash(reinterpret_cast<const char*>(&prefix), sizeof(SBPrefix));
    info->add_threat_entries()->set_hash(hash);
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  return req_base64;
}

bool V4GetHashProtocolManager::ParseHashResponse(
    const std::string& data,
    std::vector<SBFullHashResult>* full_hashes,
    base::TimeDelta* negative_cache_duration) {
  FindFullHashesResponse response;

  if (!response.ParseFromString(data)) {
    RecordParseGetHashResult(PARSE_FROM_STRING_ERROR);
    return false;
  }

  if (response.has_negative_cache_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    *negative_cache_duration = base::TimeDelta::FromSeconds(
        response.negative_cache_duration().seconds());
  }

  if (response.has_minimum_wait_duration()) {
    // Seconds resolution is good enough so we ignore the nanos field.
    next_gethash_time_ =
        Time::Now() + base::TimeDelta::FromSeconds(
                          response.minimum_wait_duration().seconds());
  }

  // We only expect one threat type per request, so we make sure
  // the threat types are consistent between matches.
  ThreatType expected_threat_type = THREAT_TYPE_UNSPECIFIED;

  // Loop over the threat matches and fill in full_hashes.
  for (const ThreatMatch& match : response.matches()) {
    // Make sure the platform and threat entry type match.
    if (!(match.has_threat_entry_type() &&
          match.threat_entry_type() == URL_EXPRESSION && match.has_threat())) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_ENTRY_TYPE_ERROR);
      return false;
    }

    if (!match.has_threat_type()) {
      RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
      return false;
    }

    if (expected_threat_type == THREAT_TYPE_UNSPECIFIED) {
      expected_threat_type = match.threat_type();
    } else if (match.threat_type() != expected_threat_type) {
      RecordParseGetHashResult(INCONSISTENT_THREAT_TYPE_ERROR);
      return false;
    }

    // Fill in the full hash.
    SBFullHashResult result;
    result.hash = StringToSBFullHash(match.threat().hash());

    if (match.has_cache_duration()) {
      // Seconds resolution is good enough so we ignore the nanos field.
      result.cache_duration =
          base::TimeDelta::FromSeconds(match.cache_duration().seconds());
    }

    // Different threat types will handle the metadata differently.
    if (match.threat_type() == API_ABUSE) {
      if (match.has_platform_type() &&
          match.platform_type() == CHROME_PLATFORM) {
        if (match.has_threat_entry_metadata()) {
          // For API Abuse, store a csv of the returned permissions.
          for (const ThreatEntryMetadata::MetadataEntry& m :
               match.threat_entry_metadata().entries()) {
            if (m.key() == "permission") {
              result.metadata += m.value() + ",";
            }
          }
        } else {
          RecordParseGetHashResult(NO_METADATA_ERROR);
          return false;
        }
      } else {
        RecordParseGetHashResult(UNEXPECTED_PLATFORM_TYPE_ERROR);
        return false;
      }
    } else {
      RecordParseGetHashResult(UNEXPECTED_THREAT_TYPE_ERROR);
      return false;
    }

    full_hashes->push_back(result);
  }
  return true;
}

void V4GetHashProtocolManager::GetFullHashes(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<PlatformType>& platforms,
    ThreatType threat_type,
    FullHashCallback callback) {
  DCHECK(CalledOnValidThread());
  // We need to wait the minimum waiting duration, and if we are in backoff,
  // we need to check if we're past the next allowed time. If we are, we can
  // proceed with the request. If not, we are required to return empty results
  // (i.e. treat the page as safe).
  if (Time::Now() <= next_gethash_time_) {
    if (gethash_error_count_) {
      RecordGetHashResult(GET_HASH_BACKOFF_ERROR);
    } else {
      RecordGetHashResult(GET_HASH_MIN_WAIT_DURATION_ERROR);
    }
    std::vector<SBFullHashResult> full_hashes;
    callback.Run(full_hashes, base::TimeDelta());
    return;
  }

  std::string req_base64 = GetHashRequest(prefixes, platforms, threat_type);
  GURL gethash_url = GetHashUrl(req_base64);

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url_fetcher_id_++, gethash_url,
                              net::URLFetcher::GET, this)
          .release();
  hash_requests_[fetcher] = callback;

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->Start();
}

void V4GetHashProtocolManager::GetFullHashesWithApis(
    const std::vector<SBPrefix>& prefixes,
    FullHashCallback callback) {
  std::vector<PlatformType> platform = {CHROME_PLATFORM};
  GetFullHashes(prefixes, platform, API_ABUSE, callback);
}

// net::URLFetcherDelegate implementation ----------------------------------

// SafeBrowsing request responses are handled here.
void V4GetHashProtocolManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  HashRequests::iterator it = hash_requests_.find(source);
  DCHECK(it != hash_requests_.end()) << "Request not found";

  // FindFullHashes response.
  // Reset the scoped pointer so the fetcher gets destroyed properly.
  scoped_ptr<const net::URLFetcher> fetcher(it->first);

  int response_code = source->GetResponseCode();
  net::URLRequestStatus status = source->GetStatus();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      kUmaV4HashResponseMetricName, status, response_code);

  const FullHashCallback& callback = it->second;
  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta negative_cache_duration;
  if (status.is_success() && response_code == net::HTTP_OK) {
    RecordGetHashResult(GET_HASH_STATUS_200);
    ResetGetHashErrors();
    std::string data;
    source->GetResponseAsString(&data);
    if (!ParseHashResponse(data, &full_hashes, &negative_cache_duration)) {
      full_hashes.clear();
      RecordGetHashResult(GET_HASH_PARSE_ERROR);
    }
  } else {
    HandleGetHashError(Time::Now());

    DVLOG(1) << "SafeBrowsing GetEncodedFullHashes request for: "
             << source->GetURL() << " failed with error: " << status.error()
             << " and response code: " << response_code;

    if (status.status() == net::URLRequestStatus::FAILED) {
      RecordGetHashResult(GET_HASH_NETWORK_ERROR);
    } else {
      RecordGetHashResult(GET_HASH_HTTP_ERROR);
    }
  }

  // Invoke the callback with full_hashes, even if there was a parse error or
  // an error response code (in which case full_hashes will be empty). The
  // caller can't be blocked indefinitely.
  callback.Run(full_hashes, negative_cache_duration);

  hash_requests_.erase(it);
}

void V4GetHashProtocolManager::HandleGetHashError(const Time& now) {
  DCHECK(CalledOnValidThread());
  base::TimeDelta next = V4ProtocolManagerUtil::GetNextBackOffInterval(
      &gethash_error_count_, &gethash_back_off_mult_);
  next_gethash_time_ = now + next;
}

GURL V4GetHashProtocolManager::GetHashUrl(const std::string& req_base64) const {
  return V4ProtocolManagerUtil::GetRequestUrl(req_base64, "encodedFullHashes",
                                              config_);
}


}  // namespace safe_browsing
