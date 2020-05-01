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

#include <stdint.h>
#include <string.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/csp_crc32.h>

#define CSP_KISS_MTU (256)

#define FEND                (0xC0)
#define FESC                (0xDB)
#define TFEND               (0xDC)
#define TFESC               (0xDD)

#define TNC_DATA            (0x00)
#define TNC_SET_HARDWARE    (0x06)
#define TNC_RETURN          (0xFF)

static const char * default_kiss_ifc_name = "KISS";
static csp_iface_t csp_kiss_interfaces[CSP_KISS_MAX_INTERFACES];
static csp_kiss_handle_t csp_kiss_handles[CSP_KISS_MAX_INTERFACES];
static size_t csp_kiss_interfaces_count = 0;

static int csp_kiss_tx(csp_iface_t * interface, csp_packet_t * packet, uint32_t timeout)
{
    (void) timeout;

    csp_assert(interface);

    /* Add CRC32 checksum */
    csp_crc32_append(packet, false);

    /* Save the outgoing id in the buffer */
    packet->id.ext = csp_hton32(packet->id.ext);
    packet->length += sizeof(packet->id.ext);

    /* Transmit data */
    csp_uapi_kiss_putc(interface, FEND);
    csp_uapi_kiss_putc(interface, TNC_DATA);

    for (unsigned int i = 0; i < packet->length; i++)
    {
        if (((unsigned char *) &packet->id.ext)[i] == FEND)
        {
            ((unsigned char *) &packet->id.ext)[i] = TFEND;
            csp_uapi_kiss_putc(interface, FESC);
        }
        else if (((unsigned char *) &packet->id.ext)[i] == FESC)
        {
            ((unsigned char *) &packet->id.ext)[i] = TFESC;
            csp_uapi_kiss_putc(interface, FESC);
        }

        csp_uapi_kiss_putc(interface,
                           ((unsigned char *) &packet->id.ext)[i]);
    }

    csp_uapi_kiss_putc(interface, FEND);
    csp_buffer_free(packet);

    return CSP_ERR_NONE;
}

