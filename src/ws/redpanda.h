//
// Created by dv on 6/26/23.
//

#pragma once
#include "kafka/client/client.h"
#include "kafka/client/configuration.h"
#include "net/unresolved_address.h"

#include <memory>
#include <ostream>

namespace ss = seastar;
namespace kc = kafka::client;

namespace wsrp {

struct record {
    iobuf key;
    iobuf value;

    record(iobuf&& key, iobuf&& value) noexcept
      : key(std::move(key))
      , value(std::move(value)) {}
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
YAML::Node make_config(const std::vector<net::unresolved_address> seeds);

// A queue consumer that producers to Redpanda.
class redpanda {
    const YAML::Node _config;
    kc::client _client;

public:
    redpanda(const YAML::Node config)
      : _config(config)
      , _client{config} {}

    ss::future<bool> is_connected();
    ss::future<> connect();
    ss::future<produce_result>
    produce(const model::topic, std::vector<record>&&);
};

} // namespace wsrp
