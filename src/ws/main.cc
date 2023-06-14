#include "kafka/client/client.h"
#include "kafka/client/configuration.h"

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/seastar.hh>
#include <seastar/util/log.hh>

namespace kc = kafka::client;
namespace ss = seastar;

static ss::logger ws_log{"ws"};

int main(int argc, char** argv) {
    ss::app_template app{};

    kc::configuration cfg{};
    kc::client c{config::to_yaml(cfg, config::redact_secrets{false})};

    return app.run(argc, argv, [&] {
        ws_log.info("hello world! client config =  {}", c.config());
        return ss::make_ready_future<int>(0);
    });
}
