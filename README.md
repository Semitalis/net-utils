# net-utils
A simple library for network communication with server and client mechanics.

[![CMake](https://github.com/Semitalis/net-utils/actions/workflows/cmake-ci-build.yml/badge.svg?branch=master)](https://github.com/Semitalis/net-utils/actions/workflows/cmake-ci-build.yml)

Current support:
- TCP sockets

Example

```c++
// Server
auto server = net_utils::Tcp::listen(PORT, [](auto client){
    // new client connection
    while(!client->is_closed()) {
        client->read(...);
        client->send(...);
    }
});

// Client
auto client = net_utils::Tcp::connect(ADDRESS);
client->send(...);
client->read(...);
```
