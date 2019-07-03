#pragma once

#include "client.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace net_utils {

class Server {
public:
    using Sptr = std::shared_ptr<Server>;

    virtual bool start(std::uint16_t port) = 0;
};

} // namespace net_utils
