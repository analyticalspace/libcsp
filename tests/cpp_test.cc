#include <cstring>
#include <atomic>
#include <thread>
#include <unistd.h>

#include "csp/csp.hh"

std::atomic_bool global_stop{false};

#define MY_TEST_PORT (10)

bool service_handler(csp_conn_t * conn,
                     csp_packet_t *& packet)
{
    auto dport = ::csp_conn_dport(conn);

    switch (dport)
    {
    case MY_TEST_PORT:
        {
            while ((packet = ::csp_read(conn, 0)) != nullptr)
            {
                std::printf("GOT USER DATA VIA CUSTOM SERVICE HANDLER!: %s\n",
                            (const char *)packet->data);
                ::csp_buffer_free(packet);
            }

            ::csp_close(conn);
            return true;
        }
        break;
    };

    return false;
}

int main()
{
    csp::config c{};
    c.addr = 8;
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

    csp::init(c);
    csp::server server{};
    csp::set_debug_level(CSP_BUFFER);

    // This doesnt use csp_thread_create intentionally to show
    // the interop with c++11 threads.
    std::thread server_thread(std::ref(server),
                              &service_handler, std::cref(global_stop));

    // this is here and not in a CSP wrapper since it's VERY linux
    // specific and we wanted to show the manual std::thread interop
    ::pthread_setname_np(server_thread.native_handle(), "SRV");

    /* test a ping */
    {
        csp::ping(c.addr /* self */, 1000);
    }

    /* test identify */
    {
        csp::ident_response resp;

        if (! csp::identify(c.addr /* self */, 1000, resp))
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
    }

    /* run 'ps' on ourself */
    {
        ::csp_ps(c.addr, 1000);
    }

    /* test the custom service handler */
    {
        const char * buffers[2] = {
            "hello!\0",
            "goodbye!\0"
        };

        (void) ::csp_transaction(CSP_PRIO_NORM, c.addr /* self */,
                                 MY_TEST_PORT, 1000, (void *)buffers[0],
                                 std::strlen(buffers[0]), nullptr, 0);

        (void) ::csp_transaction(CSP_PRIO_NORM, c.addr /* self */,
                                 MY_TEST_PORT, 1000, (void *)buffers[1],
                                 std::strlen(buffers[1]), nullptr, 0);

        // give the router a chance to ingest the packets,
        // and give the service handler a bit of time to respond.
        ::usleep(5000);
    }

    global_stop = true;
    server_thread.join();
}
