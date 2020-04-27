#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "csp/csp.h"
#include "csp/arch/csp_thread.h"
#include "csp/interfaces/csp_if_kiss.h"

static csp_kiss_if_config_t conf = {
    .ifc = "KISS_TEST",
    .user_id = 1234,
};

static csp_iface_t * kiss_if;
static csp_thread_handle_t service_handler_hdl;
static csp_thread_handle_t kiss_rx_thread_hdl;
static int fifo_a_fd = -1;
static int fifo_b_fd = -1;
static sig_atomic_t stop = 0;

static CSP_DEFINE_TASK(service_handler)
{
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
                csp_service_handler(conn, packet);
                break;
            }
        }

        if (conn)
            csp_close(conn);
    }

init_error:
    csp_thread_exit();
}

/* See NOTE1 */
static CSP_DEFINE_TASK(kiss_rx_thread)
{
    unsigned char buf[1];
    size_t len;

    while (1)
    {
        len = read(fifo_b_fd, buf, sizeof(buf));

        if (len == 0) break;

        /* assuming KISS_TEST */
        (void) csp_kiss_rx(kiss_if, buf, (uint32_t)len, NULL);
    }

    csp_thread_exit();
}

void csp_uapi_kiss_putc(csp_iface_t * interface, unsigned char buf)
{
    /* assuming KISS_TEST */
    (void) interface;
    (void) write(fifo_a_fd, &buf, sizeof(buf));
}

void onsig(int sig) { stop = sig; }

int main(int argc, char ** argv)
{
    signal(SIGINT, onsig);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <my_addr> <their_addr>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* create fifo names */
    char fifo_a[108] = {0};
    char fifo_b[108] = {0};
    sprintf(fifo_a, "/tmp/kiss_test_fifo_%s_to_%s", argv[1], argv[2]);
    sprintf(fifo_b, "/tmp/kiss_test_fifo_%s_to_%s", argv[2], argv[1]);

    printf("FIFO %s->%s: '%s'\n", argv[1], argv[2], fifo_a);
    printf("FIFO %s->%s: '%s'\n", argv[2], argv[1], fifo_b);

    (void) mkfifo(fifo_a, S_IRUSR | S_IWUSR);
    (void) mkfifo(fifo_b, S_IRUSR | S_IWUSR);

    csp_set_hostname("test1");
    csp_set_model("test-machine");
    csp_set_revision("v1.0");
    csp_buffer_init(400, 512);
    csp_debug_set_level(CSP_ERROR, 1);
    csp_debug_set_level(CSP_WARN, 1);
    csp_debug_set_level(CSP_INFO, 1);
    csp_debug_set_level(CSP_BUFFER, 0);
    csp_debug_set_level(CSP_PACKET, 1);
    csp_debug_set_level(CSP_PROTOCOL, 0);
    csp_debug_set_level(CSP_LOCK, 0);

    int a = atoi(argv[1]);
    int b = atoi(argv[2]);

    /* GROSS. We have to open fifos in order so we don't
     * deadlock. There must be a better way... */
    if (a > b)
    {
        printf("Opening %s...\n", fifo_b);
        if (0 > (fifo_b_fd = open(fifo_b, O_RDONLY))) { perror("open B"); goto fifo_init_error; }
        printf("Opening %s...\n", fifo_a);
        if (0 > (fifo_a_fd = open(fifo_a, O_WRONLY))) { perror("open A"); goto fifo_init_error; }
    }
    else {
        printf("Opening %s...\n", fifo_a);
        if (0 > (fifo_a_fd = open(fifo_a, O_WRONLY))) { perror("open A"); goto fifo_init_error; }
        printf("Opening %s...\n", fifo_b);
        if (0 > (fifo_b_fd = open(fifo_b, O_RDONLY))) { perror("open B"); goto fifo_init_error; }
    }

    if (CSP_ERR_NONE != csp_init(atoi(argv[1])))
    {
        fprintf(stderr, "Failed to init csp\n");
        return EXIT_FAILURE;
    }

    if (NULL ==
        (kiss_if = csp_kiss_init(&conf)))
    {
        fprintf(stderr, "Failed to init KISS interface.\n");
        return EXIT_FAILURE;
    }

    csp_route_set(CSP_DEFAULT_ROUTE, kiss_if, CSP_NODE_MAC);
    csp_route_print_table();
    csp_route_start_task(1000, 0);

    /* NOTE1
     * Since this is a linux test, we just create a thread to "poll"
     * the KISS read fifo. On embedded platforms this would be an interrupt
     * service routine - but could be a polling thing if you wanted.
     */
    csp_thread_create(kiss_rx_thread, "READ", 0, NULL, 0,
                      &kiss_rx_thread_hdl);

    csp_thread_create(service_handler, "SRV", 0, NULL, 0,
                      &service_handler_hdl);

    while (! stop) {
        csp_ping(atoi(argv[2]), 1000, 1, 0);
        csp_sleep_ms(1000);
    }

fifo_init_error:
    (void) close(fifo_a_fd);
    (void) close(fifo_b_fd);
    printf("REMOVE FILE: %s\n", fifo_a);
    printf("REMOVE FILE: %s\n", fifo_b);

    return EXIT_SUCCESS;
}
