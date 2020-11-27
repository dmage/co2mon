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

#define _XOPEN_SOURCE 700 /* strnlen */
#define _BSD_SOURCE /* daemon() in glibc before 2.19 */
#define _DEFAULT_SOURCE /* _BSD_SOURCE is deprecated in glibc 2.19+ */
#define _DARWIN_C_SOURCE /* daemon() on macOS */

#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>

#include "co2mon.h"

#define CODE_TAMB 0x42 /* Ambient Temperature */
#define CODE_CNTR 0x50 /* Relative Concentration of CO2 */

#define PATH_MAX 4096
#define VALUE_MAX 20

int daemonize = 0;
int print_unknown = 0;
int decode_data = 1;
const char *devicefile = NULL;
char *datadir;

struct co2mon_state {
    uint16_t data[256];
    uint8_t seen[32];
    time_t heatbeat;
    unsigned int deverr;
};

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
struct co2mon_state co2mon;

static int
bitarr_isset(uint8_t* bitarr, unsigned int ndx)
{
    return bitarr[ndx >> 3] & (1u << (ndx & 0x07u)) ? 1 : 0;
}

static void
bitarr_set(uint8_t* bitarr, unsigned int ndx)
{
    bitarr[ndx >> 3] |= 1u << (ndx & 0x07u);
}

static void
state_lock()
{
    if (pthread_mutex_lock(&state_mutex) != 0)
    {
        err(EXIT_FAILURE, "pthread_mutex_lock");
    }
}

static void
state_unlock()
{
    if (pthread_mutex_unlock(&state_mutex) != 0)
    {
        err(EXIT_FAILURE, "pthread_mutex_unlock");
    }
}

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
    const time_t now = time(0);
    snprintf(buf, VALUE_MAX, "%lld", (long long)now);
    write_value("heartbeat", buf);
    state_lock();
    co2mon.heatbeat = now;
    state_unlock();
}

static int
read_match_path(FILE* fd)
{
    // Prefix has no \r\n to support HTTP/1.0 requests with no headers.
    const char *prefix = "GET /metrics HTTP/1.";
    const int len = strlen(prefix);
    for (int i = 0; i < len; ++i)
    {
        int next = fgetc(fd);
        if (next == EOF || next != prefix[i]) // EOF or mismatch
        {
            return -1;
        }
    }
    return 0;
}

static int
read_find_crlfcrlf(FILE* fd)
{
    const char *substring = "\x0d\x0a\x0d\x0a";
    const int len = strlen(substring);
    // automata is tivial as `substring` has no repeated chars
    for (int i = 0; i < len; ++i)
    {
        assert(i >= 0);
        int next = fgetc(fd);
        if (next == EOF)
        {
            return -1;
        }
        if (next != substring[i])
        {
            if (next == substring[0])
            {
                i = 0;
            }
            else
            {
                i = -1;
            }
        }
    }
    return 0;
}

