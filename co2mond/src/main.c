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

#include "co2mon.h"

static const gchar service[] = "io.github.dmage.CO2Mon";
static const gchar object_name[] = "/io/github/dmage/CO2Mon";

static GDBusNodeInfo *introspection_data = NULL;
static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='io.github.dmage.CO2Mon'>"
    "    <method name='GetTemperature'>"
    "      <arg type='d' name='response' direction='out'/>"
    "    </method>"
    "    <method name='GetCO2'>"
    "      <arg type='q' name='response' direction='out'/>"
    "    </method>"
    "    <signal name='NewValue'>"
    "      <arg type='y' name='code'/>"
    "      <arg type='q' name='raw_value'/>"
    "      <arg type='s' name='name'/>"
    "      <arg type='v' name='value'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

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
get_temperature(void)
{
    return decode_temperature(get_raw_data(CODE_TEMP));
}

static uint16_t
get_co2(void)
{
    return get_raw_data(CODE_CO2);
}

static void
device_loop(libusb_device *dev)
{
    libusb_device_handle *handle;
    co2mon_magic_table_t magic_table = {0};
    co2mon_data_t result;

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

    printf("Sending values to D-Bus...\n");

    while (1)
    {
        int r = co2mon_read_data(handle, magic_table, result);
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

static gpointer
monitor_loop(G_GNUC_UNUSED gpointer data)
{
    gboolean show_no_device = TRUE;
    while (1)
    {
        libusb_device *dev = co2mon_find_device();
        if (dev == NULL)
        {
            if (show_no_device) {
                fprintf(stderr, "No CO2 device found\n");
                show_no_device = FALSE;
            }
            sleep(1);
            continue;
        }

        show_no_device = TRUE;

        printf("Bus %03d Device %03d\n",
            libusb_get_bus_number(dev), libusb_get_device_address(dev));

        device_loop(dev);

        co2mon_release_device(dev);
        sleep(1);
    }
    return NULL;
}

static GVariant *
handle_get_property(
    G_GNUC_UNUSED GDBusConnection  *connection,
    G_GNUC_UNUSED const gchar      *sender,
    G_GNUC_UNUSED const gchar      *object_path,
    G_GNUC_UNUSED const gchar      *interface_name,
                  const gchar      *property_name,
                  GError          **error,
    G_GNUC_UNUSED gpointer          user_data)
{
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
        "Invalid property '%s'", property_name);
    return NULL;
}

static void
handle_method_call(
    G_GNUC_UNUSED GDBusConnection       *connection,
    G_GNUC_UNUSED const gchar           *sender,
    G_GNUC_UNUSED const gchar           *object_path,
    G_GNUC_UNUSED const gchar           *interface_name,
                  const gchar           *method_name,
    G_GNUC_UNUSED GVariant              *parameters,
                  GDBusMethodInvocation *invocation,
    G_GNUC_UNUSED gpointer               user_data)
{
    if (g_strcmp0(method_name, "GetTemperature") == 0)
    {
        GVariant *result = g_variant_new("(d)", get_temperature());
        g_dbus_method_invocation_return_value(invocation, result);
    }
    else if (g_strcmp0(method_name, "GetCO2") == 0)
    {
        GVariant *result = g_variant_new("(q)", get_co2());
        g_dbus_method_invocation_return_value(invocation, result);
    }
    else
    {
        g_dbus_method_invocation_return_error(invocation,
            G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Invalid method: '%s'", method_name);
    }
}

static gboolean
handle_set_property(
    G_GNUC_UNUSED GDBusConnection  *connection,
    G_GNUC_UNUSED const gchar      *sender,
    G_GNUC_UNUSED const gchar      *object_path,
    G_GNUC_UNUSED const gchar      *interface_name,
                  const gchar      *property_name,
    G_GNUC_UNUSED GVariant         *value,
                  GError          **error,
    G_GNUC_UNUSED gpointer          user_data)
{
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
        "Invalid property: '%s'", property_name);
    return FALSE;
}

static void
on_bus_acquired(void)
{
    static GDBusInterfaceVTable interface_vtable = {
        handle_method_call,
        handle_get_property,
        handle_set_property,
        {0}
    };

    GError *error = NULL;
    guint registration_id = g_dbus_connection_register_object(
        connection,
        object_name,
        introspection_data->interfaces[0],
        &interface_vtable,
        NULL,
        NULL,
        &error
    );

    GThread *monitor_loop_thread = g_thread_new("monitor_loop", monitor_loop, NULL);
}

static void
on_name_acquired(
    G_GNUC_UNUSED GDBusConnection *connection,
    G_GNUC_UNUSED const gchar     *name,
    G_GNUC_UNUSED gpointer         user_data)
{
}

static void
on_name_lost(
    G_GNUC_UNUSED GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    fprintf(stderr, "Unable to acquire bus name '%s'. Check your /etc/dbus-1/system.d/\n", name);
    g_main_loop_quit(user_data);
}

int main(void)
{
    int r;

    GError *error = NULL;

    r = libusb_init(NULL);
    if (r < 0)
    {
        fprintf(stderr, "libusb_init: %s\n", libusb_strerror(r));
        return r;
    }

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_assert(introspection_data != NULL);

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL)
    {
        g_assert(error != NULL);
        fprintf(stderr, "Unable to connect to D-Bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    on_bus_acquired();

    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

    guint owner_id = g_bus_own_name_on_connection(
        connection,
        service,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        on_name_acquired,
        on_name_lost,
        main_loop, /* user_data */
        NULL
    );

    g_main_loop_run(main_loop);

    g_bus_unown_name(owner_id);
    g_main_loop_unref(main_loop);
    g_object_unref(connection);

    libusb_exit(NULL);
    return 1;
}
