#include "bytes/iobuf.h"
#include "kafka/client/client.h"
#include "kafka/client/configuration.h"
#include "kafka/client/types.h"
#include "kafka/protocol/fetch.h"

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/thread.hh>
#include <seastar/util/log.hh>
#include <seastar/websocket/server.hh>

#include <boost/algorithm/string.hpp>

namespace kc = kafka::client;
namespace ss = seastar;
namespace ws = ss::experimental::websocket;
namespace po = boost::program_options;

static ss::logger ws_log{"ws"};

static std::vector<net::unresolved_address>
parse_brokers(std::vector<std::string>& brokers) noexcept {
    std::vector<net::unresolved_address> addrs{};

    try {
        for (auto broker : brokers) {
            std::string host{};
            uint16_t port;

            size_t colon = broker.find_first_of(":");
            if (colon == std::string::npos) {
                port = 9092;
            } else {
                // xxx catch
                auto len = broker.size() - colon - 1;
                auto sub = broker.substr(colon + 1, len);
                auto val = std::stoul(sub);
                if (val > 65536) {
                    throw std::runtime_error("port out of range");
                }
                port = (uint16_t)val;
            }
            host = broker.substr(0, colon);
            addrs.push_back(net::unresolved_address{host, port});
        }
    } catch (const std::exception e) {
        ws_log.warn("failed to parse brokers: {}", e.what());
        return std::move(std::vector<net::unresolved_address>{});
    }

    return std::move(addrs);
}

int main(int argc, char** argv) {
    ss::app_template app{};

    auto opts = app.add_options();
    opts(
      "brokers",
      po::value<std::string>()->default_value("127.0.0.1:9092"),
      "comma-delimited list of brokers");
    opts(
      "sasl-mechanism",
      po::value<std::string>()->default_value(""),
      "SASL mechanism to use when connecting");
    opts(
      "username",
      po::value<std::string>()->default_value(""),
      "Username to use for authentication");
    opts(
      "password",
      po::value<std::string>()->default_value(""),
      "Password to use for authentication");
    opts("tls", po::bool_switch()->default_value(false), "Use TLS?");

    return app.run(argc, argv, [&]() {
        auto& c = app.configuration();

        std::vector<std::string> brokers{};
        boost::split(
          brokers,
          c["brokers"].as<std::string>(),
          boost::algorithm::is_any_of(","));
        if (brokers.size() == 0) {
            return ss::make_exception_future<int>(
              std::runtime_error("must specify at least one broker"));
        }
        auto addrs = parse_brokers(brokers);
        if (addrs.size() == 0) {
            return ss::make_exception_future<int>(
              std::runtime_error("failed to parse brokers"));
        }

        return ss::async([&]() {
            static kc::configuration cfg;
            cfg.brokers.set_value(
              std::vector<net::unresolved_address>({{"127.0.0.1", 9092}}));
            kc::client client{config::to_yaml(cfg, config::redact_secrets::no)};

            client.connect().get();
            auto res = client.is_connected().get();
            ws_log.info("connected? {}", res);
            if (res) {
                kc::record_essence rec{};
		rec.key = iobuf();
                rec.key->append("key-abc", 7);
		rec.value = iobuf();
                rec.value->append("{ name: \"dave\" }", 16);
                model::topic t{"hello"};

                auto records = std::vector<kc::record_essence>();
		records.push_back(std::move(rec));

		ws_log.info("here goes nothing...");
                auto response
                  = client.produce_records(t, std::move(records)).get();
                ws_log.info("response: {}", response);
            }
            return 0;
        });
    });
}
