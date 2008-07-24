/**
 *  File: client.c
 *
 *  Copyright (C) 2008 Du XiaoGang <dugang@188.com>
 *
 *  This file is part of udptest.
 *  
 *  udptest is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as 
 *  published by the Free Software Foundation, either version 3 of the 
 *  License, or (at your option) any later version.
 *  
 *  udptest is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with udptest.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <webmon.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

#define SERV1_ADDR          "192.168.8.56"
#define PARALLEL_CLIENT     100
#define FRAME_LENGTH        64
#define MAX_LIFE_TIME       1000 /* ms */
/* for ethernet */
#define UDP_PACKET_LEN_MIN  18
#define UDP_PACKET_LEN_MAX  1472

struct dlist_t {
    struct dlist_t *prev;
    struct dlist_t *next;
};

enum conn_status_e {
    UNINITED, 
    FREE,
    SENDING,
    SENT
};

struct conn_item_t {
    /* wait queue */
    struct dlist_t list;
    /* data */
    enum conn_status_e status;
    unsigned long seqno;
    struct sockaddr_in daddr;
    unsigned long long send_time; /* ms */
};

static char *serv_addr_def[] = {SERV1_ADDR, };
static char **serv_addr = serv_addr_def;
static int serv_addr_count = 1;
static int serv_port_begin = SERV_PORT_BEGIN;
static int parallel_client = PARALLEL_CLIENT;
static int packet_length = FRAME_LENGTH - 46;
static int max_life_time = MAX_LIFE_TIME;

static struct conn_item_t conn_arr[SERV_PORT_RANGE];
static struct dlist_t wait_queue;
static unsigned long cur_seqno = 0;
static int sock_begin;

/* for stat */
static unsigned long resp_time = 0;
static unsigned long resp_count = 0;
static unsigned long data_error = 0;
static unsigned long lost_count = 0;
static unsigned long reget_count = 0;
static int wait_queue_len = 0;

static char **
parse_serv_addr(const char *arg, int *num)
{
    const char *p, *q;
    int n, len;
    char **rets, ip[20];

    /* how many address will be used? */
    p = arg;
    n = 0;
    while (1) {
        n++;
        q = strchr(p, ',');
        /* for next */
        if (q == NULL)
            break;
        p = q + 1;
    }
    *num = n;

    rets = calloc(n + 1, sizeof(char *));
    if (rets == NULL) 
        return NULL;
    /* for each ip */
    p = arg;
    n = 0;
    while (1) {
        /* get */
        q = strchr(p, ',');
        if (q != NULL) {
            len = q - p;
        } else {
            len = strlen(p);
        }
        /* do simple check for ip */
        if (len < 7/* 1.1.1.1 */|| len >= sizeof(ip))
            goto err;
        strncpy(ip, p, len);
        ip[len] = '\0';
        rets[n] = strdup(ip);
        if (rets[n] == NULL)
            goto err;
        n++;
        /* for next */
        if (q == NULL)
            break;
        p = q + 1;
    }

    return rets;
err:
    n = 0;
    while (rets[n] != NULL) {
        free(rets[n]);
        n++;
    }
    free(rets);
    return NULL;
}

static void 
sighandler(int signo)
{
    int bakerrno, len;
    char strbuf[1024];

    if (signo == SIGQUIT)
        exit(0);

    bakerrno = errno;
    len = snprintf(strbuf, sizeof(strbuf), 
                   "resp_count: %lu, avg_time: %lums, wait_queue: %d\n"
                   "data_error: %lu, lost_count: %lu, reget_count: %lu\n", 
                   resp_count, 
                   (resp_count == 0) ? 0 : resp_time / resp_count, 
                   wait_queue_len, data_error, lost_count, reget_count);
    write(1, strbuf, len);
    errno = bakerrno;
}

/**
 *  0, go no
 *  1, complete
 */
