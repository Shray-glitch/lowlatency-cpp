#include "tcp_server.hpp"

#include "socket_utils.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>


namespace
{

void addUnique(
    std::vector<TCPSocket*>& sockets,
    TCPSocket* socket)
{
    if (
        std::find(
            sockets.begin(),
            sockets.end(),
            socket
        ) == sockets.end()
    )
    {
        sockets.push_back(socket);
    }
}

} // namespace


// ============================================================
// Constructor
// ============================================================

TCPServer::TCPServer(Logger& logger)
    : listener_socket_(logger),
      logger_(logger)
{
    recv_callback_ =
        [this](
            TCPSocket* socket,
            Nanos rx_time)
        {
            logger_.log(
                "TCPServer recv socket:% len:% rx:%\n",
                socket->fd_,
                socket->next_rcv_valid_index_,
                rx_time
            );
        };


    recv_finished_callback_ =
        [this]()
        {
            logger_.log(
                "TCPServer receive round finished\n"
            );
        };
}


// ============================================================
// Destructor
// ============================================================

TCPServer::~TCPServer()
{
    destroy();
}


// ============================================================
// Destroy server resources
// ============================================================

void TCPServer::destroy() noexcept
{
    // Accepted sockets were dynamically allocated
    // inside poll(), so this server owns them.
    for (TCPSocket* socket : sockets_)
    {
        delete socket;
    }


    sockets_.clear();
    receive_sockets_.clear();
    send_sockets_.clear();
    disconnected_sockets_.clear();


    if (efd_ >= 0)
    {
        ::close(efd_);
        efd_ = -1;
    }


    listener_socket_.destroy();
}


// ============================================================
// Add socket to epoll
// ============================================================

bool TCPServer::epollAdd(
    TCPSocket* socket)
{
    epoll_event event{};


    // EPOLLIN:
    // notify us when the socket can be read.
    //
    // EPOLLET:
    // use edge-triggered notification.
    event.events =
        EPOLLIN | EPOLLET;


    // Store the actual TCPSocket pointer so that
    // epoll_wait() gives it back to us later.
    event.data.ptr =
        static_cast<void*>(socket);


    return ::epoll_ctl(
        efd_,
        EPOLL_CTL_ADD,
        socket->fd_,
        &event
    ) != -1;
}


// ============================================================
// Remove socket from epoll
// ============================================================

bool TCPServer::epollDel(
    TCPSocket* socket)
{
    if (
        efd_ < 0 ||
        socket == nullptr ||
        socket->fd_ < 0
    )
    {
        return false;
    }


    return ::epoll_ctl(
        efd_,
        EPOLL_CTL_DEL,
        socket->fd_,
        nullptr
    ) != -1;
}


// ============================================================
// Remove a disconnected client
// ============================================================

void TCPServer::del(
    TCPSocket* socket)
{
    if (
        socket == nullptr ||
        socket == &listener_socket_
    )
    {
        return;
    }


    epollDel(socket);


    auto removeSocket =
        [socket](
            std::vector<TCPSocket*>& container)
        {
            container.erase(
                std::remove(
                    container.begin(),
                    container.end(),
                    socket
                ),
                container.end()
            );
        };


    removeSocket(sockets_);
    removeSocket(receive_sockets_);
    removeSocket(send_sockets_);
    removeSocket(disconnected_sockets_);


    delete socket;
}


// ============================================================
// Start listener
// ============================================================

void TCPServer::listen(
    const std::string& iface,
    int port)
{
    // Allow the server object to be reused.
    destroy();


    // Create epoll instance.
    efd_ =
        ::epoll_create1(0);

    assert(
        efd_ >= 0 &&
        "epoll_create1() failed"
    );


    // Empty IP means createSocket() resolves
    // the supplied interface.
    //
    // true means this TCPSocket is a listener.
    const int listener_fd =
        listener_socket_.connect(
            "",
            iface,
            port,
            true
        );


    assert(
        listener_fd >= 0 &&
        "Failed to create listener socket"
    );


    assert(
        epollAdd(
            &listener_socket_
        ) &&
        "Failed to add listener to epoll"
    );
}


// ============================================================
// Poll network state
// ============================================================

