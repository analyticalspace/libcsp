#include <csp/csp.h>
#include <csp/csp_platform.h>
#include <csp/csp_compiler.h>
#include <csp/drivers/can.h>
#include <csp/interfaces/csp_if_can.h>

int CSP_COMPILER_WEAK csp_uapi_can_init(csp_can_if_config_t * conf)
{
    (void) conf;
    return CSP_ERR_DRIVER;
}

int CSP_COMPILER_WEAK csp_uapi_can_send(csp_iface_t * interface, can_id_t id,
                                        uint8_t const data[], uint8_t dlc)
{
    (void) interface;
    (void) id;
    (void) data;
    (void) dlc;

    return CSP_ERR_DRIVER;
}