static int
send_udp_packet(int sock_fd, int bidir)
{
    static char packet_data[PACKET_LENGTH_MAX] = {0};
    struct udpdata_t *ud;
    int idx, ret;
    struct timeval tv;

    /* init send buffer */
    ud = (struct udpdata_t *)packet_data;
    if (ud->data[0] == 0)
        memset(packet_data, 0xaa, sizeof(packet_data));

    /* get conn index */
    idx = sock_fd - sock_begin;
    if (conn_arr[idx].status != UNINITED
        && conn_arr[idx].status != FREE 
        && conn_arr[idx].status != SENDING) 
    {
        ERR;
        exit(1);
    }

    /* set seqno */
    if (conn_arr[idx].status == UNINITED) {
        /* new packet */
        conn_arr[idx].status = SENDING;
        conn_arr[idx].seqno = cur_seqno++;
        /* set dest addr info */
        memset(&conn_arr[idx].daddr, 0, sizeof(conn_arr[idx].daddr));
        conn_arr[idx].daddr.sin_family = AF_INET;
        conn_arr[idx].daddr.sin_addr.s_addr = 
            inet_addr(serv_addr[idx % serv_addr_count]);
        conn_arr[idx].daddr.sin_port = htons(serv_port_begin + idx);
    } else if (conn_arr[idx].status == FREE) {
        /* new packet */
        conn_arr[idx].status = SENDING;
        conn_arr[idx].seqno = cur_seqno++;
    }

    /* packets */
    if (bidir)
        ud->type = BIDIR_REQ;
    else
        ud->type = REQ;
    ud->seqno = conn_arr[idx].seqno;

    /* send */
    ret = sendto(sock_fd, packet_data, packet_length, 0, 
                 (struct sockaddr *)&conn_arr[idx].daddr, 
                 sizeof(conn_arr[idx].daddr));
    if (ret == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            /* error */
            ERR;
            exit(1);
        }
    } else if (ret == packet_length) {
        /* sent all */
        conn_arr[idx].status = SENT;
        gettimeofday(&tv, NULL);
        conn_arr[idx].send_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        /* append to wait_queue */
        wait_queue.prev->next = (struct dlist_t *)&conn_arr[idx];
        conn_arr[idx].list.prev = wait_queue.prev;
        wait_queue.prev = (struct dlist_t *)&conn_arr[idx];
        conn_arr[idx].list.next = &wait_queue;
        wait_queue_len++;
        return 1;
    }

    /* never to here */
    ERR;
    exit(1);
}

/**
 *  0, go no
 *  1, complete
 */
static int
recv_udp_packet(int sock_fd, int bidir)
{
    int idx, ret, addr_len, i, expect_len;
    struct udpdata_t *ud;
    char rbuf[PACKET_LENGTH_MAX + 1];
    struct sockaddr_in faddr;
    struct timeval tv;

    /* get conn index */
    idx = sock_fd - sock_begin;
    if (conn_arr[idx].status != SENT) {
        ERR;
        exit(1);
    }

    if (bidir)
        expect_len = packet_length;
    else
        expect_len = sizeof(struct udpdata_t);

    /* recv */
    while (1) {
        addr_len = sizeof(faddr);
        ret = recvfrom(sock_fd, rbuf, sizeof(rbuf), 0, 
                       (struct sockaddr *)&faddr, 
                       (socklen_t *)&addr_len);
        if (ret == -1) {
            if (errno == EAGAIN) {
                return 0;
            } else {
                /* error */
                ERR;
                exit(1);
            }
        }

        /* check length */
        if (ret != expect_len) {
            /* ignore */
            continue;
        }
        /* check from */
        if (addr_len != sizeof(faddr) 
            || faddr.sin_addr.s_addr != conn_arr[idx].daddr.sin_addr.s_addr
            || faddr.sin_port != conn_arr[idx].daddr.sin_port)
        {
            /* ignore */
            continue;
        }

        /* check received data */
        ud = (struct udpdata_t *)rbuf;
        /* check seqno */
        if (conn_arr[idx].seqno != ud->seqno) {
            /* reget */
            reget_count++;
            continue;
        }
        if (bidir) {
            /* check data */
            for (i = sizeof(struct udpdata_t); i < expect_len; i++) {
                if (rbuf[i] != (char)0xaa) {
                    /* data error */
                    data_error++;
                    /* end */
                    goto data_error;
                }
            }
        }

        /* recv all */
        gettimeofday(&tv, NULL);
        resp_time += tv.tv_sec * 1000 + tv.tv_usec / 1000 - conn_arr[idx].send_time;
        resp_count++;
data_error:
        conn_arr[idx].status = FREE;
        /* detach from wait_queue */
        conn_arr[idx].list.prev->next = conn_arr[idx].list.next;
        conn_arr[idx].list.next->prev = conn_arr[idx].list.prev;
        conn_arr[idx].list.prev = NULL;
        conn_arr[idx].list.next = NULL;
        wait_queue_len--;
        return 1;
    }

    /* never to here */
    ERR;
    exit(1);
}

