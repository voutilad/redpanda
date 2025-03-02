/*
 * Copyright 2023 Redpanda Data, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once

#include "base/seastarx.h"
#include "model/fundamental.h"
#include "model/metadata.h"
#include "model/namespace.h"
#include "model/record.h"
#include "serde/envelope.h"
#include "serde/rw/variant.h"
#include "utils/named_type.h"

#include <seastar/core/chunked_fifo.hh>
#include <seastar/core/sharded.hh>
#include <seastar/core/sstring.hh>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include <cstdint>
#include <memory>
#include <type_traits>

namespace model {

/**
 * Wasm binaries are large and can be fetched from different cores, so wrap them
 * in foreign_ptr over doing copies.
 */
using wasm_binary_iobuf
  = named_type<ss::foreign_ptr<std::unique_ptr<iobuf>>, struct wasm_binary>;

/** Share the underlying iobuf. */
wasm_binary_iobuf share_wasm_binary(const wasm_binary_iobuf&);

/** Serde support for wasm_binary_iobuf - it has the same format as iobuf. */
void tag_invoke(
  serde::tag_t<serde::read_tag>,
  iobuf_parser& in,
  wasm_binary_iobuf& t,
  std::size_t const bytes_left_limit);

/** Serde support for wasm_binary_iobuf - it has the same format as iobuf. */
void tag_invoke(
  serde::tag_t<serde::write_tag>, iobuf& out, wasm_binary_iobuf t);

/**
 * An ID for a transform, these get allocated globally, to allow for users to
 * re-use names of transforms.
 */
using transform_id = named_type<int64_t, struct transform_id_tag>;
/**
 * The name of a transform, which is the user defined.
 *
 * Generally you should only use names when surfacing information to a user,
 * otherwise the ID should be used, especially if the information is persisted.
 */
using transform_name = named_type<ss::sstring, struct transform_name_tag>;

using transform_name_view
  = named_type<std::string_view, struct transform_name_view_tag>;

/**
 * Whether a transform is or should be paused (i.e. stopped but not removed from
 * the system).
 */
using is_transform_paused = ss::bool_class<struct is_paused_tag>;

/**
 * The options related to the offset at which transforms are at.
 *
 * Currently, this struct only supports specifying an initial position, but in
 * the future it may be expanded to support resetting the offset at which
 * transforms are at.
 */
struct transform_offset_options
  : serde::envelope<
      transform_offset_options,
      serde::version<0>,
      serde::compat_version<0>> {
    // Signifies that a transform should start at the latest offset available.
    //
    // This is the default, but is expected to only be used for legacy deployed
    // transforms. New deployed transforms should start their offsets at a fixed
    // point from a deploy.
    struct latest_offset
      : serde::
          envelope<latest_offset, serde::version<0>, serde::compat_version<0>> {
        bool operator==(const latest_offset&) const = default;
    };
    // A transform can either start at the latest offset or at a timestamp.
    //
    // When a timestamp is used, a timequery is used to resolve the offset for
    // each partition.
    serde::variant<latest_offset, model::timestamp> position;

    bool operator==(const transform_offset_options&) const = default;

    friend std::ostream&
    operator<<(std::ostream&, const transform_offset_options&);

    auto serde_fields() { return std::tie(position); }
};

/**
 * Metadata for a WebAssembly powered data transforms.
 */
struct transform_metadata
  : serde::envelope<
      transform_metadata,
      serde::version<2>,
      serde::compat_version<0>> {
    // The user specified name of the transform.
    transform_name name;
    // The input topic being transformed.
    model::topic_namespace input_topic;
    // Right now we force there is only one, but we're mostly setup for
    // multiple. These are also validated to be unique, but we use a vector
    // here to preserve the user specified order (which is important as it
    // will be how the ABI boundary specifies which output topic to write too).
    std::vector<model::topic_namespace> output_topics;
    // The user specified environment variable configuration.
    absl::flat_hash_map<ss::sstring, ss::sstring> environment;
    // Each transform revision has a UUID, which is the key for the wasm_binary
    // topic and can be used to uniquely identify this revision.
    uuid_t uuid;
    // The offset of the committed WASM source in the wasm_binary topic.
    model::offset source_ptr;
    // The options related to the offset that the transform processor.
    transform_offset_options offset_options;

    model::is_transform_paused paused{false};

    friend bool operator==(const transform_metadata&, const transform_metadata&)
      = default;

    friend std::ostream& operator<<(std::ostream&, const transform_metadata&);

    auto serde_fields() {
        return std::tie(
          name,
          input_topic,
          output_topics,
          environment,
          uuid,
          source_ptr,
          offset_options,
          paused);
    }
};

// A patch update for transform metadata.
//
// This is used by the Admin API handler for `PUT /v1/transform/{name}/meta`
// See `redpanda/admin/transform.cc` or `redpanda/admin/api-doc/transform.json`
// for detail.
struct transform_metadata_patch {
    // This has PUT semantics, such that the existing env values will be
    // completely overwritten by the contents of this map.
    std::optional<absl::flat_hash_map<ss::sstring, ss::sstring>> env;
    // Desired paused state for the transform
    std::optional<is_transform_paused> paused;
};

using output_topic_index = named_type<uint32_t, struct output_topic_index_tag>;

