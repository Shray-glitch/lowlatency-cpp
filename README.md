# Low-Latency C++ Building Blocks

A C++20 project exploring reusable building blocks for latency-sensitive systems, including CPU affinity, preallocated memory, inter-thread communication, asynchronous logging, and non-blocking TCP networking with Linux `epoll`.

The repository includes runnable demonstrations and eight automated correctness tests.

## Engineering Focus

- Fixed-capacity memory allocation and object reuse
- Single-producer, single-consumer communication using atomics
- CPU affinity using Linux pthread APIs
- Background-thread logging
- Fixed-size TCP send and receive buffers
- Non-blocking socket operations
- `TCP_NODELAY` for small TCP messages
- Kernel receive timestamps using `SO_TIMESTAMP`
- Event-driven connection management using `epoll`
- Explicit error handling and resource cleanup

## Components

| Component | Responsibility |
|---|---|
| `MemPool<T>` | Constructs objects inside preallocated reusable memory slots |
| `LFQueue<T>` | Transfers data between one producer and one consumer |
| `Logger` | Queues formatted values and writes them from a background thread |
| Thread utilities | Starts threads and optionally pins them to CPU cores |
| Time utilities | Provides wall-clock timestamps and monotonic latency measurements |
| Socket utilities | Creates and configures non-blocking TCP sockets |
| `TCPSocket` | Manages fixed send/receive buffers and socket I/O |
| `TCPServer` | Accepts and manages multiple connections using `epoll` |

## Requirements

- Linux or Windows Subsystem for Linux
- CMake 3.20 or newer
- A C++20-compatible compiler
- POSIX threads
- Linux networking APIs

The CPU-affinity and networking components are Linux-specific.

## Build

Configure a Release build with tests enabled:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
```

Build all targets:

```bash
cmake --build build --parallel
```

## Run the Tests

```bash
ctest --test-dir build --output-on-failure
```

The test suite covers:

| Test | Coverage |
|---|---|
| `memory_pool_test` | Capacity, object lifetime, destruction, and slot reuse |
| `lf_queue_test` | FIFO ordering, full/empty states, reuse, and two-thread transfer |
| `logger_test` | Formatting, queue pressure, and complete file output |
| `thread_utils_test` | Thread arguments and invalid CPU handling |
| `socket_utils_test` | Interface lookup, non-blocking mode, and socket options |
| `tcp_socket_test` | Bidirectional transfer, callbacks, buffer limits, and cleanup |
| `tcp_server_test` | Server setup and a loopback `PING`/`PONG` exchange |
| `time_utils_test` | Unit conversions, monotonic time, and timestamp formatting |

Expected result:

```text
100% tests passed, 0 tests failed out of 8
```

These tests validate correctness and failure handling. They are not performance benchmarks.

## Run the Demos

| Demo | Command |
|---|---|
| CPU affinity | `./build/thread_demo` |
| Memory pool | `./build/memory_pool_demo` |
| SPSC queue | `./build/lf_queue_demo` |
| Asynchronous logger | `./build/logger_demo` |
| TCP server and clients | `./build/socket_demo` |

The logger and socket demonstrations create `logger_demo.log` and `socket_demo.log`. Generated log files are excluded from Git.

## Design Overview

### Memory Pool

`MemPool<T>` reserves a fixed number of correctly aligned memory slots during construction.

`allocate()` constructs an object inside a free slot using placement new. `deallocate()` runs the object's destructor and makes that slot reusable.

This avoids repeated heap allocation after the pool has been created.

### SPSC Queue

`LFQueue<T>` is a fixed-capacity circular queue designed for exactly:

- One producer thread
- One consumer thread

The producer writes into a free slot and then publishes it. The consumer reads a published item and then releases its slot.

The queue rejects writes when all slots are occupied instead of overwriting unread data.

### Asynchronous Logger

The calling thread converts a log message into small typed elements and places them in an SPSC queue.

A background thread consumes those elements and writes them to a file. If the queue becomes full, the calling thread waits until space becomes available rather than losing data.

### TCP Networking

`TCPServer` uses one listener socket and a Linux `epoll` instance.

When a client connects:

1. The listener accepts the connection.
2. The descriptor is configured as non-blocking.
3. A `TCPSocket` object takes ownership of it.
4. Incoming data is placed in a fixed receive buffer.
5. The application callback processes the bytes.
6. Outgoing replies are placed in a fixed send buffer.

The socket code handles partial sends, temporary non-blocking errors, disconnections, and resource cleanup.

## Important Constraints

- `LFQueue` supports only one producer and one consumer.
- `Logger` should be called from one application thread.
- The logger can wait when its queue is full.
- TCP send and receive buffers have a fixed capacity of 64 KB.
- Receive callbacks must release consumed receive-buffer space.
- Memory-pool deallocation currently searches the slots linearly.
- The networking implementation is Linux-specific.
- The project is intended for learning and experimentation, not production deployment.

## Project Structure

```text
.
├── include/
│   ├── lf_queue.hpp
│   ├── logger.hpp
│   ├── mem_pool.hpp
│   ├── socket_utils.hpp
│   ├── tcp_server.hpp
│   ├── tcp_socket.hpp
│   ├── thread_utils.hpp
│   └── time_utils.hpp
├── src/
│   ├── logger.cpp
│   ├── socket_utils.cpp
│   ├── tcp_server.cpp
│   ├── tcp_socket.cpp
│   └── *_demo.cpp
├── tests/
├── CMakeLists.txt
└── README.md
```

## License

This project includes software distributed under the MIT License. See [LICENSE](LICENSE) for the complete copyright and license notice.
