cmake_minimum_required(VERSION 3.15)

project(net-utils VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(extern)

if(WIN32)
    # at least Win 8
    set(WINVER 0x0602)
    add_definitions(-D_WIN32_WINNT=${WINVER} -DWINVER=${WINVER})

    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
elseif(UNIX)
else()
    message(FATAL_ERROR "net-utils: Unsupported platform!")
endif()

find_package(Threads REQUIRED)

add_library(net-utils OBJECT
    impl/tcp.cpp
)

target_include_directories(net-utils
    PUBLIC
        impl
)

target_link_libraries(net-utils
    PUBLIC
        Threads::Threads
)

if(BUILD_TESTING)
    include(CTest)

    enable_testing()

    add_executable(tcp_test test/tcp.cpp)

    target_link_libraries(tcp_test
        PUBLIC
            net-utils
            gtest_main
    )

    add_test(NAME tcp_test COMMAND tcp_test)
endif()
