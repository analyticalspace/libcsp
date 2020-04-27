#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <thread>

#include <unistd.h>
#include <pthread.h>

#include "csp/csp.hh"
#include "csp/interfaces/csp_if_kiss.h"

#include "asi/cxx/span.hh"
#include "asi/linux/epoll.hh"
#include "asi/debug/hexdump.hh"
#include "asi/linux/net.hh"

std::atomic_bool global_stop{false};

#define USAGE \
    "%s <my nodeid> <other nodeid>\n", \
    argv[0]

bool service_handler(csp_conn_t * conn, csp_packet_t *& packet)
{
    (void) conn;
    (void) packet;
    return false;
}

namespace
{
    net::ipv4::socket tcp_socket;
    net::ipv4::address tcp_addr;
    epoll::epoll loop;

    csp_iface_t * kiss_if = nullptr;
    csp_kiss_if_config_t kiss_conf;
}

// signal handler
void onsig(int sig)
{
    (void) sig;
    global_stop = true;
}

// recall: this is called while libcsp is transmitting
// a fully framed KISS message.
void csp_uapi_kiss_putc(csp_iface_t * interface, unsigned char data)
{
    // assuming KISS_TEST
    (void) interface;

    cxx::span<cxx::byte const> buffer_p{&data, 1};
    auto buffer = cxx::as_bytes(buffer_p);

    // This will generate system_error broken_pipe if the server
    // disconnects
    (void) tcp_socket.send(buffer, MSG_NOSIGNAL);
}

int main(int argc, char ** argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, USAGE);
        return EXIT_FAILURE;
    }

    csp::config c{};
    csp::server server{};
    int other_node = std::atoi(argv[2]);

    c.addr = std::atoi(argv[1]);
    c.hostname = "test";
    c.model = "test-machine";
    c.buffer_conf.num_buffers = 400;
    c.buffer_conf.buffer_size = 512;
    c.router_conf.stack_size = 2048;
    c.router_conf.prio = 1;
    c.rdp_conf.window_size = 6;
    c.rdp_conf.conn_timeout_ms = 30000;
    c.rdp_conf.packet_timeout_ms = 16000;
    c.rdp_conf.delayed_acks = 1;
    c.rdp_conf.ack_timeout = 8000;
    c.rdp_conf.ack_delay_count = 3;

    std::signal(SIGINT, onsig);

    // init csp
    csp::init(c);
    csp::set_debug_level(CSP_PACKET);

    // setup KISS interface
    kiss_conf.ifc = "KISS_TCP";
    kiss_conf.user_id = 1234;

    // initialize KISS
    if (NULL ==
        (kiss_if = ::csp_kiss_init(&kiss_conf)))
    {
        fprintf(stderr, "Failed to init KISS interface.\n");
        return EXIT_FAILURE;
    }

    // setup KISS to be the default route. Recall that the route
    // task is already started in csp::init(c)
    ::csp_route_set(CSP_DEFAULT_ROUTE, kiss_if, CSP_NODE_MAC);
    ::csp_route_print_table();

    // This doesnt use csp_thread_create intentionally to show
    // the interop with c++11 threads.
    std::thread server_thread(std::ref(server),
                              &service_handler, std::cref(global_stop));

    // just wait for the thread init to log stuff...
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // this is here and not in a CSP wrapper since it's VERY linux
    // specific and we wanted to show the manual std::thread interop
    ::pthread_setname_np(server_thread.native_handle(), "SRV");

    // initialize tcp socket
    tcp_socket = net::ipv4::socket(net::socket::type::stream, net::protocol::tcp);
    tcp_addr = net::ipv4::address("127.0.0.1", 9999);

    // wait for server
    while (true)
    {
        std::printf("Waiting for server tcp@9999...\n");

        try {
            tcp_socket.connect(tcp_addr);
            errno = 0; // whhhhy
            break;
        }
        catch (std::exception const & e) {
            std::fprintf(stderr, "%s\n", e.what());
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (global_stop)
            goto done;
    }

    // send a reverse route from dest node to this node
    // via KISS. NOTE 'KISS' here is for the AX100 and older
    // libcsp support specifically
    {
        csp_cmp_message m;
        std::memset(&m, 0, sizeof(m));

        m.route_set.dest_node = c.addr; /* this node */
        m.route_set.next_hop_mac = CSP_NODE_MAC;
        std::strcpy(m.route_set.interface, "KISS");

        std::printf("Sending route command...\n");
        (void) ::csp_cmp_route_set(other_node, 1000 /* ms */, &m);
    }

    std::printf("Connected!\n"
                " Send 'p' to ping node %d\n"
                " Send 'i' to identify node %d\n",
                other_node, other_node);

    // construct epoll events for TCP activity
    {
        epoll::event tcp_rx_event
        {
            tcp_socket.get_fd(),
            epoll::mode::READ,
            [] (epoll::event const & e)
            {
                (void) e;

                /* receive data into buffer */
                auto m = std::make_unique<unsigned char[]>(100);
                auto bb = cxx::span<unsigned char>{m.get(), 100};
                auto b = cxx::as_writable_bytes(bb);
                tcp_socket.recv(b);

                /* TODO we should be seeinbg EPOLL_PEER_HANGUP or something,
                 * if the server closes, but we simply read 0. This isn't a good
                 * test as stream sockets can get OOB data with read of 0. */
                if (b.size() == 0)
                {
                    global_stop = true;
                    return epoll::loop_action::remove_event;
                }

                /* insert into libcsp */
                (void) ::csp_kiss_rx(kiss_if, b.data(), b.size(), nullptr);

                return epoll::loop_action::none;
            }
        };

        epoll::event stdin_event
        {
            STDIN_FILENO,
            epoll::mode::READ,
            [other_node] (epoll::event const & e)
            {
                char x[256] = {0};
                auto s = ::read(e.get_fd(), x, sizeof(x));

                if (s > 1) {

                    if (x[0] == 'p')
                    {
                        std::uint32_t rt = 0;
                        if (csp::ping(other_node, 1000, 1, 0, &rt)) {
                            std::printf("PING REPLY IN %u ms\n", rt);
                        }
                        else {
                            std::printf("NO PING REPLY\n");
                        }
                    }
                    else if (x[0] == 'i')
                    {
                        csp::ident_response resp{};

                        if (csp::identify(other_node, 1000, resp))
                        {
                            std::printf("---- IDENTIFY ----\n"
                                        "hostname: %s\n"
                                        "model   : %s\n"
                                        "revision: %s\n"
                                        "date    : %s\n"
                                        "time    : %s\n",
                                        resp.hostname.c_str(),
                                        resp.model.c_str(),
                                        resp.revision.c_str(),
                                        resp.date.c_str(),
                                        resp._time.c_str());
                        }
                        else {
                            std::printf("NO IDENT REPLY!\n");
                        }
                    }
                }

                return epoll::loop_action::none;
            }
        };

        loop.add_event(std::move(tcp_rx_event));
        loop.add_event(std::move(stdin_event));
    }

    // Run the epoll loop as the main thread
    {
        std::error_code e;

        while (! global_stop)
        {
            e = loop.poll(std::chrono::milliseconds(10));

            if (!e) continue;
            if (e == std::errc::timed_out) { (void) 0; /* no op */ }
            else break;
        }

        if (e)
            std::cout << "epoll stop, " << e << std::endl;
    }

done:
    global_stop = true;
    server_thread.join();
}
