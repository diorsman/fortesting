/**
 *  File: httpc.c
 *
 *  Copyright (C) 2008 Du XiaoGang <dugang@188.com>
 *
 *  This file is part of testhttpd.
 *  
 *  testhttpd is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as 
 *  published by the Free Software Foundation, either version 3 of the 
 *  License, or (at your option) any later version.
 *  
 *  testhttpd is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with testhttpd.  If not, see <http://www.gnu.org/licenses/>.
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
#include <netinet/in.h>
#include <arpa/inet.h>

#define ENABLE_RESET

#define MAXSOCK     3500
#define SERVADDR    "192.168.6.48"
#define SERVPORT    80
#define RESP_MAX    4000
#define ERR(string) \
        do { \
            fprintf(stderr, \
                    "Unexpected error occurred(%s:%d), errno = %d.\n" \
                    "  AUX-Message: %s.\n", \
                    __FILE__, __LINE__, errno, string); \
            errno = 0; \
        } while (0);

struct connection_t {
#define CONNECTING      0
#define SENDING_REQ     1
#define RECVING_RESP    2
    int status;
    int sent, received;
    char rbuf[RESP_MAX + 1];
};

static unsigned long long request_sent = 0, connect_failed = 0, 
                          send_failed = 0, recv_failed = 0, 
                          completed = 0;

static void 
sig_handler(int signo)
{
    int bak_errno, len;
    char strbuf[1024];
    
    bak_errno = errno;
    len = snprintf(strbuf, sizeof(strbuf), 
                   "request(sent:%llu, completed:%llu)\n"
                   "error(connect:%llu, send:%llu, recv:%llu)\n", 
                   request_sent, completed, 
                   connect_failed, send_failed, recv_failed);
    write(1, strbuf, len);
    errno = bak_errno;

    if (signo == SIGQUIT)
        exit(0);
}

static int
set_limits(int maxfd)
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
set_noblock(int fd)
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
init_connection(int pollfd, struct sockaddr_in *servaddr)
{
    int sock, ret;
    struct epoll_event epollevt;
    struct connection_t *conn;

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        ERR("null");
        exit(1);
    }
    /* set noblocking */
    ret = set_noblock(sock);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
    /* new connection_t */
    conn = malloc(sizeof(struct connection_t));
    if (conn == NULL) {
        ERR("null");
        exit(1);
    }
    conn->sent = 0;
    conn->received = 0;
    /* prepare for epoll */
    memset(&epollevt, 0, sizeof(epollevt));
    epollevt.data.u64 = (((unsigned long long)(unsigned int)conn) << 32) 
                        | sock;
    /* try connect */
    ret = connect(sock, (struct sockaddr *)servaddr, 
                  sizeof(struct sockaddr_in));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            errno = 0;
            epollevt.events = EPOLLOUT | EPOLLET;
            conn->status = CONNECTING;
        } else {
            /* error */
            ERR("null");
            exit(1);
        }
    } else {
        /* connect successful */
        epollevt.events = EPOLLIN | EPOLLET;
        conn->status = SENDING_REQ;
    }
    /* add to poll array */
    ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, sock, &epollevt);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
}

#ifdef ENABLE_RESET 
static int
valid_response(char *resp, int resp_len)
{
    char *p;
    int real_len, len;

    /* split header/body */
    p = strstr(resp, "\r\n\r\n");
    if (p == NULL)
        return 0;
    *p = '\0';
    p += 4;
    real_len = resp + resp_len - p;
    /* read content length from the http header */
    p = strstr(resp, "Content-Length:");
    if (p == NULL)
        return 0;
    p += 15;
    p += strspn(p, " \t");
    len = atoi(p);
    if (len != real_len)
        return 0;
    return 1;
}
#endif

