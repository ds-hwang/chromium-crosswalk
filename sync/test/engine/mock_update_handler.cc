// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_update_handler.h"

#include <string>

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

MockUpdateHandler::MockUpdateHandler(ModelType type)
    : apply_updates_count_(0),
      passive_apply_updates_count_(0) {
  progress_marker_.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  const std::string& token_str =
      std::string("Mock token: ") + std::string(ModelTypeToString(type));
  progress_marker_.set_token(token_str);
}

MockUpdateHandler::~MockUpdateHandler() {}

void MockUpdateHandler::GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const {
  progress_marker->CopyFrom(progress_marker_);
}

void MockUpdateHandler::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  context->Clear();
}

SyncerError MockUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  progress_marker_.CopyFrom(progress_marker);
  return syncer::SYNCER_OK;
}

void MockUpdateHandler::ApplyUpdates(sessions::StatusController* status) {
  apply_updates_count_++;
}

void MockUpdateHandler::PassiveApplyUpdates(
    sessions::StatusController* status) {
  passive_apply_updates_count_++;
}

int MockUpdateHandler::GetApplyUpdatesCount() {
  return apply_updates_count_;
}

int MockUpdateHandler::GetPassiveApplyUpdatesCount() {
  return passive_apply_updates_count_;
}

}  // namespace syncer
