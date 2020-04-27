#include <csp/csp.h>
#include <csp/csp_platform.h>
#include <csp/csp_compiler.h>
#include <csp/drivers/i2c.h>
#include <csp/interfaces/csp_if_i2c.h>

int CSP_COMPILER_WEAK csp_uapi_i2c_init(csp_i2c_if_config_t const * conf)
{
    (void) conf;
    return CSP_ERR_DRIVER;
}

int CSP_COMPILER_WEAK csp_uapi_i2c_send(int handle, i2c_frame_t const * frame, uint16_t timeout)
{
    (void) handle;
    (void) frame;
    (void) timeout;

    return CSP_ERR_TX;
}

