cmake_minimum_required(VERSION 3.20)

# Name of the overall project.
# This project contains all Chapter 4 low-latency building blocks.
project(
    low_latency_chapter4
    LANGUAGES CXX
)


# ============================================================
# Find external/system libraries
# ============================================================

# Required by std::thread / pthreads on Linux.
find_package(Threads REQUIRED)


# ============================================================
# Thread demo
# ============================================================

add_executable(
    thread_demo
    src/thread_demo.cpp
)

target_include_directories(
    thread_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(
    thread_demo
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    thread_demo
    PRIVATE
        Threads::Threads
)


# ============================================================
# Memory pool demo
# ============================================================

add_executable(
    memory_pool_demo
    src/memory_pool_demo.cpp
)

target_include_directories(
    memory_pool_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(
    memory_pool_demo
    PRIVATE
        cxx_std_20
)


# ============================================================
# SPSC lock-free queue demo
# ============================================================

add_executable(
    lf_queue_demo
    src/lf_queue_demo.cpp
)

target_include_directories(
    lf_queue_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(
    lf_queue_demo
    PRIVATE
        cxx_std_20
)

# lf_queue_demo uses createAndStartThread().
target_link_libraries(
    lf_queue_demo
    PRIVATE
        Threads::Threads
)


# ============================================================
# Low-latency logger demo
# ============================================================

add_executable(
    logger_demo
    src/logger_demo.cpp
    src/logger.cpp
)

target_include_directories(
    logger_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(
    logger_demo
    PRIVATE
        cxx_std_20
)

# Logger launches a background thread.
target_link_libraries(
    logger_demo
    PRIVATE
        Threads::Threads
)


# ============================================================
# TCP socket / epoll demo
# ============================================================

# The socket implementation uses Linux-specific APIs:
#
#   epoll
#   getifaddrs
#   POSIX sockets
#   fcntl
#
# Therefore this target is built only on Linux / WSL.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")

    add_executable(
        socket_demo

        # Demo / main()
        src/socket_demo.cpp

        # TCP server abstraction
        src/tcp_server.cpp

        # TCP connection abstraction
        src/tcp_socket.cpp

        # Low-level Unix socket utilities
        src/socket_utils.cpp

        # Async logger implementation
        src/logger.cpp
    )

    target_include_directories(
        socket_demo
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_features(
        socket_demo
        PRIVATE
            cxx_std_20
    )

    # logger.cpp launches its background logger thread.
    target_link_libraries(
        socket_demo
        PRIVATE
            Threads::Threads
    )

endif()


# ============================================================
# Compiler warnings
# ============================================================

if(MSVC)

    target_compile_options(
        thread_demo
        PRIVATE
            /W4
    )

    target_compile_options(
        memory_pool_demo
        PRIVATE
            /W4
    )

    target_compile_options(
        lf_queue_demo
        PRIVATE
            /W4
    )

    target_compile_options(
        logger_demo
        PRIVATE
            /W4
    )

else()

    target_compile_options(
        thread_demo
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_compile_options(
        memory_pool_demo
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_compile_options(
        lf_queue_demo
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_compile_options(
        logger_demo
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )


    # socket_demo only exists on Linux.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")

        target_compile_options(
            socket_demo
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )

    endif()

endif()


# ============================================================
# Memory pool test
# ============================================================

include(CTest)

if(BUILD_TESTING)

    add_executable(
        memory_pool_test
        tests/memory_pool_test.cpp
    )

    target_include_directories(
        memory_pool_test
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_features(
        memory_pool_test
        PRIVATE
            cxx_std_20
    )

    add_test(
        NAME memory_pool_test
        COMMAND memory_pool_test
    )

endif()



# ============================================================
# SPSC lock-free queue test
# ============================================================

if(BUILD_TESTING)

    add_executable(
        lf_queue_test
        tests/lf_queue_test.cpp
    )

    target_include_directories(
        lf_queue_test
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_features(
        lf_queue_test
        PRIVATE
            cxx_std_20
    )

    # The test runs one producer and one consumer thread.
    target_link_libraries(
        lf_queue_test
        PRIVATE
            Threads::Threads
    )

    add_test(
        NAME lf_queue_test
        COMMAND lf_queue_test
    )

    # Stop the test if a queue bug leaves a thread waiting forever.
    set_tests_properties(
        lf_queue_test
        PROPERTIES
            TIMEOUT 10
    )

endif()
