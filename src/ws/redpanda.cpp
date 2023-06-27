//
// Created by dv on 6/26/23.
//
#include "redpanda.h"

#include "bytes/iobuf.h"
#include "logger.h"

#include <boost/algorithm/string.hpp>

#include <utility>

namespace kc = kafka::client;

namespace wsrp {

std::vector<std::string> parse_brokers(std::string s) noexcept {
    std::vector<std::string> brokers{};
    try {
        boost::split(brokers, s, boost::algorithm::is_any_of(","));
    } catch (...) {
        // NOP
    }
    return std::move(brokers);
}

std::vector<net::unresolved_address>
parse_addresses(std::vector<std::string>& brokers) noexcept {
    std::vector<net::unresolved_address> addrs{};

    try {
        for (const auto& broker : brokers) {
            std::string host{};
            uint16_t port = DEFAULT_BROKER_PORT;

            const size_t colon = broker.find_first_of(":");
            if (colon != std::string::npos) {
                // xxx catch
                auto len = broker.size() - colon - 1;
                auto sub = broker.substr(colon + 1, len);
                auto val = std::stoul(sub);
                if (val > std::numeric_limits<uint16_t>::max()) {
                    throw std::runtime_error("port out of range");
                }
                port = (uint16_t)val;
            }
            host = broker.substr(0, colon);
            addrs.emplace_back(host, port);
        }
    } catch (const std::exception& e) {
        ws_log.warn("failed to parse brokers: {}", e.what());
        return std::move(std::vector<net::unresolved_address>{});
    }

    return std::move(addrs);
}

YAML::Node make_config(const std::vector<net::unresolved_address> seeds) {
    kc::configuration cfg{};
    cfg.brokers.set_value(seeds);
    return config::to_yaml(cfg, config::redact_secrets::no);
}

ss::future<bool> redpanda::is_connected() { return _client.is_connected(); }
ss::future<> redpanda::connect() { return _client.connect(); }

ss::future<produce_result>
redpanda::produce(const model::topic topic, std::vector<record>&& records) {
    // ignore custom partitioner for now...
    std::vector<kc::record_essence> essences;
    // for (auto& record : std::move(records)) {
    kc::record_essence e{};
    e.value = iobuf();
    e.value->append("crap", 4);
    e.partition_id = model::partition_id{0};
    // e.key = std::move(record.key);
    // e.value = std::move(record.value);
    essences.emplace_back(std::move(e));
    //}

    ws_log.info("producing {} records...", essences.size());
    return _client.produce_records(topic, std::move(essences))
      .then([](kafka::produce_response res) {
          // XXX ignore errors for now
          ws_log.info("produce result: {}", res);
          produce_result pr{};
          pr.cnt = 1;
          return ss::make_ready_future<produce_result>(std::move(pr));
      });
    return ss::make_ready_future<produce_result>(produce_result{});
}

} // namespace wsrp