static void*
prometheus_thread(void *arg)
{
    const int listen_fd = (ssize_t)arg;
    while (1) {
        const int client_fd = accept(listen_fd, NULL, NULL);
        const struct timeval maxdelay = { 5, 0 }; // 5 seconds, just like co2mon_read_data()
        struct co2mon_state copy;
        FILE* out = NULL;

        if (client_fd == -1)
        {
            perror("accept");
            continue;
        }

        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &maxdelay, sizeof(maxdelay)) != 0 ||
            setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &maxdelay, sizeof(maxdelay)) != 0)
        {
            perror("setsockopt");
            goto cleanup;
        }

        out = fdopen(client_fd, "a+b");
        if (!out)
        {
            perror("fdopen");
            goto cleanup;
        }

        if (read_match_path(out) != 0 || read_find_crlfcrlf(out) != 0)
        {
            fprintf(out,
                "HTTP/1.0 400 Bad Request\r\n"
                "Server: co2mond\r\n"
                "Connection: close\r\n"
                "\r\n"
                "goto /metrics;\r\n"
            );
            goto flush;
        }

        state_lock();
        memcpy(&copy, &co2mon, sizeof(copy));
        state_unlock();

        if (!bitarr_isset(copy.seen, CODE_TAMB) || !bitarr_isset(copy.seen, CODE_CNTR))
        {
            fprintf(out,
                "HTTP/1.0 503 Service Unavailable\r\n"
                "Server: co2mond\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Device not ready.\r\n"
            );
            goto flush;
        }

        fprintf(out,
            "HTTP/1.0 200 OK\r\n"
            "Server: co2mond\r\n"
            "Connection: close\r\n"
            "\r\n"
        );

        // Note, HTTP has \r\n and Prometheus uses \n as line separator.

        if (print_unknown)
        {
            int has_unknown = 0;
            for (int i = 0; i < sizeof(copy.data) / sizeof(copy.data[0]); ++i)
            {
                if (bitarr_isset(copy.seen, i) && i != CODE_TAMB && i != CODE_CNTR)
                {
                    has_unknown = 1;
                    break;
                }
            }
            if (has_unknown)
            {
                fprintf(out,
                    "# HELP co2mon_unknown Unknown value.\n"
                    "# TYPE co2mon_unknown gauge\n"
                );
                for (int i = 0; i < sizeof(copy.data) / sizeof(copy.data[0]); ++i)
                {
                    if (bitarr_isset(copy.seen, i) && i != CODE_TAMB && i != CODE_CNTR)
                    {
                        fprintf(out, "co2mon_unknown{key=\"x%02x\"} %d\n", i, copy.data[i]);
                    }
                }
            }
        }
        fprintf(out,
            "# HELP co2mon_temp_celsius Ambient temperature.\n"
            "# TYPE co2mon_temp_celsius gauge\n"
            "co2mon_temp_celsius %.4f\n"
            "# HELP co2mon_co2_ppm Concentration of CO2, parts per million.\n"
            "# TYPE co2mon_co2_ppm gauge\n"
            "co2mon_co2_ppm %d\n"
            "# HELP co2mon_device_errors_total CO2 monitor device error counter.\n"
            "# TYPE co2mon_device_errors_total counter\n"
            "co2mon_device_errors_total %d\n"
            "# HELP co2mon_heartbeat_time_seconds CO2 monitor heartbeat timestamp.\n"
            "# TYPE co2mon_heartbeat_time_seconds gauge\n"
            "co2mon_heartbeat_time_seconds %lld\n",
            decode_temperature(copy.data[CODE_TAMB]),
            copy.data[CODE_CNTR],
            copy.deverr,
            (long long)copy.heatbeat
        );
flush:
        fflush(out);
        if (shutdown(client_fd, SHUT_WR) != 0)
        {
            perror("shutdown");
            goto cleanup;
        }
        fgetc(out); // wait till EOF (or timeout) before calling close()
cleanup:
        if (out)
        {
            fclose(out);
        }
        else
        {
            close(client_fd);
        }

    }
}

