//
// Created by dv on 6/26/23.
//
#include "service.h"

#include "logger.h"
#include "redpanda.h"

#include <seastar/core/byteorder.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/thread.hh>
#include <seastar/websocket/server.hh>

#include <utility>

namespace ss = seastar;

using stop_iter = ss::stop_iteration;
using char_buf = ss::temporary_buffer<char>;

namespace wsrp {

/// \brief A handler for decomposing key/value pairs from a websocket
/// \param q - a pointer to a seastar queue to push the result into
/// \param in - the input stream to process
/// \param out - ignored.
ss::future<> handler(
  ss::shared_ptr<ss::queue<wsrp::record>>& q,
  ss::input_stream<char>& in,
  ss::output_stream<char>& out) {
    return ss::repeat([&in, &q] {
        // Early abort: if our input stream is already closed, give up.
        if (in.eof()) {
            return ss::make_ready_future<stop_iter>(stop_iter::yes);
        }
        return ss::do_with(wsrp::record{}, [&](wsrp::record& r) {
            ws_log.debug("handling request");
            return in
              // Read our 2-byte length of the key.
              .read_exactly(2)
              .then([&in](char_buf buf) {
                  if (buf.size() != 2 || in.eof()) {
                      return ss::make_exception_future<char_buf>(
                        std::runtime_error("disconnect"));
                  }
                  size_t const len = ss::read_be<uint16_t>(buf.get());
                  ws_log.debug("read key length: {}", len);
                  return in.read_exactly(len);
              })
              // Read our key.
              // XXX TODO: handle reading empty key
              .then([&in, &r](char_buf buf) {
                  if (buf.empty() || in.eof()) {
                      return ss::make_exception_future<char_buf>(
                        std::runtime_error("disconnect"));
                  }
                  r.key.append(std::move(buf));
                  ws_log.debug("read key: {}", r.key);
                  return in.read_exactly(2);
              })
              // Read the 2-byte length of the value.
              .then([&in](char_buf buf) {
                  if (buf.size() != 2 || in.eof()) {
                      return ss::make_exception_future<char_buf>(
                        std::runtime_error("disconnect"));
                  }
                  size_t const len = ss::read_be<uint16_t>(buf.get());
                  ws_log.debug("read value length: {}", len);
                  return in.read_exactly(len);
              })
              // Read the value.
              .then([&in, &q, &r](char_buf buf) {
                  if (buf.empty() || in.eof()) {
                      return ss::make_exception_future<stop_iter>(
                        std::runtime_error("disconnect"));
                  }
                  r.value.append(std::move(buf));
                  return q->push_eventually(std::move(r)).then([] {
                      return ss::make_ready_future<stop_iter>(stop_iter::no);
                  });
              })
              // If anything goes wrong, consider it a disconnect for now.
              .handle_exception([](std::exception_ptr e) {
                  try {
                      std::rethrow_exception(std::move(e));
                  } catch (const std::exception& e) {
                      ws_log.error("{}", e.what());
                  }
                  return ss::make_ready_future<stop_iter>(stop_iter::yes);
              });
        });
    });
}

ss::future<> service::run() {
    if (!_ws) {
        throw new std::runtime_error(
          "run() called without initializing websocket service");
    }
    if (!_rp) {
        throw new std::runtime_error(
          "run() called without initializing redpanda client");
    }

    try {
        // Create a handler function bound to our shard-local queue.
        auto fn = std::bind(
          handler, _queue, std::placeholders::_1, std::placeholders::_2);

        // XXX: the protocol name is hard-coded for now.
        _ws->register_handler("dumb-ws", fn);
        _ws->listen(_sa, {true});
    } catch (std::system_error& e) {
        ws_log.error("failed to listen on {}: {}", _sa, e);
        return ss::make_exception_future(std::move(e));
    } catch (std::exception& e) {
        ws_log.error("uh oh! {}", e);
        return ss::make_exception_future(std::move(e));
    }
    ws_log.info("listening on {} and sinking to {}", _sa, _topic);

    // We're running. Now we need to start consuming from our queue and
    // producing to Redpanda.
    // XXX do this in an async "thread" due to lifetime issues I don't
    // quite have my head around with the Redpanda client.
    return ss::async([&, this] {
        auto f = _rp->connect().then([this] {
            return ss::keep_doing([this] {
                       return _queue->pop_eventually().then(
                         [this](wsrp::record val) {
                             ws_log.debug("popped {}", val);
                             // XXX need a buffering vector...
                             std::vector<wsrp::record> batch{};
                             batch.emplace_back(std::move(val));

                             auto cnt = batch.size();
                             return _rp
                               ->produce(model::topic{_topic}, std::move(batch))
                               .then([cnt](auto unused) {
                                   ws_log.debug("sent {} record(s)", cnt);
                                   return ss::make_ready_future<>();
                               });
                         });
                   })
              .handle_exception(
                [](auto unused) { return ss::make_ready_future<>(); });
        });
        f.wait();
        return;
    });
}

ss::future<> service::stop() {
    if (_ws) {
        ws_log.info("stopping");
	if (_rp) {
	    return _rp->disconnect().then([this] {
		return _ws->stop();
	    });
	}
        return _ws->stop();
    }
    return ss::make_ready_future<>();
}

} // namespace wsrp
