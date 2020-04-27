#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zmq.h>

int main(int argc, char** argv)
{
    if(argc >= 2 && strcmp(argv[1], "-h") == 0)
    {
        fprintf(stderr, "Usage %s: <pub_url> <sub_url>\n", argv[0]);
        return EXIT_FAILURE;
    }

    void * context = zmq_ctx_new();
    void * xpub_out = zmq_socket(context, ZMQ_XPUB);
    void * xsub_in = zmq_socket(context, ZMQ_XSUB);

    if (zmq_bind(xpub_out, argc >= 2 ? argv[1] : "tcp://*:7000") != 0 ||
        zmq_bind(xsub_in, argc >= 3 ? argv[2] : "tcp://*:6000") != 0)
    {
        fprintf(stderr, "Failed to bind to hosts\n");
        return EXIT_FAILURE;
    }

    /* block */
    zmq_proxy(xpub_out, xsub_in, NULL);

    return 0;
}
