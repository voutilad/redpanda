#pragma once

#include <seastar/util/log.hh>

namespace mqtt {
inline seastar::logger log("mqtt");
} // namespace mqtt
