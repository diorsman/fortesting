/**
 *  File: main.c
 *
 *  Copyright (C) 2008 Du XiaoGang <dugang@188.com>
 *
 *  This file is part of statusd.
 *  
 *  statusd is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as 
 *  published by the Free Software Foundation, either version 3 of the 
 *  License, or (at your option) any later version.
 *  
 *  statusd is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with statusd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <webmon.h>

#define PORT_DEF            80
#define PICTURE_POINT_DEF   900
#define PICTURE_POINT_MAX   10000
#define PICTURE_POINT_MIN   100
#define SAMPLE_INTERVAL_DEF 4
#define SAMPLE_INTERVAL_MAX 600
#define SAMPLE_INTERVAL_MIN 1

static time_t begin_time;

static const char **
textinfo_cb(void *arg)
{
    /**
     * webmon_run() and main() are running in the same thread,
     * so we can use static(not thread-safe) variables in textinfo_cb
     * and don't need free-cb.
     */
    static char *rets[2], str1[1024];

    snprintf(str1, sizeof(str1), 
             "statusd has been running for %u seconds.",
             (int)(time(NULL) - begin_time));
    rets[0] = str1;
    rets[1] = NULL;
    return (const char **)rets;
}

static void
show_help(void)
{
    fprintf(stderr, 
            "usage: statusd [-p(%d): listen port]\n" 
            "               [-n(%d): picture displayed points]\n" 
            "               [-i(%d): sample interval]\n", 
            PORT_DEF, PICTURE_POINT_DEF, SAMPLE_INTERVAL_DEF);
}

int
main(int argc, char *argv[])
{
    int ret, 
        listen_port = PORT_DEF, 
        picture_point = PICTURE_POINT_DEF, 
        sample_interval = SAMPLE_INTERVAL_DEF;
    void *sd;

    while (1) {
        ret = getopt(argc, argv, "p:n:i:");
        if (ret == -1) {
            if (argv[optind] != NULL) {
                show_help();
                exit(1);
            }
            break;
        }
        switch (ret) {
        case 'p':
            listen_port = atoi(optarg);
            if (listen_port < 1 || listen_port > 65536) {
                fprintf(stderr, "The listen port should be in [1, 65536].");
                exit(1);
            }
            break;
        case 'n':
            picture_point = atoi(optarg);
            if (picture_point < PICTURE_POINT_MIN 
                || picture_point > PICTURE_POINT_MAX) 
            {
                fprintf(stderr, 
                        "The picture displayed points should be in [%d, %d].", 
                        PICTURE_POINT_MIN, PICTURE_POINT_MAX);
                exit(1);
            }
            break;
        case 'i':
            sample_interval = atoi(optarg);
            if (sample_interval < SAMPLE_INTERVAL_MIN 
                || sample_interval > SAMPLE_INTERVAL_MAX) 
            {
                fprintf(stderr, 
                        "The sample interval should be in [%d, %d] seconds.", 
                        SAMPLE_INTERVAL_MIN, SAMPLE_INTERVAL_MAX);
                exit(1);
            }
            break;
        default:
            show_help();
            exit(1);
        }
    }

    begin_time = time(NULL);

    /* daemon */
    openlog("statusd", LOG_NDELAY | LOG_PID, LOG_DAEMON);
    daemon(0, 0);

    /* signal */
    signal(SIGPIPE, SIG_IGN);

    /* init webmon */
    if (webmon_init() == -1) {
        syslog(LOG_CRIT, "webmon_init() failed, errno = %d.\n", errno);
        exit(1);
    }
    /* create an instance */
    sd = webmon_create("System Resource Web Monitor", "0.0.0.0", 
                       listen_port, picture_point, sample_interval, 
                       1, 1, 1);
    if (sd == NULL) {
        syslog(LOG_CRIT, "webmon_create() failed, errno = %d.\n", errno);
        exit(1);
    }
    /* set textinfo callback */
    webmon_set_textinfo_callback(sd, textinfo_cb, NULL);
    /* run */
    syslog(LOG_INFO, "statusd is running on 0.0.0.0:%d now.\n", 
           listen_port);
    webmon_run(sd);

    return 0;
}
