#include "tcp.hpp"

#include "client.hpp"
#include "server.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

#ifdef WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "Ws2_32.lib")
#else
#    include <arpa/inet.h>
#    include <cstring>
#    include <fcntl.h>
#    include <netdb.h>
#    include <string>
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace net_utils {

#ifdef WIN32

#    define MAPPED_WOULDBLOCK WSAEWOULDBLOCK
#    define MAPPED_CONNREFUSED WSAEWOULDBLOCK

using SocketType = SOCKET;

struct Wsa_framework {
    Wsa_framework()
    {
        WSADATA wsaData;
        auto    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed with error: " << result << std::endl;
        }
    }

    ~Wsa_framework() { (void)WSACleanup(); }
} wsa_initializer;

namespace detail {

int get_last_error()
{
    return WSAGetLastError();
}

std::string WSAGetLastErrorMessage()
{
    int  err = WSAGetLastError();
    char buf[256];
    buf[0] = '\0';

    (void)FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // flags
        NULL,                                                       // lpsource
        err,                                                        // message id
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                  // languageid
        buf,                                                        // output buffer
        sizeof(buf),                                                // size of buf, bytes
        NULL);                                                      // va_list of arguments

    if (!*buf) {
        sprintf(buf, "%d", err); // provide error # if no string available
    }

    return buf;
}

void perror(char const* msg)
{
    std::cerr << msg << WSAGetLastErrorMessage() << std::endl;
}

void close(SocketType& socket) noexcept
{
    if (socket != INVALID_SOCKET) {
        auto result = shutdown(socket, SD_SEND);
        if (result == SOCKET_ERROR) {
            detail::perror("shutdown() failed with error: ");
        }
        (void)closesocket(socket);
        socket = INVALID_SOCKET;
    }
}

} // namespace detail

#else

#    define SOCKET_ERROR -1
#    define INVALID_SOCKET 0
#    define MAPPED_WOULDBLOCK EWOULDBLOCK
#    define MAPPED_CONNREFUSED ECONNREFUSED

using SocketType = int;

namespace detail {

int get_last_error()
{
    return errno;
}

using ::perror;

void close(SocketType& socket) noexcept
{
    if (socket != INVALID_SOCKET) {
        auto result = ::close(socket);
        if (result == SOCKET_ERROR) {
            detail::perror("close() failed with error: ");
        }
        socket = INVALID_SOCKET;
    }
}

} // namespace detail

#endif

namespace {

class Client_impl final : public Client {
    SocketType m_socket;

public:
    Client_impl() : m_socket{INVALID_SOCKET} {}

    Client_impl(SocketType s) : m_socket{s} {}

    ~Client_impl() { close(); }

    void close() noexcept { detail::close(m_socket); }

    bool connect(Address addr) override
    {
        // TODO: connect directly to server
        return false;
    }

    bool is_closed() const override { return m_socket == INVALID_SOCKET; }

    bool read(std::string& dst, std::chrono::milliseconds timeout) override
    {
        if (m_socket == INVALID_SOCKET) {
            dst.clear();
            return false;
        }

        char buf[512];

        int  result{};
        auto end = std::chrono::system_clock::now() + timeout;
        do {
            // receive stuff
            result = ::recv(m_socket, buf, 512, /*MSG_DONTWAIT TODO ioctlsocket */ 0);

            // error
            if (result == SOCKET_ERROR) {
                auto const error = detail::get_last_error();
                if (error == MAPPED_WOULDBLOCK) {
                    // no error, simply continue
                    std::this_thread::yield();
                    continue;
                }
                if (error == MAPPED_CONNREFUSED) {
                    // connection was aborted
                    dst.clear();
                    close();
                    return false;
                }
                detail::perror("recv() failed with error: ");
                dst.clear();
                close();
                return false;
            }

            // append to destination buffer
            if (result > 0) {
                dst.append(buf, result);
            }

        } while ((result > 0) && (std::chrono::system_clock::now() < end));
        // std::cerr << "rcvd!\n" << dst << std::endl;

        return true;
    }

