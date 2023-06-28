//
// Created by dv on 6/26/23.
//
#include "service.h"

#include "logger.h"
#include "redpanda.h"

#include <seastar/core/byteorder.hh>
#include <seastar/core/seastar.hh>
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
	    ws_log.info("handling request");
            return in
              // Read our 2-byte length of the key.
              .read_exactly(2)
              .then([&in](char_buf buf) {
                  if (buf.size() != 2 || in.eof()) {
                      return ss::make_exception_future<char_buf>(
                        std::runtime_error("disconnect"));
                  }
                  size_t const len = ss::read_be<uint16_t>(buf.get());
		  ws_log.info("read key length: {}", len);
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
		  ws_log.info("read key: {}", r.key);
                  return in.read_exactly(2);
              })
              // Read the 2-byte length of the value.
              .then([&in](char_buf buf) {
                  if (buf.size() != 2 || in.eof()) {
                      return ss::make_exception_future<char_buf>(
                        std::runtime_error("disconnect"));
                  }
                  size_t const len = ss::read_be<uint16_t>(buf.get());
		  ws_log.info("read value length: {}", len);
                  return in.read_exactly(len);
              })
              // Read the value.
              .then([&in, &q, &r](char_buf buf) {
                  if (buf.empty() || in.eof()) {
                      return ss::make_exception_future<stop_iter>(
                        std::runtime_error("disconnect"));
                  }
                  r.value.append(std::move(buf));
		  ws_log.info("producing record: {}", r);
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
    ws_log.info("starting...");
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

    ws_log.info("listening on {}", _sa);

    return ss::keep_doing([this] {
               return _queue->pop_eventually().then([](wsrp::record val) {
                   ws_log.info("popped {}", val);
                   return ss::make_ready_future<>();
               });
           })
      .handle_exception([](auto e) {
          ws_log.debug("swallowing error {}", e);
          return ss::make_ready_future<>();
      });
}

ss::future<> service::stop() {
    ws_log.info("stopping...");
    if (_ws) {
        return _ws->stop();
    }
    return ss::make_ready_future<>();
}

} // namespace wsrp
