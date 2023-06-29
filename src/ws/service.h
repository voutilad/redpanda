//
// Created by dv on 6/26/23.
//

#pragma once

#include "redpanda.h"

#include <seastar/core/seastar.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/websocket/server.hh>

namespace ss = seastar;
namespace ws = seastar::experimental::websocket;

namespace wsrp {

static const uint16_t DEFAULT_PORT = 8000;
static const int QUEUE_DEPTH = 256;

/// \brief A WebSocket service that ferries key/value pairs into a Redpanda
/// topic.
///
/// For now, this is a single hardcoded topic.
class service {
    ss::socket_address _sa;
    ss::sstring _topic;
    std::optional<ws::server> _ws;
    ss::shared_ptr<ss::queue<wsrp::record>> _queue;
    ss::lw_shared_ptr<wsrp::redpanda> _rp;

public:
    explicit service(
      std::string topic,
      const std::vector<net::unresolved_address>& seeds,
      std::string host = "127.0.0.1",
      uint16_t port = DEFAULT_PORT)
      : _topic(ss::sstring{topic})
      , _sa(ss::socket_address(ss::ipv4_addr(host, port)))
      , _queue(ss::make_shared<ss::queue<wsrp::record>>(QUEUE_DEPTH))
      , _rp(ss::make_lw_shared<wsrp::redpanda>(seeds)) {
        _ws = ws::server{};
    }

    /// \brief called by .invoke_on_all() when starting our the sharded service
    ss::future<> run();

    /// \brief called to interrupt and teardown the websocket service
    ss::future<> stop();
};

} // namespace wsrp
