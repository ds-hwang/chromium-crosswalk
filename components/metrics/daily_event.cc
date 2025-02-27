// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/daily_event.h"

#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

enum IntervalType {
  FIRST_RUN,
  DAY_ELAPSED,
  CLOCK_CHANGED,
  NUM_INTERVAL_TYPES
};

void RecordIntervalTypeHistogram(const std::string& histogram_name,
                                 IntervalType type) {
  base::Histogram::FactoryGet(
      histogram_name,
      1,
      NUM_INTERVAL_TYPES,
      NUM_INTERVAL_TYPES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)->Add(type);
}

}  // namespace

DailyEvent::Observer::Observer() {
}

DailyEvent::Observer::~Observer() {
}

DailyEvent::DailyEvent(PrefService* pref_service,
                       const char* pref_name,
                       const std::string& histogram_name)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      histogram_name_(histogram_name) {
}

DailyEvent::~DailyEvent() {
}

// static
void DailyEvent::RegisterPref(PrefRegistrySimple* registry,
                              const char* pref_name) {
  registry->RegisterInt64Pref(pref_name, base::Time().ToInternalValue());
}

void DailyEvent::AddObserver(scoped_ptr<DailyEvent::Observer> observer) {
  DVLOG(2) << "DailyEvent observer added.";
  DCHECK(last_fired_.is_null());
  observers_.push_back(std::move(observer));
}

void DailyEvent::CheckInterval() {
  base::Time now = base::Time::Now();
  if (last_fired_.is_null()) {
    // The first time we call CheckInterval, we read the time stored in prefs.
    last_fired_ = base::Time::FromInternalValue(
        pref_service_->GetInt64(pref_name_));
    DVLOG(1) << "DailyEvent time loaded: "
             << base::TimeFormatShortDateAndTime(last_fired_);
    if (last_fired_.is_null()) {
      DVLOG(1) << "DailyEvent first run.";
      RecordIntervalTypeHistogram(histogram_name_, FIRST_RUN);
      OnInterval(now);
      return;
    }
  }
  int days_elapsed = (now - last_fired_).InDays();
  if (days_elapsed >= 1) {
    DVLOG(1) << "DailyEvent day elapsed.";
    RecordIntervalTypeHistogram(histogram_name_, DAY_ELAPSED);
    OnInterval(now);
  } else if (days_elapsed <= -1) {
    // The "last fired" time is more than a day in the future, so the clock
    // must have been changed.
    DVLOG(1) << "DailyEvent clock change detected.";
    RecordIntervalTypeHistogram(histogram_name_, CLOCK_CHANGED);
    OnInterval(now);
  }
}

void DailyEvent::OnInterval(base::Time now) {
  DCHECK(!now.is_null());
  last_fired_ = now;
  pref_service_->SetInt64(pref_name_, last_fired_.ToInternalValue());

  // Notify all observers
  for (ScopedVector<DailyEvent::Observer>::iterator it = observers_.begin();
       it != observers_.end();
       ++it) {
    (*it)->OnDailyEvent();
  }
}

}  // namespace metrics
