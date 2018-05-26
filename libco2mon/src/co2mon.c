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
#include <string.h>

#include "co2mon.h"

int
co2mon_init()
{
    int r = hid_init();
    if (r < 0)
    {
        fprintf(stderr, "hid_init: error\n");
    }
    return r;
}

void
co2mon_exit()
{
    int r = hid_exit();
    if (r < 0)
    {
        fprintf(stderr, "hid_exit: error\n");
    }
}

hid_device *
co2mon_open_device()
{
    hid_device *dev = hid_open(0x04d9, 0xa052, NULL);
    if (!dev)
    {
        fprintf(stderr, "hid_open: error\n");
    }
    return dev;
}

hid_device *
co2mon_open_device_path(const char *path)
{
    hid_device *dev = hid_open_path(path);
    if (!dev)
    {
        fprintf(stderr, "hid_open_path: error\n");
    }
    return dev;
}

void
co2mon_close_device(hid_device *dev)
{
    hid_close(dev);
}

int
co2mon_device_path(hid_device *dev, char *str, size_t maxlen)
{
    str[0] = '\0';
    return 1;
}

int
co2mon_send_magic_table(hid_device *dev, co2mon_data_t magic_table)
{
    int r = hid_send_feature_report(dev, magic_table, sizeof(co2mon_data_t));
    if (r < 0 || r != sizeof(co2mon_data_t))
    {
        fprintf(stderr, "hid_send_feature_report: error\n");
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
decode_buf(co2mon_data_t result, co2mon_data_t buf, co2mon_data_t magic_table)
{
    swap_char(&buf[0], &buf[2]);
    swap_char(&buf[1], &buf[4]);
    swap_char(&buf[3], &buf[7]);
    swap_char(&buf[5], &buf[6]);

    for (int i = 0; i < 8; ++i)
    {
        buf[i] ^= magic_table[i];
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
co2mon_read_data(hid_device *dev, co2mon_data_t magic_table, co2mon_data_t result)
{
    co2mon_data_t data = { 0 };
    int actual_length = hid_read_timeout(dev, data, sizeof(co2mon_data_t), 5000 /* milliseconds */);
    if (actual_length < 0)
    {
        fprintf(stderr, "hid_read_timeout: error\n");
        return actual_length;
    }
    if (actual_length != sizeof(co2mon_data_t))
    {
        fprintf(stderr, "hid_read_timeout: transferred %d bytes, expected %lu bytes\n", actual_length, (unsigned long)sizeof(co2mon_data_t));
        return 0;
    }

    decode_buf(result, data, magic_table);
    return actual_length;
}