static void
show_help(void)
{
    printf("usage: client [-h SERV_ADDR(%s)]\n"
           "              [-n PARALLEL_CLIENT(%d)]\n"
           "              [-l FRAME_LENGTH(%d)]\n"
           "              [-f MAX_LIFE_TIME(%d ms)]\n" 
           "              [-b BIDIRECTIONAL(disabled)]\n", 
           SERV1_ADDR, PARALLEL_CLIENT, FRAME_LENGTH, MAX_LIFE_TIME);
}

/**
 *  for webmon 
 */
static time_t begin_time;

static const char **
textinfo_cb(void *arg)
{
    char **rets, strbuf[1024];

    rets = calloc(3, sizeof(char *));
    if (rets == NULL)
        return NULL;

    /* running time */
    snprintf(strbuf, sizeof(strbuf), 
             "I have been running for %u seconds.", 
             (int)(time(NULL) - begin_time));
    *rets = strdup(strbuf);
    if (*rets == NULL)
        goto err;
    /* stats */
    snprintf(strbuf, sizeof(strbuf), 
             "Packets Statistics: sent/received(%lu), lost(%lu).", 
             resp_count, lost_count);
    *(rets + 1) = strdup(strbuf);
    if (*(rets + 1) == NULL)
        goto err;
    /* end */
    *(rets + 2) = NULL;

    return (const char **)rets;

err:
    if (*rets != NULL)
        free(*rets);
    if (*(rets + 1) != NULL)
        free(*(rets + 1));
    free(rets);
    return NULL;
}

static void
free_textinfo_cb(void *arg)
{
    char **pp = arg;

    while (*pp != NULL) {
        free(*pp);
        pp++;
    }
    free(arg);
}

