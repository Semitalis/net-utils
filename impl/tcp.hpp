#pragma once

#include "address.hpp"
#include "client.hpp"
#include "server.hpp"
#include <cstdint>
#include <memory>

namespace net_utils {

using Client_handler = std::function<void(Client::Sptr)>;

class Tcp {
    class Private;
    std::unique_ptr<Private> m;

public:
    Tcp();
    virtual ~Tcp();

    // no copy
    Tcp(Tcp const& other)            = delete;
    Tcp& operator=(Tcp const& other) = delete;

    // no move
    Tcp(Tcp&& other)            = delete;
    Tcp& operator=(Tcp&& other) = delete;

    Server::Sptr listen(std::uint16_t port, Client_handler handler);

    Client::Sptr connect(Address addr);
};

} // namespace net_utils
