
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
    ss::sharded<wsrp::service> ws;

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
    opts(
      "host",
      po::value<std::string>()->default_value("127.0.0.1"),
      "IP to listen on");
    opts(
      "port",
      po::value<std::uint16_t>()->default_value(8000),
      "TCP port to listen on");
    opts(
      "timeout",
      po::value<std::int32_t>()->default_value(-1),
      "Timeout for listening (< 0 for nearly infinite)");
    opts(
      "topic", po::value<std::string>()->required(), "Topic to sink data to");
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
        auto seeds = wsrp::parse_addresses(brokers);
        if (seeds.empty()) {
            return ss::make_exception_future<int>(
              std::runtime_error("failed to parse seed addresses"));
        }

        auto topic = c["topic"].as<std::string>();
        auto host = c["host"].as<std::string>();
        auto port = c["port"].as<std::uint16_t>();
        auto timeout = c["timeout"].as<std::int32_t>();

        return ws.start(topic, seeds, host, port)
          .then([&] {
              return ws.invoke_on_all([](wsrp::service& s) { (void)s.run(); });
          })
          .then([timeout] {
              auto dur = (timeout > 0) ? std::chrono::seconds(timeout)
                                       : std::chrono::years(100);
              return ss::sleep_abortable(dur).handle_exception(
                [](auto ignored) {});
          })
          .then([&ws] { return ws.stop(); })
          .then([] { return ss::make_ready_future<int>(0); });
    });
}
