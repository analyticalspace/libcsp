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

/* SocketCAN driver
 *
 * Uses Linux's interface for socket-based CAN communication. If LIBSOCKETCAN
 * is included, it will be used to configure and manage the physical controller.
 */

#ifndef __linux__
#   error "Linux Specific. Please implement board-specific CAN support in another file"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/socket.h>

#include <csp/csp.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/drivers/can.h>
#include <csp/arch/csp_thread.h>

#ifdef CSP_HAVE_LIBSOCKETCAN
#   include <libsocketcan.h>
#endif

typedef struct {
    csp_can_if_config_t const * conf;
    csp_thread_handle_t rx_thread_hdl;
    char rx_thread_name[32];
    int socket;
} csp_socketcan_driver_t;

/* Create an array equal to that of csp_can_interfaces in csp_if_can.c.
 * This array will hold instances of the SOCKETCAN handle only. */
static csp_socketcan_driver_t drivers[CSP_CAN_MAX_INTERFACES];

static CSP_DEFINE_TASK(rx_thread)
{
    struct can_frame frame;
    int nbytes;

    /* get driver from the thread args so we can
     * get access to the socket */
    csp_socketcan_driver_t * driver =
        (csp_socketcan_driver_t *) param;

    csp_assert(driver);

    /* detach this since there is no driver destructor.
     * We can use this directly as this file is linux specific. */
    pthread_detach(pthread_self());

    while (1 /* forever */)
    {
        nbytes = read(driver->socket, &frame, sizeof(frame));

        if (nbytes < 0) {
            /* Use errno as we're linux specific */
            csp_log_error("socketcan %s: read: %s", __func__, strerror(errno));
            continue;
        }

        if (nbytes != sizeof(frame)) {
            csp_log_warn("Read incomplete CAN frame");
            continue;
        }

        /* Avoid error and RTR frames */
        if (frame.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG) || !(frame.can_id & CAN_EFF_FLAG))
        {
            /* Drop error and remote frames */
            csp_log_warn("Discarding ERR/RTR/SFF frame");
            continue;
        }

        frame.can_id &= CAN_EFF_MASK;

        /* Call RX callback */
        csp_can_rx(driver->conf->iface, frame.can_id, frame.data, frame.can_dlc, NULL);
    }

    /* We should never reach this point */
    csp_thread_exit();
}

int csp_uapi_can_send(csp_iface_t * interface, can_id_t id, uint8_t const data[], uint8_t dlc)
{
    int i;
    int ret = CSP_ERR_NONE;
    csp_socketcan_driver_t * driver = interface->driver;
    struct can_frame frame;

    memset(&frame, 0, sizeof(frame));

    /* canfd not supported... */
    if (dlc > 8)
        return CSP_ERR_INVAL;

    if (driver->conf->use_extended_mask) {
        frame.can_id = id | (CAN_EFF_FLAG & ~CAN_SFF_MASK);
    }
    else {
        frame.can_id = id | (CAN_SFF_MASK & ~CAN_EFF_MASK);
    }

    /* Copy data to frame */
    for (i = 0; i < dlc; i++)
        frame.data[i] = data[i];

    /* Set DLC */
    frame.can_dlc = dlc;

    /* Send frame */
    while (write(driver->socket, &frame, sizeof(frame)) != sizeof(frame))
    {
        /* Wait 10 ms and try again */
        if (errno == ENOBUFS) {
            csp_sleep_ms(10);
        }
        else {
            csp_log_error("%s: write: %s", __func__, strerror(errno));
            ret = CSP_ERR_DRIVER;
            break;
        }
    }

    return ret;
}

int csp_uapi_can_init(csp_can_if_config_t * conf)
{
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_filter filter;

    csp_socketcan_driver_t * driver = NULL;

    csp_assert(conf);

    if (conf->instance > (CSP_CAN_MAX_INTERFACES - 1))
    {
        csp_log_error("%s: Garbage instance '%u' > '%u'",
                      __func__, conf->instance, (CSP_CAN_MAX_INTERFACES - 1));
        return CSP_ERR_INVAL;
    }

    /* initialize pointers and state */
    driver = &drivers[conf->instance];
    driver->socket = -1;
    driver->conf = conf;

    snprintf(driver->rx_thread_name, sizeof(driver->rx_thread_name),
             "%sRx", driver->conf->ifc);

#ifdef CSP_HAVE_LIBSOCKETCAN
    /* Set interface up via socketcan. If we don't have this, then
     * the device must be configured externally. */
    if (conf->bitrate > 0) {
        can_do_stop(conf->ifc);
        can_set_bitrate(conf->ifc, conf->bitrate);
        can_set_restart_ms(conf->ifc, 100);
        can_do_start(conf->ifc);
    }
#endif

    /* Create socket */
    if ((driver->socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        csp_log_error("%s: socket: %s", __func__, strerror(errno));
        goto init_error;
    }

    /* Locate interface */
    strncpy(ifr.ifr_name, conf->ifc, IFNAMSIZ - 1);

    if (ioctl(driver->socket, SIOCGIFINDEX, &ifr) < 0) {
        csp_log_error("%s: ioctl: %s", __func__, strerror(errno));
        goto init_error;
    }

    /* Bind the socket to CAN interface */
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(driver->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        csp_log_error("%s: bind: %s", __func__, strerror(errno));
        goto init_error;
    }

    /* build and apply the filter */
    filter.can_id = conf->id;
    filter.can_mask = conf->mask;

    if (! conf->use_extended_mask) {
        filter.can_mask |= (CAN_EFF_MASK & ~CAN_SFF_MASK);
    }

    if (setsockopt(driver->socket,
                   SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter)) < 0)
    {
        csp_log_error("%s: setsockopt: %s", __func__, strerror(errno));
        goto init_error;
    }

    /* set the socketcan driver as the opaque data in the interface */
    conf->iface->driver = driver;

    /* Create receive thread */
    if (CSP_ERR_NONE !=
        csp_thread_create(rx_thread, driver->rx_thread_name, conf->impl_task_stack_size,
                          (void *)(&drivers[conf->instance]),
                          conf->impl_task_priority, &driver->rx_thread_hdl))
    {
        /* Using errno here as this file is linux specific */
        csp_log_error("%s: csp_thread_create: %s", __func__, strerror(errno));
        goto init_error;
    }

    return CSP_ERR_NONE;

init_error:
    if (driver->socket != -1)
        close(driver->socket);

    return CSP_ERR_DRIVER;
}
