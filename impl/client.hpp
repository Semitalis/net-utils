#pragma once

#include "address.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace net_utils {

class Client {
public:
    using Sptr = std::shared_ptr<Client>;

    virtual bool connect(Address addr)                                     = 0;
    virtual bool is_closed() const                                         = 0;
    virtual bool read(std::string& buf, std::chrono::milliseconds timeout) = 0;
    virtual bool send(std::string const& buf)                              = 0;
};

} // namespace net_utils
