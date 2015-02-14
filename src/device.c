/*
 * co2mon - programming interface to CO2 sensor.
 * Copyright (C) 2015  Oleg Bulatov <oleg@bulatov.me>

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "device.h"

#ifndef HAVE_LIBUSB_STRERROR
const char *
libusb_strerror(enum libusb_error errcode)
{
    switch (errcode)
    {
    case LIBUSB_SUCCESS:
        return "Success";
    case LIBUSB_ERROR_IO:
        return "Input/output error";
    case LIBUSB_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case LIBUSB_ERROR_ACCESS:
        return "Access denied (insufficient permissions)";
    case LIBUSB_ERROR_NO_DEVICE:
        return "No such device (it may have been disconnected)";
    case LIBUSB_ERROR_NOT_FOUND:
        return "Entity not found";
    case LIBUSB_ERROR_BUSY:
        return "Resource busy";
    case LIBUSB_ERROR_TIMEOUT:
        return "Operation timed out";
    case LIBUSB_ERROR_OVERFLOW:
        return "Overflow";
    case LIBUSB_ERROR_PIPE:
        return "Pipe error";
    case LIBUSB_ERROR_INTERRUPTED:
        return "System call interrupted (perhaps due to signal)";
    case LIBUSB_ERROR_NO_MEM:
        return "Insufficient memory";
    case LIBUSB_ERROR_NOT_SUPPORTED:
        return "Operation not supported or unimplemented on this platform";
    case LIBUSB_ERROR_OTHER:
        return "Other error";
    }
    return "Unknown error";
}
#endif

static int
is_co2_device(libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0)
    {
        fprintf(stderr, "libusb_get_device_descriptor: %s\n", libusb_strerror(r));
        return 0;
    }

    return desc.idVendor == 0x04d9 && desc.idProduct == 0xa052;
}

libusb_device *
co2mon_find_device(void)
{
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
    {
        fprintf(stderr, "libusb_get_device_list: %s\n", libusb_strerror(cnt));
        return NULL;
    }

    libusb_device *result = NULL;
    for (int i = 0; devs[i] != NULL; ++i)
    {
        libusb_device *dev = devs[i];
        if (is_co2_device(dev))
        {
            result = dev;
            libusb_ref_device(dev);
            break;
        }
    }

    libusb_free_device_list(devs, 1);

    return result;
}

void
co2mon_release_device(libusb_device *dev)
{
    libusb_unref_device(dev);
}

libusb_device_handle *
co2mon_open_device(libusb_device *dev)
{
    libusb_device_handle *handle;
    int r = libusb_open(dev, &handle);
    if (r != 0)
    {
        fprintf(stderr, "libusb_open: %s\n", libusb_strerror(r));
        return NULL;
    }

#ifdef __linux__
    libusb_detach_kernel_driver(handle, 0);
#endif

    r = libusb_claim_interface(handle, 0);
    if (r != 0)
    {
        fprintf(stderr, "libusb_claim_interface: %s\n", libusb_strerror(r));
        libusb_close(handle);
        return NULL;
    }

    return handle;
}

void
co2mon_close_device(libusb_device_handle *handle)
{
    libusb_close(handle);
}

int
co2mon_send_magic_table(libusb_device_handle *handle, co2mon_magic_table_t magic_table)
{
    int r = libusb_control_transfer(
        handle,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        LIBUSB_REQUEST_SET_CONFIGURATION,
        0x0300, 0,
        magic_table, sizeof(co2mon_magic_table_t),
        2000 /* milliseconds */);
    if (r < 0 || r != sizeof(co2mon_magic_table_t))
    {
        fprintf(stderr, "libusb_control_transfer(out, magic_table): %s\n", libusb_strerror(r));
        return 0;
    }
    return 1;
}

static void
swap_char(unsigned char *a, unsigned char *b)
{
    unsigned char tmp = *a;
    *a = *b;
    *b = tmp;
}

static void
decode_buf(co2mon_data_t result, co2mon_data_t buf, co2mon_magic_table_t magic_table)
{
    swap_char(&buf[0], &buf[2]);
    swap_char(&buf[1], &buf[4]);
    swap_char(&buf[3], &buf[7]);
    swap_char(&buf[5], &buf[6]);

    for (int i = 0; i < 8; ++i)
    {
        buf[i] ^= magic_table[i];
        // TODO: do we really need to xor it with magic_table? Its value is {0}
    }

    unsigned char tmp = (buf[7] << 5);
    result[7] = (buf[6] << 5) | (buf[7] >> 3);
    result[6] = (buf[5] << 5) | (buf[6] >> 3);
    result[5] = (buf[4] << 5) | (buf[5] >> 3);
    result[4] = (buf[3] << 5) | (buf[4] >> 3);
    result[3] = (buf[2] << 5) | (buf[3] >> 3);
    result[2] = (buf[1] << 5) | (buf[2] >> 3);
    result[1] = (buf[0] << 5) | (buf[1] >> 3);
    result[0] = tmp | (buf[0] >> 3);

    const unsigned char magic_word[8] = "Htemp99e";
    for (int i = 0; i < 8; ++i)
    {
        result[i] -= (magic_word[i] << 4) | (magic_word[i] >> 4);
    }
}

int
co2mon_read_data(libusb_device_handle *handle, co2mon_magic_table_t magic_table, co2mon_data_t result)
{
    int actual_length;
    co2mon_data_t data = {0};
    int r = libusb_interrupt_transfer(handle,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
        data, sizeof(co2mon_data_t), &actual_length,
        5000 /* milliseconds */);
    if (r < 0)
    {
        fprintf(stderr, "libusb_interrupt_transfer(in, data): %s\n", libusb_strerror(r));
        return r;
    }
    if (actual_length != sizeof(co2mon_data_t))
    {
        fprintf(stderr, "libusb_interrupt_transfer(in, data): trasferred %d bytes, expected %lu bytes\n", actual_length, (unsigned long)sizeof(co2mon_data_t));
        return 0;
    }

    decode_buf(result, data, magic_table);
    return actual_length;
}
