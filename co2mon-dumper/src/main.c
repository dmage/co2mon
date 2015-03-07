/*
 * co2mon - programming interface to CO2 sensor.
 * Copyright (C) 2015  Oleg Bulatov <oleg@bulatov.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "co2mon.h"

#define CODE_TEMP 0x42
#define CODE_CO2 0x50

static double
decode_temperature(uint16_t w)
{
    return (double)w * 0.0625 - 273.15;
}

static void
device_loop(co2mon_device dev)
{
    co2mon_magic_table_t magic_table = {0};
    co2mon_data_t result;

    if (!co2mon_send_magic_table(dev, magic_table))
    {
        fprintf(stderr, "Unable to send magic table to CO2 device\n");
        return;
    }

    printf("Sending values to D-Bus...\n");

    while (1)
    {
        co2mon_data_t result;

        int r = co2mon_read_data(dev, magic_table, result);
        if (r <= 0)
        {
            fprintf(stderr, "Error while reading data from device\n");
            break;
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

        switch (r0)
        {
        case CODE_TEMP:
            printf("TEMP %.2f\n", decode_temperature(w));
            break;
        case CODE_CO2:
            printf("CO2 %d\n", (int)w);
            break;
        default:
            printf("%02x %d\n", r0, (int)w);
        }
    }
}

static void
monitor_loop()
{
    int show_no_device = 1;
    while (1)
    {
        co2mon_device dev = co2mon_open_device();
        if (dev == NULL)
        {
            if (show_no_device) {
                fprintf(stderr, "Unable to open CO2 device\n");
                show_no_device = 0;
            }
        }
        else
        {
            show_no_device = 1;

            char path[16];
            if (co2mon_device_path(dev, path, 16))
            {
                printf("Path: %s\n", path);
            }
            else
            {
                printf("Path: (error)\n");
            }

            device_loop(dev);

            co2mon_close_device(dev);
        }
        sleep(1);
    }
}

int main(void)
{
    int r = co2mon_init();
    if (r < 0)
    {
        return r;
    }

    monitor_loop();

    co2mon_exit();
    return 1;
}
