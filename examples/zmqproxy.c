/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <zmq.h>

#include <csp/csp.h>

#define USAGE \
	"Usage: %s [-d N] [-h]\n" \
	"zmq_proxy(3) wrapper for libcsp with XPUB @ tcp://*:7000 and XSUB @ tcp://*:6000\n" \
	"A capture thread is started at tcp://localhost:7000 to log traffic\n" \
	"\nOptions:\n" \
	"  -d : Debug level, range 0-6\n" \
	"\n"

static sig_atomic_t stop = 0;
static int lingertime = 0;

static void * task_capture(void *ctx) {

	/* Subscriber (RX) */
	void *subscriber = zmq_socket(ctx, ZMQ_SUB);
	zmq_setsockopt(subscriber, ZMQ_LINGER, &lingertime, sizeof(lingertime));
	zmq_connect(subscriber, "tcp://localhost:7000");
	zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);

	/* Allocated 'raw' CSP packet */
	csp_packet_t * packet = malloc(1024);
	assert(packet != NULL);

	while (! stop) {
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, 1024);

		/* Receive data */
		if (zmq_msg_recv(&msg, subscriber, ZMQ_DONTWAIT) < 0) {
			if (EAGAIN == zmq_errno()) {
				zmq_msg_close(&msg);
				usleep(100000);
				continue;
			}
			else {
				zmq_msg_close(&msg);
				csp_log_error("ZMQ: %s\r\n", zmq_strerror(zmq_errno()));
				continue;
			}
		}

		int datalen = zmq_msg_size(&msg);
		if (datalen < 5) {
			csp_log_warn("ZMQ: Too short datalen: %u\r\n", datalen);
			while(zmq_msg_recv(&msg, subscriber, ZMQ_NOBLOCK) > 0)
				zmq_msg_close(&msg);
			continue;
		}

		/* Copy the data from zmq to csp */
		char * satidptr = ((char *) &packet->id) - 1;
		memcpy(satidptr, zmq_msg_data(&msg), datalen);
		packet->length = datalen - sizeof(packet->id) - 1;

		csp_log_packet("Input: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %" PRIu16,
					   packet->id.src, packet->id.dst, packet->id.dport,
					   packet->id.sport, packet->id.pri, packet->id.flags, packet->length);

		zmq_msg_close(&msg);
	}

	zmq_disconnect(subscriber, "tcp://localhost:7000");
	zmq_close(subscriber);
	free(packet);

	return NULL;
}

static void on_sig(int s) {

	(void) s;
	stop = 1;
}

static void csp_log_hook(csp_debug_level_t level, const char *format, va_list args)
{
	(void) level;
	vprintf(format, args);
	printf("\r\n");
	fflush(stdout);
}

int main(int argc, char ** argv) {

	csp_debug_level_t debug_level = CSP_PACKET;
	int opt;

	while ((opt = getopt(argc, argv, "d:h")) != -1) {
		switch (opt) {
			case 'd':
				debug_level = atoi(optarg);
				break;
			default:
				fprintf(stderr, USAGE, argv[0]);
				return EXIT_FAILURE;
				break;
		}
	}

	/* enable/disable debug levels */
	for (csp_debug_level_t i = 0; i <= CSP_LOCK; ++i) {
		csp_debug_set_level(i, (i <= debug_level) ? true : false);
	}

	csp_debug_hook_set(csp_log_hook);

	signal(SIGINT, on_sig);

	void * ctx = zmq_ctx_new();
	assert(ctx);

	void *frontend = zmq_socket(ctx, ZMQ_XSUB);
	assert(frontend);
	zmq_setsockopt(frontend, ZMQ_LINGER, &lingertime, sizeof(lingertime));
	zmq_bind(frontend, "tcp://*:6000");

	void *backend = zmq_socket(ctx, ZMQ_XPUB);
	assert(backend);
	zmq_setsockopt(backend, ZMQ_LINGER, &lingertime, sizeof(lingertime));
	zmq_bind(backend, "tcp://*:7000");

	pthread_t capworker;
	pthread_create(&capworker, NULL, task_capture, ctx);

	csp_log_info("Starting ZMQproxy");
	zmq_proxy(frontend, backend, NULL);

	// wait for control-c
	pthread_join(capworker, NULL);

	csp_log_info("Closing ZMQproxy");

	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(ctx);

	return EXIT_SUCCESS;
}
