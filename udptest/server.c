/**
 *  File: server.c
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
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_ADDR           "0.0.0.0"
#define SERV_PORT_BEGIN     8000
#define SERV_PORT_MAX_RANGE 1000
#define PACKET_MAX_LENGTH   2048
#define ERR \
        do { \
            fprintf(stderr, \
                    "Exception from (%s:%d), errno = %d.\n", \
                    __FILE__, __LINE__, errno); \
        } while (0);

struct conn_item_t {
    /* data */
    unsigned long data_len;
    char buf[PACKET_MAX_LENGTH];
    struct sockaddr_in faddr;
};

static struct conn_item_t conn_arr[SERV_PORT_MAX_RANGE];
static int sock_begin;

/* for stat */
static unsigned long recv_count = 0;
static unsigned long send_count = 0;

static int
setlimits(int maxfd)
{
    struct rlimit rlmt;

    if (getrlimit(RLIMIT_NOFILE, &rlmt))
        return -1;
    rlmt.rlim_cur = maxfd;
    rlmt.rlim_max = maxfd;
    if (setrlimit(RLIMIT_NOFILE, &rlmt))
        return -1;
    return 0;
}

static int
setnoblock(int fd)
{
    int flag, ret;

    flag = fcntl(fd, F_GETFL);
    if (flag == -1)
        return -1;
    ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    if (ret == -1)
        return -1;
    return 0;
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
                   "recv_count: %lu, send_count: %lu\n", 
                   recv_count, send_count);
    write(1, strbuf, len);
    errno = bakerrno;
}

/**
 *  0, go no
 *  1, complete
 */
static int
send_udp_packet(int sock_fd)
{
    int idx, ret;

    /* get conn index */
    idx = sock_fd - sock_begin;

    /* send */
    ret = sendto(sock_fd, conn_arr[idx].buf, conn_arr[idx].data_len, 0, 
                 (struct sockaddr *)&conn_arr[idx].faddr, 
                 sizeof(conn_arr[idx].faddr));
    if (ret == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            /* error */
            ERR;
            exit(1);
        }
    } else if (ret == conn_arr[idx].data_len) {
        conn_arr[idx].data_len = 0;
        send_count++;
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
recv_udp_packet(int sock_fd)
{
    int idx, ret, addr_len;

    /* get conn index */
    idx = sock_fd - sock_begin;
    if (conn_arr[idx].data_len != 0) {
        ERR;
        exit(1);
    }

    /* recv */
    while (1) {
        addr_len = sizeof(conn_arr[idx].faddr);
        ret = recvfrom(sock_fd, conn_arr[idx].buf, PACKET_MAX_LENGTH, 0, 
                       (struct sockaddr *)&conn_arr[idx].faddr, 
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

        conn_arr[idx].data_len = ret;
        recv_count++;
        return 1;
    }

    /* never to here */
    ERR;
    exit(1);
}

int
main(int argc, char *argv[])
{
    int i, ret, pollfd, sock, nfd, fd;
    struct sockaddr_in laddr;
    struct epoll_event epollevt, outevtarr[SERV_PORT_MAX_RANGE];

    /* set signal handler */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);

    /* set resource limit */
    ret = setlimits(SERV_PORT_MAX_RANGE + 20);
    if (ret == -1) {
        ERR;
        exit(1);
    }

    /* init epoll */
    pollfd = epoll_create(SERV_PORT_MAX_RANGE);
    if (pollfd == -1) {
        ERR;
        exit(1);
    }

    /* create socket array */
    for (i = 0; i < SERV_PORT_MAX_RANGE; i++) {
        /* create socket */
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            ERR;
            exit(1);
        }
        if (i == 0)
            sock_begin = sock;
        /* bind */
        memset(&laddr, 0, sizeof(laddr));
        laddr.sin_family = AF_INET;
        laddr.sin_addr.s_addr = inet_addr(SERV_ADDR);
        laddr.sin_port = htons(SERV_PORT_BEGIN + i);
        ret = bind(sock, (struct sockaddr *)&laddr, sizeof(laddr));
        if (ret == -1) {
            ERR;
            exit(1);
        }
        /* setnoblock */
        ret = setnoblock(sock);
        if (ret == -1) {
            ERR;
            exit(1);
        }
        /* for epoll */
        memset(&epollevt, 0, sizeof(epollevt));
        epollevt.data.fd = sock;
        epollevt.events = EPOLLIN;
        ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, sock, &epollevt);
        if (ret == -1) {
            ERR;
            exit(1);
        }
    }
    /* check */
    if (sock != sock_begin + SERV_PORT_MAX_RANGE - 1) {
        ERR;
        exit(1);
    }
        
    /* main loop */
    while (1) {
        /* epoll events */
        nfd = epoll_wait(pollfd, outevtarr, SERV_PORT_MAX_RANGE, -1);
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
                    if (send_udp_packet(fd) == 1) {
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
                    if (recv_udp_packet(fd) == 1) {
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
        }
    }

    return 0;
}
