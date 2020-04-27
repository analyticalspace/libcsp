#include <csp/csp.h>
#include <csp/csp_platform.h>
#include <csp/csp_compiler.h>

void CSP_COMPILER_WEAK csp_uapi_kiss_putc(csp_iface_t * interface, unsigned char buf)
{
    (void) interface;
    (void) buf;
}

void CSP_COMPILER_WEAK csp_uapi_kiss_discard(csp_iface_t * interface,
                                             unsigned char c, CSP_BASE_TYPE * task_woken)
{
    (void) interface;
    (void) c;
    (void) task_woken;
}
