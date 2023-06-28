
#include "logger.h"
#include "redpanda.h"
#include "service.h"

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/thread.hh>

#include <bytes/bytes.h>

namespace ss = seastar;
namespace po = boost::program_options;

int main(int argc, char** argv) {
    ss::app_template app{};

    // Parse any of our CLI arguments.
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

    // Fire up the reactor!
    return app.run(argc, argv, [&]() -> ss::future<int> {
        auto& c = app.configuration();

        // Figure out the addresses of our seed brokers.
        auto brokers = wsrp::parse_brokers(c["brokers"].as<std::string>());
        if (brokers.empty()) {
            return ss::make_exception_future<int>(std::runtime_error(
              "must specify at least one valid seed broker"));
        }
        auto addrs = wsrp::parse_addresses(brokers);
        if (addrs.empty()) {
            return ss::make_exception_future<int>(
              std::runtime_error("failed to parse addresses"));
        }

        return ss::async([&, addrs = std::move(addrs)] {
	    // Make our client
            wsrp::redpanda client{wsrp::make_config(addrs)};

	    // Connect
	    auto f = client.connect().then([&] {
                wsrp::ws_log.info("connected!");

		wsrp::record r{};
		const char* key = "mykey";
		const char* val = "myvalue";
		r.key.append(key, std::strlen(key));
		r.value.append(val, std::strlen(val));
		std::vector<wsrp::record> records{};
		records.emplace_back(std::move(r));

		// Produce
		return client.produce(model::topic{"junk"}, std::move(records))
                  .then([](auto res) {
                      wsrp::ws_log.info("produced!");
                      return ss::make_ready_future<int>(0);
                  });
            });
            wsrp::ws_log.info("seeing if we're connected...");
            auto res = f.get();
            wsrp::ws_log.info("result = {}", res);
            return 0;
        });
    });
}
