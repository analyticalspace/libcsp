#include <stdint.h>
#include <inttypes.h>

#include "csp/csp.h"
#include "csp/csp_endian.h"
#include "csp/csp_debug.h"
#include "csp/csp_cmp.h"

COMMAND_MODULE_PROLOG()

COMMAND_DEFINE(cmd_uptime)
{
    COMMAND_CTX_INIT(1);
    COMMAND_ARG_UNSIGNED_INTEGRAL(node, uint8_t, 1, 32);
    COMMAND_CTX_END();

    uint32_t uptime = 0;

    int status =
        csp_transaction(CSP_PRIO_NORM, node, CSP_UPTIME, 2000,
                        NULL, 0, &uptime, sizeof(uptime));
    
    if (status == 0) {
        COMMAND_RESULT(NULL, "Timeout after %" PRIu32 "ms", 2000);
        COMMAND_RETURN(COMMAND_ERROR_FAIL);
    }

    uptime = csp_ntoh32(uptime);

    COMMAND_RESULT(NULL,
                   "Uptime %" PRIu32 " seconds", uptime);

    COMMAND_RETURN(COMMAND_SUCCESS);
}

COMMAND_DEFINE(cmd_ping)
{
    uint32_t timeout = 1000, size = 1;
    uint32_t options = 0;
    uint8_t node = 0;

    if (COMMAND_CTX_ARGC() == 1)
    {
        COMMAND_CTX_INIT(1);
        COMMAND_ARG_UNSIGNED_INTEGRAL(_node, uint8_t, 1, 32);
        COMMAND_CTX_END();

        node = _node;
    }
    else if (COMMAND_CTX_ARGC() == 2)
    {
        COMMAND_CTX_INIT(2);
        COMMAND_ARG_UNSIGNED_INTEGRAL(_node, uint8_t, 1, 32);
        COMMAND_ARG_STRING(optstring);
        COMMAND_CTX_END();

        node = _node;

        if (strchr(optstring, 'r'))
            options |= CSP_O_RDP;
        if (strchr(optstring, 'x'))
            options |= CSP_O_XTEA;
        if (strchr(optstring, 'h'))
            options |= CSP_O_HMAC;
        if (strchr(optstring, 'c'))
            options |= CSP_O_CRC32;
    }
    else if (COMMAND_CTX_ARGC() == 3)
    {
        COMMAND_CTX_INIT(3);
        COMMAND_ARG_UNSIGNED_INTEGRAL(_node, uint8_t, 1, 32);
        COMMAND_ARG_STRING(optstring);
        COMMAND_ARG_UNSIGNED_INTEGRAL(_size, uint8_t, 1, UINT8_MAX);
        COMMAND_CTX_END();

        node = _node;
        size = _size;

        if (strchr(optstring, 'r'))
            options |= CSP_O_RDP;
        if (strchr(optstring, 'x'))
            options |= CSP_O_XTEA;
        if (strchr(optstring, 'h'))
            options |= CSP_O_HMAC;
        if (strchr(optstring, 'c'))
            options |= CSP_O_CRC32;
    }

    COMMAND_RESULT(NULL,
                   "Ping: node %" PRIu8 ", timeout %" PRIu32 ", size %" PRIu32
                   ", options 0x%02" PRIX32, node, timeout, size, options);

    int p = csp_ping(node, timeout, size, options);

    if (p <= 0) {
        COMMAND_RESULT(NULL, "Timeout after %" PRIu32 "ms", timeout);
        COMMAND_RETURN(COMMAND_ERROR_FAIL);
    }

    COMMAND_RESULT(NULL, "Reply in %d ms", p);
    COMMAND_RETURN(COMMAND_SUCCESS);
}

COMMAND_DEFINE(cmd_ident)
{
    COMMAND_CTX_INIT(1);
    COMMAND_ARG_UNSIGNED_INTEGRAL(node, uint8_t, 1, 32);
    COMMAND_CTX_END();

    uint32_t timeout = 1000;
    struct csp_cmp_message msg;

    int ret = csp_cmp_ident(node, timeout, &msg);

    if (ret != CSP_ERR_NONE)
    {
        COMMAND_RESULT(NULL,
                       "Cannot access node %u, error: %d", node, ret);

        COMMAND_RETURN(COMMAND_ERROR_FAIL);
    }

    COMMAND_RESULT(NULL,
                   "Hostname: %s\r\n"
                   "Model:    %s\r\n"
                   "Revision: %s\r\n"
                   "Date:     %s\r\n"
                   "Time:     %s",
                   msg.ident.hostname, msg.ident.model,
                   msg.ident.revision,
                   msg.ident.date, msg.ident.time);

    COMMAND_RETURN(COMMAND_SUCCESS);
}

COMMAND_DEFINE(cmd_debug)
{
    COMMAND_CTX_INIT(1);
    COMMAND_ARG_UNSIGNED_INTEGRAL(level, uint8_t, CSP_ERROR, CSP_LOCK);
    COMMAND_CTX_END();

    csp_debug_set_level(CSP_ERROR,    level >= CSP_ERROR    ? true : false);
    csp_debug_set_level(CSP_WARN,     level >= CSP_WARN     ? true : false);
    csp_debug_set_level(CSP_INFO,     level >= CSP_INFO     ? true : false);
    csp_debug_set_level(CSP_BUFFER,   level >= CSP_BUFFER   ? true : false);
    csp_debug_set_level(CSP_PACKET,   level >= CSP_PACKET   ? true : false);
    csp_debug_set_level(CSP_PROTOCOL, level >= CSP_PROTOCOL ? true : false);
    csp_debug_set_level(CSP_LOCK,     level >= CSP_LOCK     ? true : false);

    COMMAND_RESULT(NULL,
                   "libcsp debug level set to %" PRIu8, level);

    COMMAND_RETURN(COMMAND_SUCCESS);
}

COMMAND_DEFINE(cmd_ps)
{
    COMMAND_CTX_INIT(2);
    COMMAND_ARG_UNSIGNED_INTEGRAL(node, uint8_t, 1, 32);
    COMMAND_ARG_UNSIGNED_INTEGRAL(timeout, uint32_t, 1, 10000);
    COMMAND_CTX_END();

    csp_ps(node, timeout);

    COMMAND_RETURN(COMMAND_SUCCESS);
}

COMMAND_MODULE_EPILOG()