int
main(int argc, char *argv[])
{
    int ret, i, pollfd, nfd, fd, req_len;
    struct sockaddr_in servaddr;
    struct epoll_event epollevt, outevtarr[MAXSOCK];
    struct connection_t *conn;
    const char *req = "GET /index.html HTTP/1.1\r\n"
                      "Host: TestHttpd\r\n"
                      "User-Agent: TestHttpd/httpc\r\n"
                      "\r\n";
    char err_buf[1024];

    /* get req_len */
    req_len = strlen(req);
    /* set signal handler */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    /* set resource limit */
    ret = set_limits(MAXSOCK + 20);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
    /* init epoll */
    pollfd = epoll_create(MAXSOCK);
    if (pollfd == -1) {
        ERR("null");
        exit(1);
    }
    /* set server addrinfo */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(SERVADDR);
    servaddr.sin_port = htons(SERVPORT);
    /* connect server */
    for (i = 0; i < MAXSOCK; i++)
        init_connection(pollfd, &servaddr);

    /* main loop */
    while (1) {
        /* epoll events */
        nfd = epoll_wait(pollfd, outevtarr, MAXSOCK, -1);
        if (nfd == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            ERR("null");
            exit(1);
        } else {
            for (i = 0; i < nfd; i++) {
                fd = (int)(outevtarr[i].data.u64 & 0xffffffffLL);
                conn = (struct connection_t *)(unsigned int)(outevtarr[i].data.u64 >> 32);

                if (outevtarr[i].events & EPOLLOUT) {
                    if (conn->status == CONNECTING) {
                        /* maybe connect succeed */
                        int optval;
                        socklen_t optvallen = sizeof(optval);

                        ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, 
                                         &optval, &optvallen);
                        if (ret == -1) {
                            ERR("null");
                            exit(1);
                        }
                        if (optval == 0) {
                            /* connect successful */
                            conn->status = SENDING_REQ;
                            /* try to send http request */
                            while (conn->sent < req_len) {
                                ret = write(fd, req + conn->sent, 
                                            req_len - conn->sent);
                                if (ret == -1) {
                                    if (errno == EAGAIN) {
                                        errno = 0;
                                        break;
                                    }
                                    /* error, reconnect */
                                    send_failed++;
                                    close(fd);
                                    free(conn);
                                    init_connection(pollfd, &servaddr);
                                    break;
                                } else {
                                    conn->sent += ret;
                                    if (conn->sent == req_len) {
                                        /* all are sent, wait for response */
                                        request_sent++;
                                        conn->status = RECVING_RESP;
                                        /* EPOLL_CTL_MOD */
                                        memset(&epollevt, 0, sizeof(epollevt));
                                        epollevt.events = EPOLLIN | EPOLLET;
                                        epollevt.data.u64 = outevtarr[i].data.u64;
                                        ret = epoll_ctl(pollfd, EPOLL_CTL_MOD, 
                                                        fd, &epollevt);
                                        if (ret == -1) {
                                            ERR("null");
                                            exit(1);
                                        }
                                    }
                                    break;
                                }
                            }
                        } else {
                            /* connect failed, reconnect */
                            connect_failed++;
                            close(fd);
                            free(conn);
                            init_connection(pollfd, &servaddr);
                        }
                    } else {
                        /* sending request */
                        while (conn->sent < req_len) {
                            ret = write(fd, req + conn->sent, 
                                        req_len - conn->sent);
                            if (ret == -1) {
                                if (errno == EAGAIN) {
                                    errno = 0;
                                    break;
                                }
                                /* error, reconnect */
                                send_failed++;
                                close(fd);
                                free(conn);
                                init_connection(pollfd, &servaddr);
                                break;
                            } else {
                                conn->sent += ret;
                                if (conn->sent == req_len) {
                                    /* all are sent, wait for response */
                                    request_sent++;
                                    conn->status = RECVING_RESP;
                                    /* EPOLL_CTL_MOD */
                                    memset(&epollevt, 0, sizeof(epollevt));
                                    epollevt.events = EPOLLIN | EPOLLET;
                                    epollevt.data.u64 = outevtarr[i].data.u64;
                                    ret = epoll_ctl(pollfd, EPOLL_CTL_MOD, fd, 
                                                    &epollevt);
                                    if (ret == -1) {
                                        ERR("null");
                                        exit(1);
                                    }
                                }
                                break;
                            }
                        }
                    }
                } else if (outevtarr[i].events & EPOLLIN) {
                    /* data arrive */
                    while (1) {
#ifdef ENABLE_RESET
                        ret = read(fd, conn->rbuf + conn->received, 
                                   sizeof(conn->rbuf) - 1 - conn->received);
#else
                        ret = read(fd, conn->rbuf, sizeof(conn->rbuf));
#endif
                        if (ret == -1) {
                            if (errno == EAGAIN) {
                                errno = 0;
                                break;
#ifdef ENABLE_RESET
                            } else if (errno == ECONNRESET) {
                                conn->rbuf[conn->received] = '\0';
                                if (valid_response(conn->rbuf, conn->received)) {
                                    errno = 0;
                                    completed++;
                                } else {
                                    /* error, reconnect */
                                    ERR("null");
                                    recv_failed++;
                                }
                                close(fd);
                                free(conn);
                                init_connection(pollfd, &servaddr);
#endif
                            } else {
                                /* error, reconnect */
                                ERR("null");
                                recv_failed++;
                                close(fd);
                                free(conn);
                                init_connection(pollfd, &servaddr);
                            }
                        } else if (ret == 0) {
                            /* recv FIN, close and reconnect */
                            completed++;
                            close(fd);
                            free(conn);
                            init_connection(pollfd, &servaddr);
                        } else {
                            /* response */
                            conn->received += ret;
                        }
                    }
                } else {
                    /* error */
                    snprintf(err_buf, sizeof(err_buf), "events = 0x%08x", 
                             outevtarr[i].events);
                    ERR(err_buf);
                    if (conn->status == CONNECTING)
                        connect_failed++;
                    else if (conn->status == SENDING_REQ)
                        send_failed++;
                    else if (conn->status == RECVING_RESP)
                        recv_failed++;
                    /* clear */
                    close(fd);
                    free(conn);
                    /* err, reopen/connect */
                    init_connection(pollfd, &servaddr);
                }
            }
        }
    }

    return 0;
}