    bool send(std::string const& buf) override
    {
        // Echo the buffer back to the sender
        auto result = ::send(m_socket, buf.data(), (int)buf.size(), 0);
        if (result == SOCKET_ERROR) {
            detail::perror("send() failed with error: ");
            return false;
        }
        // std::cerr << "Bytes sent: " << result << std::endl;
        return true;
    }
};

class Server_impl final : public Server {
    std::atomic_bool m_active{true};
    SocketType       m_socket;
    Address          m_address;
    Client_handler   m_handler;

public:
    Server_impl(Client_handler handler) : m_handler{std::move(handler)} {}

    ~Server_impl() { close(); }

    void close() noexcept
    {
        m_active = false;
        detail::close(m_socket);
    }

    bool start(std::uint16_t port) override
    {
        if (!m_handler) {
            std::cerr << "no client handler!" << std::endl;
            return false;
        }

        char      buf[32];
        addrinfo* info{NULL};
        addrinfo  hints;
        ::memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags    = AI_PASSIVE;

        // resolve the server address and port
        auto const port_str = std::to_string(port);
        auto       result   = ::getaddrinfo(NULL, port_str.c_str(), &hints, &info);
        if (result != 0) {
            std::cerr << "getaddrinfo failed with error: " << gai_strerror(result) << std::endl;
            return 1;
        }

        // create a SocketType for connecting to server
        m_socket = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (m_socket < 0) {
            detail::perror("socket() failed with error: ");
            ::freeaddrinfo(info);
            return false;
        }

        // reuse addr and port
#ifdef WIN32
        char const enable = 1;
#else
        int const enable = 1;
#endif
        if (::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) ==
            SOCKET_ERROR) {
            detail::perror("setsockopt(SO_REUSEADDR) failed");
            ::freeaddrinfo(info);
            return false;
        }
#ifndef WIN32
        if (::setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) ==
            SOCKET_ERROR) {
            detail::perror("setsockopt(SO_REUSEPORT) failed");
            ::freeaddrinfo(info);
            return false;
        }
#endif

        // setup the TCP listening socket
        result = ::bind(m_socket, info->ai_addr, (int)info->ai_addrlen);
        ::freeaddrinfo(info);
        if (result == SOCKET_ERROR) {
            detail::perror("bind() failed with error: ");
            close();
            return false;
        }
        result = ::listen(m_socket, SOMAXCONN);
        if (result == SOCKET_ERROR) {
            detail::perror("listen() failed with error: ");
            close();
            return false;
        }

        // start loop that will continously accept incoming connections
        std::thread{[this]() {
            while (m_active) {
                auto client_socket = ::accept(m_socket, NULL, NULL);
                if (client_socket == INVALID_SOCKET) {
                    detail::perror("accept() failed with error: ");
                    continue;
                }

#ifdef WIN32
                u_long iMode{1};
                if (::ioctlsocket(client_socket, FIONBIO, &iMode) == SOCKET_ERROR) {
                    detail::perror("ioctlsocket() could not set socket to non blocking mode: ");
                    continue;
                }
#else
                int flags = ::fcntl(client_socket, F_GETFL, 0);
                if (::fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == SOCKET_ERROR) {
                    detail::perror("fcntl() could not set socket to non blocking mode: ");
                    continue;
                }
#endif

                m_handler(std::make_shared<Client_impl>(client_socket));
            }
        }}.detach();
        return true;
    }
};

} // namespace

class Tcp::Private final {
public:
};

Tcp::Tcp() : m{std::make_unique<Private>()}
{
}

Tcp::~Tcp()
{
}

Server::Sptr Tcp::listen(std::uint16_t port, Client_handler handler)
{
    auto server = std::make_shared<Server_impl>(std::move(handler));
    if (server->start(port)) {
        return server;
    }
    return nullptr;
}

Client::Sptr Tcp::connect(Address addr)
{
    auto client = std::make_shared<Client_impl>();
    if (client->connect(addr)) {
        return client;
    }
    return nullptr;
}

} // namespace net_utils
