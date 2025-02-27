# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Setting these two variables allows other targets to use the
    # sync_proto_sources variable as the list of sync protocol buffer files.
    'sync_proto_sources_dir': 'protocol',
    'sync_proto_sources': [
      '<@(sync_proto_source_paths)',
    ],
  },

  'includes': [
    'protocol/protocol.gypi',
    'sync_android.gypi',
    'sync_tests.gypi',
  ],

  'targets': [
    # This target will add '-lsync_core' and '-lsync_proto' to the link line of
    # targets that depend on it.  Anything that depends on sync code should
    # declare a dependency on this target.
    {
      'target_name': 'sync',
      'type': 'none',
      'dependencies': [
        'sync_core',
        'sync_proto',
      ],
      'export_dependent_settings': [
        'sync_proto',
      ],
    },

    # Contains everything related to sync implementation that does not depend
    # on chrome/ or components/.  Do not depend on this directly.  Depend on
    # the 'sync' target to get the sync protobufs, too.
    {
      'target_name': 'sync_core',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'SYNC_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../crypto/crypto.gyp:crypto',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/zlib/zlib.gyp:zlib',
        '../url/url.gyp:url_lib',
        'attachment_store_proto',
        'sync_proto',
      ],
      'export_dependent_settings': [
        'sync_proto',
      ],
      'sources': [
        'api/attachments/attachment.cc',
        'api/attachments/attachment.h',
        'api/attachments/attachment_id.cc',
        'api/attachments/attachment_id.h',
        'api/attachments/attachment_metadata.cc',
        'api/attachments/attachment_metadata.h',
        'api/attachments/attachment_store.cc',
        'api/attachments/attachment_store.h',
        'api/attachments/attachment_store_backend.cc',
        'api/attachments/attachment_store_backend.h',
        'api/data_batch.h',
        'api/entity_change.cc',
        'api/entity_change.h',
        'api/entity_data.cc',
        'api/entity_data.h',
        'api/metadata_batch.cc',
        'api/metadata_batch.h',
        'api/metadata_change_list.h',
        'api/model_type_change_processor.cc',
        'api/model_type_change_processor.h',
        'api/model_type_service.cc',
        'api/model_type_service.h',
        'api/model_type_store.cc',
        'api/model_type_store.h',
        'api/string_ordinal.h',
        'api/sync_change.cc',
        'api/sync_change.h',
        'api/sync_change_processor.cc',
        'api/sync_change_processor.h',
        'api/sync_data.cc',
        'api/sync_data.h',
        'api/sync_error.cc',
        'api/sync_error.h',
        'api/sync_error_factory.cc',
        'api/sync_error_factory.h',
        'api/sync_merge_result.cc',
        'api/sync_merge_result.h',
        'api/syncable_service.cc',
        'api/syncable_service.h',
        'api/time.h',
        'base/sync_export.h',
        'engine/all_status.cc',
        'engine/all_status.h',
        'engine/apply_control_data_updates.cc',
        'engine/apply_control_data_updates.h',
        'engine/backoff_delay_provider.cc',
        'engine/backoff_delay_provider.h',
        'engine/clear_server_data.cc',
        'engine/clear_server_data.h',
        'engine/commit.cc',
        'engine/commit.h',
        'engine/commit_contribution.cc',
        'engine/commit_contribution.h',
        'engine/commit_contributor.cc',
        'engine/commit_contributor.h',
        'engine/commit_processor.cc',
        'engine/commit_processor.h',
        'engine/commit_queue.cc',
        'engine/commit_queue.h',
        'engine/commit_util.cc',
        'engine/commit_util.h',
        'engine/conflict_resolver.cc',
        'engine/conflict_resolver.h',
        'engine/conflict_util.cc',
        'engine/conflict_util.h',
        'engine/directory_commit_contribution.cc',
        'engine/directory_commit_contribution.h',
        'engine/directory_commit_contributor.cc',
        'engine/directory_commit_contributor.h',
        'engine/directory_update_handler.cc',
        'engine/directory_update_handler.h',
        'engine/entity_tracker.cc',
        'engine/entity_tracker.h',
        'engine/get_commit_ids.cc',
        'engine/get_commit_ids.h',
        'engine/get_updates_delegate.cc',
        'engine/get_updates_delegate.h',
        'engine/get_updates_processor.cc',
        'engine/get_updates_processor.h',
        'engine/model_type_worker.cc',
        'engine/model_type_worker.h',
        'engine/net/server_connection_manager.cc',
        'engine/net/server_connection_manager.h',
        'engine/net/url_translator.cc',
        'engine/net/url_translator.h',
        'engine/non_blocking_type_commit_contribution.cc',
        'engine/non_blocking_type_commit_contribution.h',
        'engine/nudge_handler.cc',
        'engine/nudge_handler.h',
        'engine/nudge_source.cc',
        'engine/nudge_source.h',
        'engine/process_updates_util.cc',
        'engine/process_updates_util.h',
        'engine/sync_cycle_event.cc',
        'engine/sync_cycle_event.h',
        'engine/sync_engine_event_listener.cc',
        'engine/sync_engine_event_listener.h',
        'engine/sync_scheduler.cc',
        'engine/sync_scheduler.h',
        'engine/sync_scheduler_impl.cc',
        'engine/sync_scheduler_impl.h',
        'engine/syncer.cc',
        'engine/syncer.h',
        'engine/syncer_proto_util.cc',
        'engine/syncer_proto_util.h',
        'engine/syncer_types.h',
        'engine/syncer_util.cc',
        'engine/syncer_util.h',
        'engine/traffic_logger.cc',
        'engine/traffic_logger.h',
        'engine/update_applicator.cc',
        'engine/update_applicator.h',
        'engine/update_handler.cc',
        'engine/update_handler.h',
        'internal_api/activation_context.cc',
        'internal_api/attachments/attachment_downloader.cc',
        'internal_api/attachments/attachment_downloader_impl.cc',
        'internal_api/attachments/attachment_service.cc',
        'internal_api/attachments/attachment_service_impl.cc',
        'internal_api/attachments/attachment_service_proxy.cc',
        'internal_api/attachments/attachment_service_proxy_for_test.cc',
        'internal_api/attachments/attachment_store_frontend.cc',
        'internal_api/attachments/attachment_uploader.cc',
        'internal_api/attachments/attachment_uploader_impl.cc',
        'internal_api/attachments/attachment_util.cc',
        'internal_api/attachments/fake_attachment_downloader.cc',
        'internal_api/attachments/fake_attachment_uploader.cc',
        'internal_api/attachments/in_memory_attachment_store.cc',
        'internal_api/attachments/on_disk_attachment_store.cc',
        'internal_api/attachments/task_queue.cc',
        'internal_api/base_node.cc',
        'internal_api/base_transaction.cc',
        'internal_api/change_record.cc',
        'internal_api/change_reorder_buffer.cc',
        'internal_api/change_reorder_buffer.h',
        'internal_api/debug_info_event_listener.cc',
        'internal_api/debug_info_event_listener.h',
        'internal_api/delete_journal.cc',
        'internal_api/events/clear_server_data_request_event.cc',
        'internal_api/events/clear_server_data_response_event.cc',
        'internal_api/events/commit_request_event.cc',
        'internal_api/events/commit_response_event.cc',
        'internal_api/events/configure_get_updates_request_event.cc',
        'internal_api/events/get_updates_response_event.cc',
        'internal_api/events/normal_get_updates_request_event.cc',
        'internal_api/events/poll_get_updates_request_event.cc',
        'internal_api/events/protocol_event.cc',
        'internal_api/http_bridge.cc',
        'internal_api/http_bridge_network_resources.cc',
        'internal_api/internal_components_factory_impl.cc',
        'internal_api/js_mutation_event_observer.cc',
        'internal_api/js_mutation_event_observer.h',
        'internal_api/js_sync_encryption_handler_observer.cc',
        'internal_api/js_sync_encryption_handler_observer.h',
        'internal_api/js_sync_manager_observer.cc',
        'internal_api/js_sync_manager_observer.h',
        'internal_api/model_type_entity.cc',
        'internal_api/model_type_store_backend.cc',
        'internal_api/model_type_store_impl.cc',
        'internal_api/protocol_event_buffer.cc',
        'internal_api/protocol_event_buffer.h',
        'internal_api/public/activation_context.h',
        'internal_api/public/attachments/attachment_downloader.h',
        'internal_api/public/attachments/attachment_downloader_impl.h',
        'internal_api/public/attachments/attachment_service.h',
        'internal_api/public/attachments/attachment_service_impl.h',
        'internal_api/public/attachments/attachment_service_proxy.h',
        'internal_api/public/attachments/attachment_service_proxy_for_test.h',
        'internal_api/public/attachments/attachment_store_frontend.h',
        'internal_api/public/attachments/attachment_uploader.h',
        'internal_api/public/attachments/attachment_uploader_impl.h',
        'internal_api/public/attachments/attachment_util.h',
        'internal_api/public/attachments/fake_attachment_downloader.h',
        'internal_api/public/attachments/fake_attachment_uploader.h',
        'internal_api/public/attachments/in_memory_attachment_store.h',
        'internal_api/public/attachments/on_disk_attachment_store.h',
        'internal_api/public/attachments/task_queue.h',
        'internal_api/public/base/attachment_id_proto.cc',
        'internal_api/public/base/attachment_id_proto.h',
        'internal_api/public/base/cancelation_observer.cc',
        'internal_api/public/base/cancelation_observer.h',
        'internal_api/public/base/cancelation_signal.cc',
        'internal_api/public/base/cancelation_signal.h',
        'internal_api/public/base/enum_set.h',
        'internal_api/public/base/enum_set.h',
        'internal_api/public/base/invalidation_interface.cc',
        'internal_api/public/base/invalidation_interface.h',
        'internal_api/public/base/model_type.h',
        'internal_api/public/base/node_ordinal.cc',
        'internal_api/public/base/node_ordinal.h',
        'internal_api/public/base/ordinal.h',
        'internal_api/public/base/progress_marker_map.cc',
        'internal_api/public/base/progress_marker_map.h',
        'internal_api/public/base/stop_source.h',
        'internal_api/public/base/unique_position.cc',
        'internal_api/public/base/unique_position.h',
        'internal_api/public/base_node.h',
        'internal_api/public/base_transaction.h',
        'internal_api/public/change_record.h',
        'internal_api/public/configure_reason.h',
        'internal_api/public/connection_status.h',
        'internal_api/public/data_batch_impl.h',
        'internal_api/public/data_batch_impl.cc',
        'internal_api/public/data_type_association_stats.cc',
        'internal_api/public/data_type_association_stats.h',
        'internal_api/public/data_type_debug_info_listener.cc',
        'internal_api/public/data_type_debug_info_listener.h',
        'internal_api/public/delete_journal.h',
        'internal_api/public/engine/model_safe_worker.cc',
        'internal_api/public/engine/model_safe_worker.h',
        'internal_api/public/engine/passive_model_worker.cc',
        'internal_api/public/engine/passive_model_worker.h',
        'internal_api/public/engine/polling_constants.cc',
        'internal_api/public/engine/polling_constants.h',
        'internal_api/public/engine/sync_status.cc',
        'internal_api/public/engine/sync_status.h',
        'internal_api/public/events/clear_server_data_request_event.h',
        'internal_api/public/events/clear_server_data_response_event.h',
        'internal_api/public/events/commit_request_event.h',
        'internal_api/public/events/commit_response_event.h',
        'internal_api/public/events/configure_get_updates_request_event.h',
        'internal_api/public/events/get_updates_response_event.h',
        'internal_api/public/events/normal_get_updates_request_event.h',
        'internal_api/public/events/poll_get_updates_request_event.h',
        'internal_api/public/events/protocol_event.h',
        'internal_api/public/http_bridge.h',
        'internal_api/public/http_bridge_network_resources.h',
        'internal_api/public/http_post_provider_factory.h',
        'internal_api/public/http_post_provider_interface.h',
        'internal_api/public/internal_components_factory.h',
        'internal_api/public/internal_components_factory_impl.h',
        'internal_api/public/model_type_entity.h',
        'internal_api/public/model_type_processor.cc',
        'internal_api/public/model_type_processor.h',
        'internal_api/public/model_type_store_backend.h',
        'internal_api/public/model_type_store_impl.h',
        'internal_api/public/network_resources.h',
        'internal_api/public/non_blocking_sync_common.cc',
        'internal_api/public/non_blocking_sync_common.h',
        'internal_api/public/read_node.h',
        'internal_api/public/read_transaction.h',
        'internal_api/public/sessions/commit_counters.cc',
        'internal_api/public/sessions/commit_counters.h',
        'internal_api/public/sessions/model_neutral_state.cc',
        'internal_api/public/sessions/model_neutral_state.h',
        'internal_api/public/sessions/status_counters.cc',
        'internal_api/public/sessions/status_counters.h',
        'internal_api/public/sessions/sync_session_snapshot.cc',
        'internal_api/public/sessions/sync_session_snapshot.h',
        'internal_api/public/sessions/type_debug_info_observer.cc',
        'internal_api/public/sessions/type_debug_info_observer.h',
        'internal_api/public/sessions/update_counters.cc',
        'internal_api/public/sessions/update_counters.h',
        'internal_api/public/shared_model_type_processor.h',
        'internal_api/public/shutdown_reason.h',
        'internal_api/public/simple_metadata_change_list.cc',
        'internal_api/public/simple_metadata_change_list.h',
        'internal_api/public/sync_auth_provider.h',
        'internal_api/public/sync_context.h',
        'internal_api/public/sync_context_proxy.h',
        'internal_api/public/sync_encryption_handler.cc',
        'internal_api/public/sync_encryption_handler.h',
        'internal_api/public/sync_manager.cc',
        'internal_api/public/sync_manager.h',
        'internal_api/public/sync_manager_factory.h',
        'internal_api/public/user_share.h',
        'internal_api/public/util/experiments.h',
        'internal_api/public/util/immutable.h',
        'internal_api/public/util/proto_value_ptr.h',
        'internal_api/public/util/sync_db_util.h',
        'internal_api/public/util/sync_string_conversions.cc',
        'internal_api/public/util/sync_string_conversions.h',
        'internal_api/public/util/syncer_error.cc',
        'internal_api/public/util/syncer_error.h',
        'internal_api/public/util/unrecoverable_error_handler.h',
        'internal_api/public/util/unrecoverable_error_info.cc',
        'internal_api/public/util/unrecoverable_error_info.h',
        'internal_api/public/util/weak_handle.cc',
        'internal_api/public/util/weak_handle.h',
        'internal_api/public/write_node.h',
        'internal_api/public/write_transaction.h',
        'internal_api/read_node.cc',
        'internal_api/read_transaction.cc',
        'internal_api/shared_model_type_processor.cc',
        'internal_api/sync_context.cc',
        'internal_api/sync_context_proxy.cc',
        'internal_api/sync_context_proxy_impl.cc',
        'internal_api/sync_context_proxy_impl.h',
        'internal_api/sync_db_util.cc',
        'internal_api/sync_encryption_handler_impl.cc',
        'internal_api/sync_encryption_handler_impl.h',
        'internal_api/sync_manager_factory.cc',
        'internal_api/sync_manager_impl.cc',
        'internal_api/sync_manager_impl.h',
        'internal_api/syncapi_internal.cc',
        'internal_api/syncapi_internal.h',
        'internal_api/syncapi_server_connection_manager.cc',
        'internal_api/syncapi_server_connection_manager.h',
        'internal_api/user_share.cc',
        'internal_api/write_node.cc',
        'internal_api/write_transaction.cc',
        'js/js_backend.h',
        'js/js_controller.h',
        'js/js_event_details.cc',
        'js/js_event_details.h',
        'js/js_event_handler.h',
        'js/sync_js_controller.cc',
        'js/sync_js_controller.h',
        'protocol/proto_enum_conversions.cc',
        'protocol/proto_enum_conversions.h',
        'protocol/proto_value_conversions.cc',
        'protocol/proto_value_conversions.h',
        'protocol/sync_protocol_error.cc',
        'protocol/sync_protocol_error.h',
        'sessions/data_type_tracker.cc',
        'sessions/data_type_tracker.h',
        'sessions/debug_info_getter.h',
        'sessions/directory_type_debug_info_emitter.cc',
        'sessions/directory_type_debug_info_emitter.h',
        'sessions/model_type_registry.cc',
        'sessions/model_type_registry.h',
        'sessions/nudge_tracker.cc',
        'sessions/nudge_tracker.h',
        'sessions/status_controller.cc',
        'sessions/status_controller.h',
        'sessions/sync_session.cc',
        'sessions/sync_session.h',
        'sessions/sync_session_context.cc',
        'sessions/sync_session_context.h',
        'syncable/dir_open_result.h',
        'syncable/directory.cc',
        'syncable/directory.h',
        'syncable/directory_backing_store.cc',
        'syncable/directory_backing_store.h',
        'syncable/directory_change_delegate.h',
        'syncable/entry.cc',
        'syncable/entry.h',
        'syncable/entry_kernel.cc',
        'syncable/entry_kernel.h',
        'syncable/in_memory_directory_backing_store.cc',
        'syncable/in_memory_directory_backing_store.h',
        'syncable/invalid_directory_backing_store.cc',
        'syncable/invalid_directory_backing_store.h',
        'syncable/metahandle_set.h',
        'syncable/model_neutral_mutable_entry.cc',
        'syncable/model_neutral_mutable_entry.h',
        'syncable/model_type.cc',
        'syncable/mutable_entry.cc',
        'syncable/mutable_entry.h',
        'syncable/nigori_handler.cc',
        'syncable/nigori_handler.h',
        'syncable/nigori_util.cc',
        'syncable/nigori_util.h',
        'syncable/on_disk_directory_backing_store.cc',
        'syncable/on_disk_directory_backing_store.h',
        'syncable/parent_child_index.cc',
        'syncable/parent_child_index.h',
        'syncable/scoped_kernel_lock.cc',
        'syncable/scoped_kernel_lock.h',
        'syncable/scoped_parent_child_index_updater.cc',
        'syncable/scoped_parent_child_index_updater.h',
        'syncable/syncable-inl.h',
        'syncable/syncable_base_transaction.cc',
        'syncable/syncable_base_transaction.h',
        'syncable/syncable_base_write_transaction.cc',
        'syncable/syncable_base_write_transaction.h',
        'syncable/syncable_changes_version.h',
        'syncable/syncable_columns.h',
        'syncable/syncable_delete_journal.cc',
        'syncable/syncable_delete_journal.h',
        'syncable/syncable_enum_conversions.cc',
        'syncable/syncable_enum_conversions.h',
        'syncable/syncable_id.cc',
        'syncable/syncable_id.h',
        'syncable/syncable_model_neutral_write_transaction.cc',
        'syncable/syncable_model_neutral_write_transaction.h',
        'syncable/syncable_proto_util.cc',
        'syncable/syncable_proto_util.h',
        'syncable/syncable_read_transaction.cc',
        'syncable/syncable_read_transaction.h',
        'syncable/syncable_util.cc',
        'syncable/syncable_util.h',
        'syncable/syncable_write_transaction.cc',
        'syncable/syncable_write_transaction.h',
        'syncable/transaction_observer.h',
        'syncable/write_transaction_info.cc',
        'syncable/write_transaction_info.h',
        'util/cryptographer.cc',
        'util/cryptographer.h',
        'util/data_type_histogram.cc',
        'util/data_type_histogram.h',
        'util/encryptor.h',
        'util/extensions_activity.cc',
        'util/extensions_activity.h',
        'util/get_session_name.cc',
        'util/get_session_name.h',
        'util/get_session_name_ios.h',
        'util/get_session_name_ios.mm',
        'util/get_session_name_linux.cc',
        'util/get_session_name_linux.h',
        'util/get_session_name_mac.h',
        'util/get_session_name_mac.mm',
        'util/get_session_name_win.cc',
        'util/get_session_name_win.h',
        'util/logging.cc',
        'util/logging.h',
        'util/nigori.cc',
        'util/nigori.h',
        'util/time.cc',
        'util/time.h',
      ],

      'conditions': [
        ['OS=="linux" and chromeos==1', {
          # Required by get_session_name.cc on Chrome OS.
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
            ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              # Required by get_session_name_mac.mm on Mac.
              '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            ]
          },
        }],
        ['OS=="android"', {
          'dependencies': [
            'sync_jni_headers',
          ],
          'sources': [
            'android/model_type_helper.cc',
            'android/model_type_helper.h',
            'android/sync_jni_registrar.cc',
            'android/sync_jni_registrar.h',
          ],
        }],
      ],
    },
    {
      # Contains sync protobuf definitions.  Do not depend on this directly.
      # Depend on the 'sync' target to get the relevant C++ code, too.
      #
      # GN version: //sync/protocol
      'target_name': 'sync_proto',
      'type': '<(component)',
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SYNC_PROTO_IMPLEMENTATION',
      ],
      'sources': [
        # When adding a new proto source file, add its path to the list defined
        # in sync/protocol/protocol.gypi.
        '<@(sync_proto_sources)',
      ],
      'variables': {
        'enable_wexit_time_destructors': 1,
        'proto_in_dir': './protocol',
        'proto_out_dir': 'sync/protocol',
        'cc_generator_options': 'dllexport_decl=SYNC_PROTO_EXPORT:',
        'cc_include': 'sync/protocol/sync_proto_export.h',
      },
      'includes': [
        '../build/protoc.gypi'
      ],
    },
    {
      # Contains attachment_store protobuf definitions.  Do not depend on this
      # directly.
      # Depend on the 'sync' target to get the relevant C++ code, too.
      #
      # GN version: //sync/internal_api/attachments/proto
      'target_name': 'attachment_store_proto',
      'type': 'static_library',
      'sources': [
        # NOTE: If you add a file to this list, also add it to
        # sync/internal_api/attachments/proto/BUILD.gn
        'internal_api/attachments/proto/attachment_store.proto',
      ],
      'variables': {
        'enable_wexit_time_destructors': 1,
        'proto_in_dir': 'internal_api/attachments/proto',
        'proto_out_dir': 'sync/internal_api/attachments/proto',
        'cc_generator_options': 'dllexport_decl=SYNC_EXPORT:',
        'cc_include': 'sync/base/sync_export.h',
      },
      'includes': [
        '../build/protoc.gypi'
      ],
      'defines': [
        'SYNC_IMPLEMENTATION'
      ],
    },
  ],
}
