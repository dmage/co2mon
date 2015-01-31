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
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>

#include "device.h"

#define CODE_TEMP 0x42
#define CODE_CO2 0x50

GDBusConnection *connection;

uint16_t co2mon_data[256];
GRWLock co2mon_data_lock;

static double
decode_temperature(uint16_t w)
{
    return (double)w * 0.0625 - 273.15;
}

static uint16_t
get_raw_data(unsigned char code)
{
    uint16_t w;
    g_rw_lock_reader_lock(&co2mon_data_lock);
    w = co2mon_data[code];
    g_rw_lock_reader_unlock(&co2mon_data_lock);
    return w;
}

static double
get_temperature()
{
    return decode_temperature(get_raw_data(CODE_TEMP));
}

static uint16_t
get_co2()
{
    return get_raw_data(CODE_CO2);
}

static void
device_loop(libusb_device *dev)
{
    libusb_device_handle *handle;
    int r;
    unsigned char magic_table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char result[8];

    handle = co2mon_open_device(dev);
    if (handle == NULL)
    {
        fprintf(stderr, "Unable to contact with CO2 device\n");
        return;
    }

    if (!co2mon_send_magic_table(handle, magic_table))
    {
        fprintf(stderr, "Unable to send magic table to CO2 device\n");
        libusb_close(handle);
        return;
    }

    while (1)
    {
        r = co2mon_read_data(handle, magic_table, result);
        if (r == LIBUSB_ERROR_NO_DEVICE)
        {
            fprintf(stderr, "Device has been disconnected\n");
            break;
        }
        else if (r <= 0)
        {
            continue;
        }

        if (result[4] != 0x0d)
        {
            fprintf(stderr, "Unexpected data from device (data[4] = %02hhx, await 0x0d)\n", result[4]);
            continue;
        }

        unsigned char r0, r1, r2, r3, checksum;
        r0 = result[0];
        r1 = result[1];
        r2 = result[2];
        r3 = result[3];
        checksum = r0 + r1 + r2;
        if (checksum != r3)
        {
            fprintf(stderr, "checksum error (%02hhx, await %02hhx)\n", checksum, r3);
            continue;
        }

        uint16_t w = (result[1] << 8) + result[2];
        g_rw_lock_writer_lock(&co2mon_data_lock);
        co2mon_data[r0] = w;
        g_rw_lock_writer_unlock(&co2mon_data_lock);

        const char *name = "UNKNOWN";
        GVariant *value = NULL;
        switch (r0)
        {
        case CODE_TEMP:
            name = "TEMP";
            value = g_variant_new_double(decode_temperature(w));
            break;
        case CODE_CO2:
            name = "CO2";
            value = g_variant_new_uint16(w);
            break;
        default:
            value = g_variant_new_uint16(w);
        }

        GVariant *params = g_variant_new("(yqsv)", r0, w, name, value);

        GError *error = NULL;
        gboolean ret = g_dbus_connection_emit_signal(
            connection,
            NULL, /* destination bus name */
            "/io/github/dmage/CO2Mon",
            "io.github.dmage.CO2Mon",
            "NewValue",
            params,
            &error);
        if (ret != TRUE)
        {
            fprintf(stderr, "Unable to emit D-Bus signal: %s\n", error->message);
            g_error_free(error);
        }
    }

    co2mon_close_device(handle);
}

int main()
{
    int r;

    GError *error = NULL;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL)
    {
        g_assert(error != NULL);
        fprintf(stderr, "Unable to connect to D-Bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    r = libusb_init(NULL);
    if (r < 0)
    {
        fprintf(stderr, "libusb_init: error %d\n", r);
        return r;
    }

    while (1)
    {
        libusb_device *dev = co2mon_find_device();
        if (dev == NULL)
        {
            fprintf(stderr, "No CO2 device found\n");
            sleep(1);
            continue;
        }

        printf("Bus %03d Device %03d\n",
            libusb_get_bus_number(dev), libusb_get_device_address(dev));
        device_loop(dev);
        co2mon_release_device(dev);
        sleep(1);
    }

    libusb_exit(NULL);
    return 0;
}