int csp_kiss_rx(csp_iface_t * interface, uint8_t * buf, uint32_t len,
                CSP_BASE_TYPE * task_woken)
{
    csp_kiss_handle_t * driver =
        (csp_kiss_handle_t *) interface->driver;

    while (len--)
    {
        /* Input */
        unsigned char inputbyte = *buf++;

        /* If packet was too long */
        if (driver->rx_length > (unsigned int)(interface->mtu + CSP_HEADER_LENGTH))
        {
            //csp_log_warn("KISS RX overflow");
            interface->rx_error++;
            driver->rx_mode = KISS_MODE_NOT_STARTED;
            driver->rx_length = 0;
        }

        switch (driver->rx_mode)
        {
        case KISS_MODE_NOT_STARTED:
            {
                /* Send normal chars back to driver for handling */
                if (inputbyte != FEND) {
                    csp_uapi_kiss_discard(interface, inputbyte, task_woken);
                    break;
                }

                /* Try to allocate new buffer */
                if (driver->rx_packet == NULL) {
                    if (task_woken == NULL) {
                        driver->rx_packet = csp_buffer_get(interface->mtu);
                    } else {
                        driver->rx_packet = csp_buffer_get_isr(interface->mtu);
                    }
                }

                /* If no more memory, skip frame */
                if (driver->rx_packet == NULL) {
                    driver->rx_mode = KISS_MODE_SKIP_FRAME;
                    break;
                }

                /* Start transfer */
                driver->rx_length = 0;
                driver->rx_mode = KISS_MODE_STARTED;
                driver->rx_first = 1;
            }
            break;
        case KISS_MODE_STARTED:
            {
                /* Escape char */
                if (inputbyte == FESC)
                {
                    driver->rx_mode = KISS_MODE_ESCAPED;
                    break;
                }

                /* End Char */
                if (inputbyte == FEND)
                {
                    /* Accept message */
                    if (driver->rx_length > 0)
                    {
                        /* Check for valid length */
                        if (driver->rx_length < CSP_HEADER_LENGTH + sizeof(uint32_t)) {
                            //csp_log_warn("KISS short frame skipped, len: %u", driver->rx_length);
                            interface->rx_error++;
                            driver->rx_mode = KISS_MODE_NOT_STARTED;
                            break;
                        }

                        /* Count received frame */
                        interface->frame++;

                        /* The CSP packet length is without the header */
                        driver->rx_packet->length = driver->rx_length - CSP_HEADER_LENGTH;

                        /* Convert the packet from network to host order */
                        driver->rx_packet->id.ext = csp_ntoh32(driver->rx_packet->id.ext);

                        /* Validate CRC */
                        if (csp_crc32_verify(driver->rx_packet, false) != CSP_ERR_NONE) {
                            //csp_log_warn("KISS invalid crc frame skipped, len: %u", driver->rx_packet->length);
                            interface->rx_error++;
                            driver->rx_mode = KISS_MODE_NOT_STARTED;
                            break;
                        }

                        /* Send back into CSP, notice calling from task so last argument must be NULL! */
                        csp_qfifo_write(driver->rx_packet, interface, task_woken);
                        driver->rx_packet = NULL;
                        driver->rx_mode = KISS_MODE_NOT_STARTED;
                        break;
                    }

                    /* Break after the end char */
                    break;
                }

                /* Skip the first char after FEND which is TNC_DATA (0x00) */
                if (driver->rx_first) {
                    driver->rx_first = 0;
                    break;
                }

                /* Valid data char */
                ((char *) &driver->rx_packet->id.ext)[driver->rx_length++] = inputbyte;
            }
            break;

        case KISS_MODE_ESCAPED:
            {
                /* Escaped escape char */
                if (inputbyte == TFESC)
                    ((char *) &driver->rx_packet->id.ext)[driver->rx_length++] = FESC;

                /* Escaped fend char */
                if (inputbyte == TFEND)
                    ((char *) &driver->rx_packet->id.ext)[driver->rx_length++] = FEND;

                /* Go back to started mode */
                driver->rx_mode = KISS_MODE_STARTED;
            }
            break;
        case KISS_MODE_SKIP_FRAME:
            {
                /* Just wait for end char */
                if (inputbyte == FEND)
                    driver->rx_mode = KISS_MODE_NOT_STARTED;
            }
            break;
        } /* switch */
    } /* while (len--) */

    return CSP_ERR_NONE;
}

csp_iface_t * csp_kiss_init(csp_kiss_if_config_t * conf)
{
    csp_assert(conf);

    if (csp_kiss_interfaces_count >= (CSP_KISS_MAX_INTERFACES-1)) {
        csp_log_error("Too many KISS interfaces created.");
        return NULL;
    }

    if (strlen(conf->ifc) == 0) {
        csp_log_warn("Setting KISS interface name to '%s'", default_kiss_ifc_name);
        conf->ifc = default_kiss_ifc_name;
    }

    if (csp_iflist_get_by_name(conf->ifc)) {
        csp_log_error("KISS interface with name '%s' already exists", conf->ifc);
        return NULL;
    }

    /* setup interface */
    csp_iface_t * new_if = &csp_kiss_interfaces[csp_kiss_interfaces_count];
    new_if->name = conf->ifc;
    new_if->mtu = CSP_KISS_MTU;
    new_if->nexthop = csp_kiss_tx;

    /* setup handle */
    new_if->driver = &csp_kiss_handles[csp_kiss_interfaces_count];
    ((csp_kiss_handle_t *)(new_if->driver))->rx_packet = NULL;
    ((csp_kiss_handle_t *)(new_if->driver))->rx_mode = KISS_MODE_NOT_STARTED;
    ((csp_kiss_handle_t *)(new_if->driver))->driver_data = conf;

    /* Regsiter interface */
    csp_iflist_add(new_if);
    csp_kiss_interfaces_count += 1;

    return new_if;
}
