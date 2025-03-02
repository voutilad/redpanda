// Copyright 2024 Redpanda Data, Inc.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.md
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0

#pragma once

#include "cluster/remote_topic_properties.h"
#include "model/compression.h"
#include "model/fundamental.h"
#include "model/metadata.h"
#include "model/timestamp.h"
#include "pandaproxy/schema_registry/subject_name_strategy.h"
#include "reflection/adl.h"
#include "storage/ntp_config.h"
#include "utils/tristate.h"

namespace cluster {

/**
 * Structure holding topic properties overrides, empty values will be replaced
 * with defaults
 */
struct topic_properties
  : serde::
      envelope<topic_properties, serde::version<8>, serde::compat_version<0>> {
    topic_properties() noexcept = default;
    topic_properties(
      std::optional<model::compression> compression,
      std::optional<model::cleanup_policy_bitflags> cleanup_policy_bitflags,
      std::optional<model::compaction_strategy> compaction_strategy,
      std::optional<model::timestamp_type> timestamp_type,
      std::optional<size_t> segment_size,
      tristate<size_t> retention_bytes,
      tristate<std::chrono::milliseconds> retention_duration,
      std::optional<bool> recovery,
      std::optional<model::shadow_indexing_mode> shadow_indexing,
      std::optional<bool> read_replica,
      std::optional<ss::sstring> read_replica_bucket,
      std::optional<remote_topic_properties> remote_topic_properties,
      std::optional<uint32_t> batch_max_bytes,
      tristate<size_t> retention_local_target_bytes,
      tristate<std::chrono::milliseconds> retention_local_target_ms,
      bool remote_delete,
      tristate<std::chrono::milliseconds> segment_ms,
      std::optional<bool> record_key_schema_id_validation,
      std::optional<bool> record_key_schema_id_validation_compat,
      std::optional<pandaproxy::schema_registry::subject_name_strategy>
        record_key_subject_name_strategy,
      std::optional<pandaproxy::schema_registry::subject_name_strategy>
        record_key_subject_name_strategy_compat,
      std::optional<bool> record_value_schema_id_validation,
      std::optional<bool> record_value_schema_id_validation_compat,
      std::optional<pandaproxy::schema_registry::subject_name_strategy>
        record_value_subject_name_strategy,
      std::optional<pandaproxy::schema_registry::subject_name_strategy>
        record_value_subject_name_strategy_compat,
      tristate<size_t> initial_retention_local_target_bytes,
      tristate<std::chrono::milliseconds> initial_retention_local_target_ms,
      std::optional<model::vcluster_id> mpx_virtual_cluster_id,
      std::optional<model::write_caching_mode> write_caching,
      std::optional<std::chrono::milliseconds> flush_ms,
      std::optional<size_t> flush_bytes)
      : compression(compression)
      , cleanup_policy_bitflags(cleanup_policy_bitflags)
      , compaction_strategy(compaction_strategy)
      , timestamp_type(timestamp_type)
      , segment_size(segment_size)
      , retention_bytes(retention_bytes)
      , retention_duration(retention_duration)
      , recovery(recovery)
      , shadow_indexing(shadow_indexing)
      , read_replica(read_replica)
      , read_replica_bucket(std::move(read_replica_bucket))
      , remote_topic_properties(remote_topic_properties)
      , batch_max_bytes(batch_max_bytes)
      , retention_local_target_bytes(retention_local_target_bytes)
      , retention_local_target_ms(retention_local_target_ms)
      , remote_delete(remote_delete)
      , segment_ms(segment_ms)
      , record_key_schema_id_validation(record_key_schema_id_validation)
      , record_key_schema_id_validation_compat(
          record_key_schema_id_validation_compat)
      , record_key_subject_name_strategy(record_key_subject_name_strategy)
      , record_key_subject_name_strategy_compat(
          record_key_subject_name_strategy_compat)
      , record_value_schema_id_validation(record_value_schema_id_validation)
      , record_value_schema_id_validation_compat(
          record_value_schema_id_validation_compat)
      , record_value_subject_name_strategy(record_value_subject_name_strategy)
      , record_value_subject_name_strategy_compat(
          record_value_subject_name_strategy_compat)
      , initial_retention_local_target_bytes(
          initial_retention_local_target_bytes)
      , initial_retention_local_target_ms(initial_retention_local_target_ms)
      , mpx_virtual_cluster_id(mpx_virtual_cluster_id)
      , write_caching(write_caching)
      , flush_ms(flush_ms)
      , flush_bytes(flush_bytes) {}

    std::optional<model::compression> compression;
    std::optional<model::cleanup_policy_bitflags> cleanup_policy_bitflags;
    std::optional<model::compaction_strategy> compaction_strategy;
    std::optional<model::timestamp_type> timestamp_type;
    std::optional<size_t> segment_size;
    tristate<size_t> retention_bytes{std::nullopt};
    tristate<std::chrono::milliseconds> retention_duration{std::nullopt};
    std::optional<bool> recovery;
    std::optional<model::shadow_indexing_mode> shadow_indexing;
    std::optional<bool> read_replica;
    std::optional<ss::sstring> read_replica_bucket;

    // Topic properties for a topic that already has remote data (e.g.
    // recovery topics).
    std::optional<remote_topic_properties> remote_topic_properties;

    std::optional<uint32_t> batch_max_bytes;
    tristate<size_t> retention_local_target_bytes{std::nullopt};
    tristate<std::chrono::milliseconds> retention_local_target_ms{std::nullopt};

    // Remote deletes are enabled by default in new tiered storage topics,
    // disabled by default in legacy topics during upgrade.
    // This is intentionally not an optional: all topics have a concrete value
    // one way or another.  There is no "use the cluster default".
    bool remote_delete{storage::ntp_config::default_remote_delete};

    tristate<std::chrono::milliseconds> segment_ms{std::nullopt};

    std::optional<bool> record_key_schema_id_validation;
    std::optional<bool> record_key_schema_id_validation_compat;
    std::optional<pandaproxy::schema_registry::subject_name_strategy>
      record_key_subject_name_strategy;
    std::optional<pandaproxy::schema_registry::subject_name_strategy>
      record_key_subject_name_strategy_compat;
    std::optional<bool> record_value_schema_id_validation;
    std::optional<bool> record_value_schema_id_validation_compat;
    std::optional<pandaproxy::schema_registry::subject_name_strategy>
      record_value_subject_name_strategy;
    std::optional<pandaproxy::schema_registry::subject_name_strategy>
      record_value_subject_name_strategy_compat;

    tristate<size_t> initial_retention_local_target_bytes{std::nullopt};
    tristate<std::chrono::milliseconds> initial_retention_local_target_ms{
      std::nullopt};
    std::optional<model::vcluster_id> mpx_virtual_cluster_id;
    std::optional<model::write_caching_mode> write_caching;
    std::optional<std::chrono::milliseconds> flush_ms;
    std::optional<size_t> flush_bytes;

    bool is_compacted() const;
    bool has_overrides() const;
    bool requires_remote_erase() const;

    storage::ntp_config::default_overrides get_ntp_cfg_overrides() const;

    friend std::ostream& operator<<(std::ostream&, const topic_properties&);
    auto serde_fields() {
        return std::tie(
          compression,
          cleanup_policy_bitflags,
          compaction_strategy,
          timestamp_type,
          segment_size,
          retention_bytes,
          retention_duration,
          recovery,
          shadow_indexing,
          read_replica,
          read_replica_bucket,
          remote_topic_properties,
          batch_max_bytes,
          retention_local_target_bytes,
          retention_local_target_ms,
          remote_delete,
          segment_ms,
          record_key_schema_id_validation,
          record_key_schema_id_validation_compat,
          record_key_subject_name_strategy,
          record_key_subject_name_strategy_compat,
          record_value_schema_id_validation,
          record_value_schema_id_validation_compat,
          record_value_subject_name_strategy,
          record_value_subject_name_strategy_compat,
          initial_retention_local_target_bytes,
          initial_retention_local_target_ms,
          mpx_virtual_cluster_id,
          write_caching,
          flush_ms,
          flush_bytes);
    }

    friend bool operator==(const topic_properties&, const topic_properties&)
      = default;
};

} // namespace cluster

namespace reflection {

template<>
struct adl<cluster::topic_properties> {
    void to(iobuf&, cluster::topic_properties&&);
    cluster::topic_properties from(iobuf_parser&);
};

} // namespace reflection