// key / value types used to track consumption offsets by transforms.
struct transform_offsets_key
  : serde::envelope<
      transform_offsets_key,
      serde::version<1>,
      serde::compat_version<0>> {
    using rpc_adl_exempt = std::true_type;
    transform_id id;
    // id of the partition from transform's input/source topic.
    partition_id partition;
    // The index of the output topic within the transform metadata.
    output_topic_index output_topic;

    auto operator<=>(const transform_offsets_key&) const = default;

    friend std::ostream&
    operator<<(std::ostream&, const transform_offsets_key&);

    template<typename H>
    friend H AbslHashValue(H h, const transform_offsets_key& k) {
        return H::combine(
          std::move(h), k.id(), k.partition(), k.output_topic());
    }

    void serde_read(iobuf_parser& in, const serde::header& h);
    void serde_write(iobuf& out) const;
};

struct transform_offsets_value
  : serde::envelope<
      transform_offsets_value,
      serde::version<0>,
      serde::compat_version<0>> {
    kafka::offset offset;

    friend std::ostream&
    operator<<(std::ostream&, const transform_offsets_value&);

    auto serde_fields() { return std::tie(offset); }
};

using transform_offsets_map
  = absl::btree_map<transform_offsets_key, transform_offsets_value>;

/**
 * A flattened entry of transorm_offsets_map to return to the admin API.
 */
struct transform_committed_offset {
    transform_name name;
    partition_id partition;
    kafka::offset offset;
};

inline const model::topic transform_offsets_topic("transform_offsets");
inline const model::topic_namespace transform_offsets_nt(
  model::kafka_internal_namespace, transform_offsets_topic);

/**
 * A (possibly inconsistent) snapshot of a transform.
 *
 * We capture the metadata associated with the transform for as well as the
 * state for each processor.
 */
struct transform_report
  : serde::
      envelope<transform_report, serde::version<0>, serde::compat_version<0>> {
    struct processor
      : serde::
          envelope<processor, serde::version<0>, serde::compat_version<0>> {
        enum class state : uint8_t { unknown, inactive, running, errored };
        model::partition_id id;
        state status;
        model::node_id node;
        int64_t lag;
        friend bool operator==(const processor&, const processor&) = default;

        friend std::ostream& operator<<(std::ostream&, const processor&);

        auto serde_fields() { return std::tie(id, status, node, lag); }
    };

    transform_report() = default;
    explicit transform_report(transform_metadata meta);
    transform_report(
      transform_metadata meta, absl::btree_map<model::partition_id, processor>);

    // The overall metadata for a transform.
    transform_metadata metadata;

    // The state of each processor of a transform.
    //
    // Currently, there should be a single processor for each partition on the
    // input topic.
    absl::btree_map<model::partition_id, processor> processors;

    friend bool operator==(const transform_report&, const transform_report&)
      = default;

    auto serde_fields() { return std::tie(metadata, processors); }

    // Add a processor's report to the overall transform's report.
    void add(processor);
};

std::string_view
processor_state_to_string(transform_report::processor::state state);

std::ostream&
operator<<(std::ostream& os, transform_report::processor::state s);

/**
 * A cluster wide view of all the currently running transforms.
 */
struct cluster_transform_report
  : serde::envelope<
      cluster_transform_report,
      serde::version<0>,
      serde::compat_version<0>> {
    absl::btree_map<model::transform_id, transform_report> transforms;

    friend bool
    operator==(const cluster_transform_report&, const cluster_transform_report&)
      = default;

    auto serde_fields() { return std::tie(transforms); }

    // Add a processor's report for a single transform to this overall report.
    void
    add(transform_id, const transform_metadata&, transform_report::processor);

    // Merge cluster views of transforms into this report.
    //
    // This is useful for aggregating multiple node's reports into a single
    // report.
    void merge(const cluster_transform_report&);
};

/**
 * The output of a user's transformed function.
 *
 * It is a buffer formatted with the "payload" of a record using Kafka's wire
 * format. Specifically the following fields are included:
 *
 * keyLength: varint
 * key: byte[]
 * valueLen: varint
 * value: byte[]
 * Headers => [Header]
 *
 * Where Header is:
 *
 * headerKeyLength: varint
 * headerKey: String
 * headerValueLength: varint
 * Value: byte[]
 *
 * See: https://kafka.apache.org/documentation/#record for more information.
 *
 */
class transformed_data {
public:
    /**
     * Create a transformed record - validating the format is correct.
     */
    static std::optional<transformed_data> create_validated(iobuf);

    /**
     * Create a transformed record from a record, generally used in testing.
     */
    static transformed_data from_record(record);

    /**
     * Create a batch from transformed_data.
     */
    static record_batch
      make_batch(timestamp, ss::chunked_fifo<transformed_data>);

    /**
     * Generate a serialized record from the following metadata.
     */
    iobuf to_serialized_record(
      record_attributes, int64_t timestamp_delta, int32_t offset_delta) &&;

    /**
     * The memory usage for this struct and it's data.
     */
    size_t memory_usage() const;

    bool operator==(const transformed_data&) const = default;

    /**
     * Explicitly make a copy of this transformed data.
     */
    transformed_data copy() const;

private:
    explicit transformed_data(iobuf d);

    iobuf _data;
};

} // namespace model