void TCPServer::poll() noexcept
{
    // --------------------------------------------------------
    // Remove sockets that were marked disconnected
    // during the previous pass.
    // --------------------------------------------------------

    const auto disconnected =
        disconnected_sockets_;

    disconnected_sockets_.clear();


    for (TCPSocket* socket : disconnected)
    {
        del(socket);
    }


    // --------------------------------------------------------
    // Ask epoll which descriptors have events.
    //
    // timeout = 0 means:
    // return immediately; never block this thread.
    // --------------------------------------------------------

    const int event_count =
        ::epoll_wait(
            efd_,
            events_,
            MAX_EPOLL_EVENTS,
            0
        );


    if (event_count < 0)
    {
        // EINTR means a signal interrupted the call.
        // Nothing fatal happened to the server.
        if (errno == EINTR)
        {
            return;
        }

        return;
    }


    bool have_new_connection =
        false;


    // --------------------------------------------------------
    // Interpret epoll events.
    // --------------------------------------------------------

    for (
        int i = 0;
        i < event_count;
        ++i)
    {
        epoll_event& event =
            events_[i];


        auto* socket =
            static_cast<TCPSocket*>(
                event.data.ptr
            );


        if (socket == nullptr)
        {
            continue;
        }


        // ----------------------------------------
        // Error / disconnect
        // ----------------------------------------

        if (
            event.events &
            (EPOLLERR | EPOLLHUP)
        )
        {
            if (
                socket
                != &listener_socket_
            )
            {
                addUnique(
                    disconnected_sockets_,
                    socket
                );
            }

            continue;
        }


        // ----------------------------------------
        // Socket has something readable.
        // ----------------------------------------

        if (event.events & EPOLLIN)
        {
            // EPOLLIN on listener means:
            // one or more clients are waiting
            // to be accepted.
            if (
                socket
                == &listener_socket_
            )
            {
                have_new_connection =
                    true;

                continue;
            }


            // Normal client socket:
            // incoming bytes are available.
            addUnique(
                receive_sockets_,
                socket
            );
        }


        // The book checks EPOLLOUT too.
        //
        // Our epoll registration currently requests
        // only EPOLLIN | EPOLLET, so this will normally
        // not be produced. We retain the handling because
        // it is part of the server architecture.
        if (event.events & EPOLLOUT)
        {
            addUnique(
                send_sockets_,
                socket
            );
        }
    }


    // --------------------------------------------------------
    // Accept all currently pending connections.
    //
    // Because the listener is non-blocking and epoll is
    // edge-triggered, keep accepting until accept()
    // says there are no more.
    // --------------------------------------------------------

    if (have_new_connection)
    {
        while (true)
        {
            sockaddr_storage address{};

            socklen_t address_length =
                sizeof(address);


            const int client_fd =
                ::accept(
                    listener_socket_.fd_,
                    reinterpret_cast<sockaddr*>(
                        &address
                    ),
                    &address_length
                );


            if (client_fd < 0)
            {
                // Non-blocking listener:
                // no more connections are currently
                // waiting to be accepted.
                if (
                    errno == EAGAIN ||
                    errno == EWOULDBLOCK
                )
                {
                    break;
                }

                break;
            }


            // accept() gives us a new descriptor.
            // Configure it for low-latency operation.
            if (
                !setNonBlocking(client_fd) ||
                !setNoDelay(client_fd)
            )
            {
                ::close(client_fd);
                continue;
            }


            // Wrap the raw descriptor in our TCPSocket.
            auto* socket =
                new TCPSocket(logger_);


            socket->fd_ =
                client_fd;


            // All accepted sockets use the server's
            // application receive callback.
            socket->recv_callback_ =
                recv_callback_;


            if (!epollAdd(socket))
            {
                delete socket;
                continue;
            }


            addUnique(
                sockets_,
                socket
            );


            // The book adds newly accepted sockets to the
            // receive collection as well.
            addUnique(
                receive_sockets_,
                socket
            );
        }
    }
}


// ============================================================
// Perform actual I/O
// ============================================================

void TCPServer::sendAndRecv() noexcept
{
    bool received_anything =
        false;


    // --------------------------------------------------------
    // Read from sockets that may contain incoming data.
    //
    // TCPSocket::sendAndRecv() also flushes that socket's
    // outgoing buffer, which is useful when the receive
    // callback queued a reply.
    // --------------------------------------------------------

    for (
        TCPSocket* socket :
        receive_sockets_)
    {
        if (socket == nullptr)
        {
            continue;
        }


        if (socket->sendAndRecv())
        {
            received_anything = true;
        }


        if (
            socket->recv_disconnected_ ||
            socket->send_disconnected_
        )
        {
            addUnique(
                disconnected_sockets_,
                socket
            );
        }
    }


    // One callback after this receive round.
    if (received_anything)
    {
        recv_finished_callback_();
    }


    // --------------------------------------------------------
    // Handle explicit writable sockets.
    // --------------------------------------------------------

    for (
        TCPSocket* socket :
        send_sockets_)
    {
        if (socket == nullptr)
        {
            continue;
        }


        // Avoid doing it twice if this socket was
        // already processed above.
        if (
            std::find(
                receive_sockets_.begin(),
                receive_sockets_.end(),
                socket
            ) != receive_sockets_.end()
        )
        {
            continue;
        }


        socket->sendAndRecv();


        if (
            socket->recv_disconnected_ ||
            socket->send_disconnected_
        )
        {
            addUnique(
                disconnected_sockets_,
                socket
            );
        }
    }
}