static int
completed_sample(void *arg, double *ret)
{
    int interval/* interval in ms */;
    struct timeval tv;

    static unsigned long last_count = -1;
    static struct timeval last_sample_time;

    /* compute */
    if (last_count == -1) {
        *ret = 0.0;
        /* for later sample */
        last_count = resp_count;
        gettimeofday(&last_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        *ret = ((double)(resp_count - last_count) * 1000) / interval;
        /* for later sample */
        last_count = resp_count;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}

static int
avg_time_sample(void *arg, double *ret)
{
    static unsigned long last_count = -1, last_time;

    /* compute */
    if (last_count == -1) {
        *ret = 0.0;
        /* for later sample */
        last_time = resp_time;
        last_count = resp_count;
    } else {
        /* compute */
        *ret = (resp_count - last_count == 0) ? 
               0.0 : (double)(resp_time - last_time) / (resp_count - last_count);
        /* for later sample */
        last_time = resp_time;
        last_count = resp_count;
    }
    /* ok */
    return 0;
}

#if 0
static int
wait_queue_length_sample(void *arg, double *ret)
{
    *ret = (double)wait_queue_len;
    return 0;
}

static int
data_error_sample(void *arg, double *ret)
{
    int interval/* interval in ms */;
    struct timeval tv;

    static unsigned long last_count = -1;
    static struct timeval last_sample_time;

    /* compute */
    if (last_count == -1) {
        *ret = 0.0;
        /* for later sample */
        last_count = data_error;
        gettimeofday(&last_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        *ret = ((double)(data_error - last_count) * 1000) / interval;
        /* for later sample */
        last_count = data_error;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}
#endif

static int
lost_sample(void *arg, double *ret)
{
    int interval/* interval in ms */;
    struct timeval tv;

    static unsigned long last_count = -1;
    static struct timeval last_sample_time;

    /* compute */
    if (last_count == -1) {
        *ret = 0.0;
        /* for later sample */
        last_count = lost_count;
        gettimeofday(&last_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        *ret = ((double)(lost_count - last_count) * 1000) / interval;
        /* for later sample */
        last_count = lost_count;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}

struct webmon_graph_t my_graphs[] = {
    /* for packets transfer */
    {"收发包速率", 1, 100000, 0,
     {"packets/second"},
     {completed_sample},
     {NULL},
    },
    /* for avg time */
    {"平均收发包时间", 1, 100, 0,
     {"ms"},
     {avg_time_sample},
     {NULL},
    },
#if 0
    /* for wait queue length */
    {"等待队列长度", 1, 500, 0,
     {"n"},
     {wait_queue_length_sample},
     {NULL},
    },
#endif
    /* for error stat */
    {"丢包速率", 1, 100, 0,
     {"packets/second"},
     {lost_sample},
     {NULL},
    },
};

static void *
wrap_webmon_run(void *arg)
{
    begin_time = time(NULL);
    webmon_run(arg);
    return NULL;
}

int
main(int argc, char *argv[])
{
    int i, ret, pollfd, sock = -1, nfd, fd, idx, bidirectional = 0;
    struct epoll_event epollevt, outevtarr[SERV_PORT_RANGE];
    struct timeval tv;
    unsigned long long cur_time;
    struct dlist_t *dl;
    void *sd;
    pthread_t ptid;

    /* parse line args */
    while (1) {
        ret = getopt(argc, argv, "h:n:l:f:b");
        if (ret == -1) {
            if (argv[optind] != NULL) {
                show_help();
                exit(1);
            }
            break;      /* break while */
        }
        switch (ret) {
        case 'h':      /* SERV_ADDR */
            serv_addr = parse_serv_addr(optarg, &serv_addr_count);
            if (serv_addr == NULL) {
                show_help();
                exit(1);
            }
            break;
        case 'n':      /* PARALLEL_CLIENT */
            parallel_client = atoi(optarg);
            if (parallel_client > SERV_PORT_RANGE)
                parallel_client = SERV_PORT_RANGE;
            break;
        case 'l':      /* FRAME_LENGTH */
            packet_length = atoi(optarg) - 46;
            if (packet_length < UDP_PACKET_LEN_MIN)
                packet_length = UDP_PACKET_LEN_MIN;
            if (packet_length > UDP_PACKET_LEN_MAX)
                packet_length = UDP_PACKET_LEN_MAX;
            break;
        case 'f':      /* MAX_LIFE_TIME */
            max_life_time = atoi(optarg);
            break;
        case 'b':      /* BIDIRECTIONAL */
            bidirectional = 1;
            break;
        default:
            show_help();
            exit(1);
        }
    }

    /* init conn_arr */
    memset(conn_arr, 0, sizeof(conn_arr));
    wait_queue.prev = &wait_queue;
    wait_queue.next = &wait_queue;

    /* set signal handler */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);

    /* set resource limit */
    ret = setlimits(parallel_client + 20);
    if (ret == -1) {
        ERR;
        exit(1);
    }

    /* init epoll */
    pollfd = epoll_create(parallel_client);
    if (pollfd == -1) {
        ERR;
        exit(1);
    }

    /* create socket array */
    for (i = 0; i < parallel_client; i++) {
        /* create socket */
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            ERR;
            exit(1);
        }
        if (i == 0)
            sock_begin = sock;
        /* setnoblock */
        ret = setnoblock(sock);
        if (ret == -1) {
            ERR;
            exit(1);
        }
        /* for epoll */
        memset(&epollevt, 0, sizeof(epollevt));
        epollevt.data.fd = sock;
        epollevt.events = EPOLLOUT;
        ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, sock, &epollevt);
        if (ret == -1) {
            ERR;
            exit(1);
        }
    }
    /* check */
    if (sock != sock_begin + parallel_client - 1) {
        ERR;
        exit(1);
    }
        
    /* init and craete webmon */
    if (webmon_init() == -1) {
        ERR;
        exit(1);
    }
    sd = webmon_create("updtest client", "0.0.0.0", 8888, 900, 4, 1, 0, 0);
    if (sd == NULL) {
        ERR;
        exit(1);
    }
    /* set textinfo callback */
    webmon_set_textinfo_callback(sd, textinfo_cb, NULL);
    webmon_set_free_textinfo_callback(sd, free_textinfo_cb);
    /* add graphs */
    for (i = 0; i < sizeof(my_graphs) / sizeof(struct webmon_graph_t); i++) {
        if (webmon_addgraph(sd, &my_graphs[i]) == -1) {
            ERR;
            exit(1);
        }
    }
    /* start new thread */
    if (pthread_create(&ptid, NULL, wrap_webmon_run, sd) != 0) {
        ERR;
        exit(1);
    }

    /* main loop */
    while (1) {
        /* epoll events */
        nfd = epoll_wait(pollfd, outevtarr, parallel_client, 
                         max_life_time / 5/* ms */);
        if (nfd == -1) {
            if (errno == EINTR)
                continue;
            ERR;
            exit(1);
        } else {
            for (i = 0; i < nfd; i++) {
                fd = outevtarr[i].data.fd;

                if (outevtarr[i].events & EPOLLOUT) {
                    /* send */
                    if (send_udp_packet(fd, bidirectional) == 1) {
                        memset(&epollevt, 0, sizeof(epollevt));
                        epollevt.events = EPOLLIN;
                        epollevt.data.fd = fd;
                        ret = epoll_ctl(pollfd, EPOLL_CTL_MOD, fd, &epollevt);
                        if (ret == -1) {
                            ERR;
                            exit(1);
                        }
                    }
                } else if (outevtarr[i].events & EPOLLIN) {
                    /* recv */
                    if (recv_udp_packet(fd, bidirectional) == 1) {
                        memset(&epollevt, 0, sizeof(epollevt));
                        epollevt.events = EPOLLOUT;
                        epollevt.data.fd = fd;
                        ret = epoll_ctl(pollfd, EPOLL_CTL_MOD, fd, &epollevt);
                        if (ret == -1) {
                            ERR;
                            exit(1);
                        }
                    }
                } else {
                    ERR;
                    exit(1);
                }
            }

            /* for packet possibly lost */
            gettimeofday(&tv, NULL);
            cur_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            while ((dl = wait_queue.next) != &wait_queue) {
                /* expire? */
                if (cur_time - ((struct conn_item_t *)dl)->send_time 
                    >= max_life_time)
                {
                    /* lost */
                    lost_count++;
                    /* reset conn */
                    idx = ((struct conn_item_t *)dl) - conn_arr;
                    conn_arr[idx].status = FREE;
                    /* detach from wait_queue */
                    conn_arr[idx].list.prev->next = conn_arr[idx].list.next;
                    conn_arr[idx].list.next->prev = conn_arr[idx].list.prev;
                    conn_arr[idx].list.prev = NULL;
                    conn_arr[idx].list.next = NULL;
                    wait_queue_len--;
                    /* poll OUT */
                    memset(&epollevt, 0, sizeof(epollevt));
                    epollevt.events = EPOLLOUT;
                    epollevt.data.fd = sock_begin + idx;
                    ret = epoll_ctl(pollfd, EPOLL_CTL_MOD, sock_begin + idx, 
                                    &epollevt);
                    if (ret == -1) {
                        ERR;
                        exit(1);
                    }
                } else {
                    break;
                }
            }
        }
    }

    return 0;
}
