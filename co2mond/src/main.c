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

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "co2mon.h"

#define CODE_TAMB 0x42 /* Ambient Temperature */
#define CODE_CNTR 0x50 /* Relative Concentration of CO2 */

#define PATH_MAX 4096
#define VALUE_MAX 20

int daemonize = 0;
int print_unknown = 0;
char *datadir;

uint16_t co2mon_data[256];

static int
lock(int fd, short int type)
{
    struct flock lock;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_whence = SEEK_SET;
    lock.l_type = type;
    return fcntl(fd, F_SETLKW, &lock);
}

static double
decode_temperature(uint16_t w)
{
    return (double)w * 0.0625 - 273.15;
}

static int
write_data(int fd, const char *value)
{
    char data[VALUE_MAX + 1];
    snprintf(data, VALUE_MAX + 1, "%s\n", value);

    if (lock(fd, F_WRLCK) != 0)
    {
        perror("lock");
        return 0;
    }

    if (ftruncate(fd, 0) != 0)
    {
        perror("ftruncate");
        return 0;
    }

    ssize_t len = strnlen(data, VALUE_MAX + 1);
    if (write(fd, data, len) != len)
    {
        perror("write");
        return 0;
    }

    if (lock(fd, F_UNLCK) != 0)
    {
        perror("unlock");
        return 0;
    }

    return 1;
}

static int
write_value(const char *name, const char *value)
{
    if (!datadir)
    {
        return 1;
    }

    char filename[PATH_MAX];
    snprintf(filename, PATH_MAX, "%s/%s", datadir, name);

    int fd = open(filename, O_CREAT | O_WRONLY, 0666);
    if (fd == -1)
    {
        perror(filename);
        return 0;
    }

    int result = write_data(fd, value);
    close(fd);
    return result;
}

static void
write_heartbeat()
{
    char buf[VALUE_MAX];
    snprintf(buf, VALUE_MAX, "%lld", (long long)time(0));
    write_value("heartbeat", buf);
}

static void
device_loop(co2mon_device dev)
{
    co2mon_data_t magic_table = { 0 };
    co2mon_data_t result;

    if (!co2mon_send_magic_table(dev, magic_table))
    {
        fprintf(stderr, "Unable to send magic table to CO2 device\n");
        return;
    }

    while (1)
    {
        int r = co2mon_read_data(dev, magic_table, result);
        if (r <= 0)
        {
            fprintf(stderr, "Error while reading data from device\n");
            break;
        }

        if (result[4] != 0x0d)
        {
            fprintf(stderr, "Unexpected data from device (data[4] = %02hhx, want 0x0d)\n", result[4]);
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

        char buf[VALUE_MAX];
        uint16_t w = (result[1] << 8) + result[2];

        switch (r0)
        {
        case CODE_TAMB:
            snprintf(buf, VALUE_MAX, "%.4f", decode_temperature(w));

            if (!daemonize)
            {
                printf("Tamb\t%s\n", buf);
                fflush(stdout);
            }

            if (co2mon_data[r0] != w)
            {
                if (write_value("Tamb", buf))
                {
                    co2mon_data[r0] = w;
                }
            }

            write_heartbeat();

            break;
        case CODE_CNTR:
            snprintf(buf, VALUE_MAX, "%d", (int)w);

            if (!daemonize)
            {
                printf("CntR\t%s\n", buf);
                fflush(stdout);
            }

            if (co2mon_data[r0] != w)
            {
                if (write_value("CntR", buf))
                {
                    co2mon_data[r0] = w;
                }
            }

            write_heartbeat();

            break;
        default:
            if (print_unknown && !daemonize)
            {
                printf("0x%02hhx\t%d\n", r0, (int)w);
                fflush(stdout);
            }
            co2mon_data[r0] = w;
        }
    }
}

static void
main_loop()
{
    int error_shown = 0;
    while (1)
    {
        co2mon_device dev = co2mon_open_device();
        if (dev == NULL)
        {
            if (!error_shown)
            {
                fprintf(stderr, "Unable to open CO2 device\n");
                error_shown = 1;
            }
            sleep(1);
            continue;
        } else {
            error_shown = 0;
        }

        device_loop(dev);

        co2mon_close_device(dev);
    }
}

int main(int argc, char *argv[])
{
    char *reldatadir = 0;
    char *pidfile = 0;
    char *logfile = 0;

    char c;
    int opterr = 0;
    int show_help = 0;
    while ((c = getopt(argc, argv, ":dhuD:l:p:")) != -1)
    {
        switch (c)
        {
        case 'd':
            daemonize = 1;
            break;
        case 'h':
            show_help = 1;
            break;
        case 'u':
            print_unknown = 1;
            break;
        case 'D':
            reldatadir = optarg;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'p':
            pidfile = optarg;
            break;
        case ':':
            fprintf(stderr, "Option -%c requires an operand\n", optopt);
            opterr++;
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: -%c\n", optopt);
            opterr++;
        }
    }
    if (show_help || opterr || optind != argc)
    {
        fprintf(stderr, "usage: co2mond [-dhu] [-D datadir] [-p pidfle] [-l logfile]\n");
        if (show_help)
        {
            fprintf(stderr, "\n");
            fprintf(stderr, "  -d    run as a daemon\n");
            fprintf(stderr, "  -h    show this help message\n");
            fprintf(stderr, "  -u    print values for unknown items\n");
            fprintf(stderr, "  -D datadir\n");
            fprintf(stderr, "        store values from the sensor in datadir\n");
            fprintf(stderr, "  -p pidfile\n");
            fprintf(stderr, "        write PID to a file named pidfile\n");
            fprintf(stderr, "  -l logfile\n");
            fprintf(stderr, "        write diagnostic information to a file named logfile\n");
            fprintf(stderr, "\n");
        }
        exit(1);
    }
    if (daemonize && !reldatadir)
    {
        fprintf(stderr, "co2mond: it is useless to use -d without -D.\n");
        exit(1);
    }

    if (reldatadir)
    {
        datadir = realpath(reldatadir, NULL);
        if (datadir == NULL)
        {
            perror(reldatadir);
            exit(1);
        }
    }

    int pidfd = -1;
    if (pidfile)
    {
        pidfd = open(pidfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (pidfd == -1)
        {
            perror(pidfile);
            exit(1);
        }
    }

    int logfd = -1;
    if (logfile)
    {
        logfd = open(logfile, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (logfd == -1)
        {
            perror(logfile);
            exit(1);
        }
    }

    if (daemonize)
    {
        if (daemon(0, 0) == -1)
        {
            perror("daemon");
            exit(1);
        }
    }

    if (pidfd != -1)
    {
        char pid[VALUE_MAX];
        snprintf(pid, VALUE_MAX, "%lld", (long long)getpid());
        if (!write_data(pidfd, pid))
        {
            exit(1);
        }
    }

    if (logfd != -1)
    {
        dup2(logfd, fileno(stderr));
        close(logfd);
    }

    int r = co2mon_init();
    if (r < 0)
    {
        return r;
    }

    main_loop();

    co2mon_exit();

    if (datadir)
    {
        free(datadir);
    }
    return 1;
}
