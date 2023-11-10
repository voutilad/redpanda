#include "logger.h"

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>

int main(int argc, char** argv) {
    seastar::app_template app{};

    return app.run(argc, argv, [&]() -> seastar::future<int> {
        mqtt::log.info("started");
        return seastar::make_ready_future<int>(0);
    });
}
