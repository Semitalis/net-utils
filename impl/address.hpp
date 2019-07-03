#pragma once

#include <cstdint>
#include <string>

namespace net_utils {

class Address {
    std::string   m_hostname;
    std::uint16_t m_port{};

public:
    Address() = default;

    Address(std::string const& /*addr*/) {}

    std::string hostname() const { return m_hostname; }

    std::uint16_t port() const { return m_port; }
};

} // namespace net_utils
