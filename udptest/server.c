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
#include <signal.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

struct conn_item_t {
    /* data */
    unsigned long data_len;
    char buf[PACKET_LENGTH_MAX];
    struct sockaddr_in faddr;
};

static struct conn_item_t conn_arr[SERV_PORT_RANGE];
static int sock_begin;

/* for stat */
static unsigned long recv_count = 0;
static unsigned long send_count = 0;

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
    int idx, len = 0, ret;
    struct udpdata_t *ud;

    /* get conn index */
    idx = sock_fd - sock_begin;
    ud = (struct udpdata_t *)conn_arr[idx].buf;

    if (ud->type == REQ) {
        /* respond a ack */
        len = sizeof(struct udpdata_t);
    } else if (ud->type == BIDIR_REQ) {
        /* respond whole request data */
        len = conn_arr[idx].data_len;
    } else {
        /* ignore */
        conn_arr[idx].data_len = 0;
        return 1;
    }
    ret = sendto(sock_fd, conn_arr[idx].buf, len, 0, 
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
    } else if (ret == len) {
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
        ret = recvfrom(sock_fd, conn_arr[idx].buf, PACKET_LENGTH_MAX, 0, 
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
    struct epoll_event epollevt, outevtarr[SERV_PORT_RANGE];

    /* set signal handler */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);

    /* set resource limit */
    ret = setlimits(SERV_PORT_RANGE + 20);
    if (ret == -1) {
        ERR;
        exit(1);
    }

    /* init epoll */
    pollfd = epoll_create(SERV_PORT_RANGE);
    if (pollfd == -1) {
        ERR;
        exit(1);
    }

    /* create socket array */
    for (i = 0; i < SERV_PORT_RANGE; i++) {
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
        laddr.sin_addr.s_addr = inet_addr("0.0.0.0");
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
    if (sock != sock_begin + SERV_PORT_RANGE - 1) {
        ERR;
        exit(1);
    }
        
    /* main loop */
    while (1) {
        /* epoll events */
        nfd = epoll_wait(pollfd, outevtarr, SERV_PORT_RANGE, -1);
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
