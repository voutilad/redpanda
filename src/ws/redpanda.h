//
// Created by dv on 6/26/23.
//

#pragma once
#include "kafka/client/client.h"
#include "kafka/client/configuration.h"
#include "net/unresolved_address.h"

#include <seastar/core/shared_ptr.hh>

#include <memory>
#include <ostream>

namespace ss = seastar;
namespace kc = kafka::client;

namespace wsrp {

struct record {
    iobuf key{};
    iobuf value{};

    record() noexcept
      : key{}
      , value{} {}

    record(iobuf&& key, iobuf&& value) noexcept
      : key(std::move(key))
      , value(std::move(value)) {}

    record(
      ss::temporary_buffer<char>&& key_buf,
      ss::temporary_buffer<char>&& value_buf) noexcept
      : key{}
      , value{} {
        key.append(std::move(key_buf));
        value.append(std::move(value_buf));
    }

    friend std::ostream& operator<<(std::ostream& os, const record& record) {
        os << "{key: " << record.key << ", value: " << record.value << "}";
        return os;
    }
};

struct produce_result {
    std::optional<model::topic_partition> tp;
    size_t cnt{0};

    produce_result()
      : tp(std::nullopt) {}

    friend std::ostream&
    operator<<(std::ostream& os, const produce_result& produceResult) {
        os << "{tp: " << produceResult.tp << " cnt: " << produceResult.cnt
           << "}";
        return os;
    }
};

const int DEFAULT_BROKER_PORT = 9092;

/// \brief Decompose a std::string of comma separated host:ports into a vector
/// of host:ports.
std::vector<std::string> parse_brokers(std::string s) noexcept;

/// \brief Decompose a vector of hostname:ip strings into a vector of addresses
std::vector<net::unresolved_address>
parse_addresses(std::vector<std::string>& brokers) noexcept;

/// \brief Make a simple config object for bootstrapping a Redpanda client
YAML::Node make_config(const std::vector<net::unresolved_address>& seeds);

// A queue consumer that producers to Redpanda.
class redpanda {
    std::vector<net::unresolved_address> _seeds;
    kc::configuration _cfg{};
    ss::lw_shared_ptr<kc::client> _client;

public:
    redpanda(const std::vector<net::unresolved_address>& seeds)
      : _seeds(seeds) {
        _cfg.brokers.set_value(_seeds);
        _client = ss::make_lw_shared<kc::client>(
          config::to_yaml(_cfg, config::redact_secrets::no));
    }

    ss::future<bool> is_connected();
    ss::future<> connect();
    ss::future<> disconnect();
    ss::future<produce_result>
    produce(const model::topic, std::vector<record>&&);
};

} // namespace wsrp