static void
device_loop(co2mon_device dev)
{
    co2mon_data_t magic_table = { 0 };
    co2mon_data_t result;
    uint16_t written_tamb = 0;
    uint16_t written_cntr = 0;

    if (!co2mon_send_magic_table(dev, magic_table))
    {
        state_lock();
        co2mon.deverr++;
        state_unlock();
        fprintf(stderr, "Unable to send magic table to CO2 device\n");
        return;
    }

    state_lock();
    memset(co2mon.seen, 0, sizeof(co2mon.seen));
    state_unlock();

    while (1)
    {
        int r = co2mon_read_data(dev, magic_table, result);
        if (r <= 0)
        {
            state_lock();
            co2mon.deverr++;
            state_unlock();
            fprintf(stderr, "Error while reading data from device\n");
            break;
        }

        if (result[4] != 0x0d)
        {
            state_lock();
            co2mon.deverr++;
            state_unlock();
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
            state_lock();
            co2mon.deverr++;
            state_unlock();
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

            if (written_tamb != w)
            {
                if (write_value("Tamb", buf))
                {
                    written_tamb = w;
                }
            }

            write_heartbeat();

            break;
        case CODE_CNTR:
            if ((unsigned)w > 3000) {
                // Avoid reading spurious (uninitialized?) data
                break;
            }
            snprintf(buf, VALUE_MAX, "%d", (int)w);

            if (!daemonize)
            {
                printf("CntR\t%s\n", buf);
                fflush(stdout);
            }

            if (written_cntr != w)
            {
                if (write_value("CntR", buf))
                {
                    written_cntr = w;
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
        }

        state_lock();
        co2mon.data[r0] = w;
        bitarr_set(co2mon.seen, r0);
        state_unlock();
    }
}

static co2mon_device
open_device()
{
    if (devicefile)
    {
        return co2mon_open_device_path(devicefile);
    }
    return co2mon_open_device();
}

static void
main_loop()
{
    int error_shown = 0;
    while (1)
    {
        co2mon_device dev = open_device();
        if (dev == NULL)
        {
            if (!error_shown)
            {
                fprintf(stderr, "Unable to open CO2 device\n");
                error_shown = 1;
            }
            sleep(1);
            continue;
        }
        else
        {
            error_shown = 0;
        }

        device_loop(dev);

        co2mon_close_device(dev);
    }
}

int main(int argc, char *argv[])
{
    char *reldatadir = 0;
    char *promaddr = 0;
    char *pidfile = 0;
    char *logfile = 0;

    int c;
    int opterr = 0;
    int show_help = 0;
    while ((c = getopt(argc, argv, ":dnhuD:P:f:l:p:")) != -1)
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
        case 'n':
            decode_data = 0;
            break;
        case 'D':
            reldatadir = optarg;
            break;
        case 'P':
            promaddr = optarg;
            break;
        case 'f':
            devicefile = optarg;
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
        fprintf(stderr, "usage: co2mond [-dhun] [-D datadir] [-f device] [-p pidfle] [-l logfile]\n");
        if (show_help)
        {
            fprintf(stderr, "\n");
            fprintf(stderr, "  -d    run as a daemon\n");
            fprintf(stderr, "  -h    show this help message\n");
            fprintf(stderr, "  -u    print values for unknown items\n");
            fprintf(stderr, "  -n    don't decode the data\n");
            fprintf(stderr, "  -D datadir\n");
            fprintf(stderr, "        store values from the sensor in datadir\n");
            fprintf(stderr, "  -P host:port\n");
            fprintf(stderr, "        address on which to expose metrics\n");
            fprintf(stderr, "  -f devicefile\n");
#ifdef __linux__
            fprintf(stderr, "        path to a device (e.g., /dev/hidraw0)\n");
#else
            fprintf(stderr, "        path to a device\n");
#endif
            fprintf(stderr, "  -p pidfile\n");
            fprintf(stderr, "        write PID to a file named pidfile\n");
            fprintf(stderr, "  -l logfile\n");
            fprintf(stderr, "        write diagnostic information to a file named logfile\n");
            fprintf(stderr, "\n");
        }
        exit(1);
    }
    if (daemonize && !reldatadir && !promaddr)
    {
        fprintf(stderr, "co2mond: it is useless to use -d without -D or -P.\n");
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

    int listen_fd = -1;
    if (promaddr)
    {
        char *copy = strdup(promaddr);
        char *colon = strrchr(copy, ':');
        char *port = colon ? colon + 1 : copy;
        if (colon)
        {
            *colon = '\0';
        }
        char *host =
            (!colon || copy[0] == '\0') ? NULL :
            (copy[0] == '[') ? copy + 1 :
            copy;
        if (host)
        {
            size_t hlen = strlen(host);
            if (copy[0] == '[' && hlen > 0 && host[hlen - 1] == ']')
            {
                host[hlen - 1] = '\0';
            }
        }

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE | AI_NUMERICSERV;

        int gai_errno = getaddrinfo(host, port, &hints, &res);
        if (gai_errno != 0)
        {
            errx(EXIT_FAILURE, "getaddrinfo(%s): %s", promaddr, gai_strerror(gai_errno));
        }

        listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (listen_fd == -1)
        {
            err(EXIT_FAILURE, "socket");
        }

        int on = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
        {
            err(EXIT_FAILURE, "setsockopt(SO_REUSEADDR)");
        }

        if (bind(listen_fd, res->ai_addr, res->ai_addrlen) != 0)
        {
            err(EXIT_FAILURE, "bind");
        }

        if (listen(listen_fd, 128) != 0)
        {
            err(EXIT_FAILURE, "listen");
        }

        if (signal(SIGPIPE, SIG_IGN) != 0)
        {
            err(EXIT_FAILURE, "signal(SIGPIPE, SIG_IGN)");
        }

        free(copy);
        freeaddrinfo(res);
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

    if (listen_fd != -1)
    {
        pthread_t tid;
        if (pthread_create(&tid, NULL, prometheus_thread, (void*)((size_t)listen_fd)) != 0)
        {
            err(EXIT_FAILURE, "pthread_create");
        }

        if (pthread_detach(tid) != 0)
        {
            err(EXIT_FAILURE, "pthread_detach");
        }
    }

    if (logfd != -1)
    {
        dup2(logfd, fileno(stderr));
        close(logfd);
    }

    int r = co2mon_init(decode_data);
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
