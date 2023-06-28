//
// Created by dv on 6/26/23.
//

#pragma once

#include "redpanda.h"

#include <seastar/core/seastar.hh>
#include <seastar/websocket/server.hh>

namespace ss = seastar;
namespace ws = seastar::experimental::websocket;

namespace wsrp {

static const int QUEUE_DEPTH = 256;

/// \brief A WebSocket service that ferries key/value pairs into a Redpanda
/// topic.
///
/// For now, this is a single hardcoded topic.
class service {
    ss::socket_address _sa;
    std::optional<ws::server> _ws;
    ss::shared_ptr<ss::queue<wsrp::record>> _queue;
    std::optional<wsrp::redpanda> _rp;

public:
    explicit service(ss::socket_address listen_on)
      : _sa(listen_on)
      , _queue(ss::make_shared<ss::queue<wsrp::record>>(QUEUE_DEPTH)) {
        _ws = ws::server{};
    }

    /// \brief called by .invoke_on_all() when starting our the sharded service
    ss::future<> run();

    /// \brief called to interrupt and teardown the websocket service
    ss::future<> stop();
};

} // namespace wsrp
