#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "csp/csp.h"
#include "csp/arch/csp_thread.h"
#include "csp/interfaces/csp_if_can.h"

static csp_can_if_config_t conf = {
    .ifc = "vcan0",
    .use_extended_mask = true,
    .bitrate = 1000000,
    .impl_task_stack_size = 0,
    .impl_task_priority = 0
};

static csp_iface_t * csp_can;
static csp_thread_handle_t service_handler_hdl;

static CSP_DEFINE_TASK(service_handler)
{
    size_t counter = 0;
    int csp_err;
    csp_socket_t * socket = csp_socket(0);
    csp_conn_t * conn = NULL;
    csp_packet_t * packet = NULL;

    if (CSP_ERR_NONE !=
        (csp_err = csp_bind(socket, CSP_ANY)))
    {
        fprintf(stderr, "Failed to bind with err: %d\n", csp_err);
        goto init_error;
    }

    if (CSP_ERR_NONE !=
        (csp_err = csp_listen(socket, CSP_CONN_QUEUE_LENGTH)))
    {
        fprintf(stderr, "Failed to listen with err: %d\n", csp_err);
        goto init_error;
    }

    while (1)
    {
        if (NULL ==
            (conn = csp_accept(socket, 1000)))
        {
            continue;
        }

        while ((packet = csp_read(conn, 0)) != NULL)
        {
            switch (csp_conn_dport(conn))
            {
            default:
                {
                    csp_service_handler(conn, packet);
                    counter += 1;
                    fprintf(stderr, "Counter: %zu\n", counter);
                }
                break;
            }
        }

        if (conn)
            csp_close(conn);
    }

init_error:
    csp_thread_exit();
}

int main(int argc, char ** argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <my_addr> <their_addr>\n", argv[0]);
        return EXIT_FAILURE;
    }

    csp_set_hostname("test1");
    csp_set_model("test-machine");
    csp_set_revision("v1.0");
    csp_buffer_init(400, 512);
    csp_debug_set_level(CSP_ERROR, 1);
    csp_debug_set_level(CSP_WARN, 1);
    csp_debug_set_level(CSP_INFO, 1);
    csp_debug_set_level(CSP_BUFFER, 0);
    csp_debug_set_level(CSP_PACKET, 0);
    csp_debug_set_level(CSP_PROTOCOL, 0);
    csp_debug_set_level(CSP_LOCK, 0);

    if (CSP_ERR_NONE != csp_init(atoi(argv[1])))
    {
        fprintf(stderr, "Failed to init csp\n");
        return EXIT_FAILURE;
    }

    if (NULL ==
        (csp_can = csp_can_init(&conf)))
    {
        fprintf(stderr, "Failed to init can\n");
        return EXIT_FAILURE;
    }

    csp_route_set(CSP_DEFAULT_ROUTE, csp_can, CSP_NODE_MAC);
    csp_route_print_table();
    csp_route_start_task(0, 0);

    csp_thread_create(service_handler, "SRV", 0, NULL, 0,
                      &service_handler_hdl);

    // max size of pings for the loop below
    const unsigned int max = 32;

    while (1)
    {
        if (strcmp(argv[2], "-") != 0)
        {
            csp_ping(atoi(argv[2]), 1000, 1, 0);
            csp_ping(atoi(argv[2]), 1000, (1 + (rand() / (RAND_MAX / (max-1) + 1))), 0);
        }
    }

    return EXIT_SUCCESS;
}
