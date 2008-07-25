/**
 *  File: webmon.c
 *
 *  Copyright (C) 2008 Du XiaoGang <dugang@188.com>
 *
 *  This file is part of libwebmon.
 *  
 *  libwebmon is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as 
 *  published by the Free Software Foundation, either version 3 of the 
 *  License, or (at your option) any later version.
 *  
 *  libwebmon is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libwebmon.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defgraphs.h"
#include "str.h"
#include "webmon.h"

#define SOFTWARE        "libwebmon"
#define VERSION         "0.0.1"
#define GRAPH_MAX       50
#define CONN_MAX        100
#define REQ_MAX         4096
#define URL_MAX         1024
#define POLL_INTERVAL   600
#define GRAPH_WIDTH_DEF 900
#define COMPRESS_HDRLEN 1024
#define CHUNK_HDRLEN_1  10
#define CHUNK_HDRLEN_2  7

/* States for checked_state(from thttpd/2.25b). */
#define CHST_FIRSTWORD  0
#define CHST_FIRSTWS    1
#define CHST_SECONDWORD 2
#define CHST_SECONDWS   3
#define CHST_THIRDWORD  4
#define CHST_THIRDWS    5
#define CHST_LINE       6
#define CHST_LF         7
#define CHST_CR         8                
#define CHST_CRLF       9
#define CHST_CRLFCR     10
#define CHST_BOGUS      11

#define GR_NO_REQUEST   0
#define GR_GOT_REQUEST  1
#define GR_BAD_REQUEST  2

struct dlist_t {
    struct dlist_t *prev;
    struct dlist_t *next;
};

struct webmon_t {
    /* init data */
    const char *title;
    int listen_fd;
    int nsample;
    int interval;
    TEXTINFO_CB textinfo_cb;
    void *textinfo_cb_arg;
    FREE_TEXTINFO_CB free_textinfo_cb;
    int graph_count;
    const struct webmon_graph_t *graphs[GRAPH_MAX];
    GRAPH_CB graph_cb;
    void *graph_cb_arg;
    FREE_GRAPH_CB free_graph_cb;
    /* dync data */
    time_t stat_begin_time;
    double *values[GRAPH_MAX][LINE_COUNT_MAX];
    int begin_idx;
    int stat_count;
    struct dlist_t conn_list;
};

struct entity_t {
    union {
        int status;
        const char *name;
    } u;
    char *header;
    int header_dyn;
    size_t header_length;
    char *body;
    int body_dyn;
    size_t body_length;
};

struct connection_t {
    /* dlist */
    /* dlist must be the first item of connection_t */
    struct dlist_t list;
    /* data */
    int fd;
    /* request */
    char req_buf[REQ_MAX];
    size_t req_read;
    size_t req_checked;
    int checked_state;
#define METHOD_HEAD     0
#define METHOD_GET      1
    int method;
    /* response */
    struct entity_t *entity;
    int entity_dyn;
    size_t header_sent;
    size_t body_sent;
};

static struct entity_t *error_arr = NULL;
static size_t error_count, error_alloced;

static int
error_cmp(const void *a, const void *b)
{
    struct entity_t *ea, *eb;

    ea = (struct entity_t*)a;
    eb = (struct entity_t*)b;
    return (ea->u.status - eb->u.status);
}

#define ERROR_ALLOC_STEP    16
static int
error_install(int status, const char *text)
{
    void *tmpp;
    int ret;
    char header[4096];

    if (error_count == error_alloced) {
        error_alloced += ERROR_ALLOC_STEP;
        tmpp = realloc(error_arr, sizeof(struct entity_t) * error_alloced);
        if (tmpp == NULL) {
            return -1;
        }
        error_arr = tmpp;
    }
    /* status */
    error_arr[error_count].u.status = status;
    /* header */
    ret = snprintf(header, sizeof(header), 
                   "HTTP/1.1 %d %s\r\n"
                   "Server: %s-%s\r\n"
                   "Content-Length: 0\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n", 
                   status, text, SOFTWARE, VERSION);
    if (ret >= sizeof(header)) {
        return -1;
    }
    error_arr[error_count].header_dyn = 0;
    error_arr[error_count].header_length = ret;
    error_arr[error_count].header = strdup(header);
    if (error_arr[error_count].header == NULL) {
        return -1;
    }
    /* body */
    error_arr[error_count].body_dyn = 0;
    error_arr[error_count].body_length = 0;
    error_arr[error_count].body = NULL;

    error_count++;
    return 0;
}

static int
error_init(void)
{
    int i;

    error_count = 0;
    error_alloced = ERROR_ALLOC_STEP;
    error_arr = malloc(sizeof(struct entity_t) * error_alloced);
    if (error_arr == NULL) {
        return -1;
    }

    /* install */
    if (error_install(304, "Not Modified") == -1) 
        goto err;
    if (error_install(400, "Bad Request") == -1) 
        goto err;
    if (error_install(404, "Not Found") == -1) 
        goto err;
    if (error_install(413, "Request Entity Too Large") == -1) 
        goto err;
    if (error_install(414, "Request-URI Too Long") == -1) 
        goto err;
    if (error_install(500, "Internal Server Error") == -1) 
        goto err;
    if (error_install(501, "Not Implemented") == -1) 
        goto err;
    
    qsort(error_arr, error_count, sizeof(struct entity_t), error_cmp);
    return 0;

err:
    for (i = 0; i < error_count; i++)
        free(error_arr[i].header);
    free(error_arr);
    return -1;
}

int
webmon_init(void)
{
    /* error init */
    return error_init();
}

static void
send_error(struct connection_t *c, int status)
{
    struct entity_t s, *rets;

    s.u.status = status;
    rets = (struct entity_t*)bsearch(&s, error_arr, error_count,
                                     sizeof(struct entity_t), error_cmp);
    c->entity = rets;
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

static int
listen_init(const char *ip, int port)
{
    int listenfd, ret, on;
    struct sockaddr_in localaddr;

    /* init listen port */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        return -1;
    }
    /* SO_REUSEADDR */
    on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, 
               sizeof(on));
    /* bind */
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr(ip);
    localaddr.sin_port = htons(port);
    ret = bind(listenfd, (struct sockaddr *)&localaddr, 
               sizeof(localaddr));
    if (ret == -1) {
        close(listenfd);
        return -1;
    }
    /* noblock */
    ret = set_noblock(listenfd);
    if (ret == -1) {
        /* ignore and next */
        close(listenfd);
        return -1;
    }
    /* listen */
    ret = listen(listenfd, 5);
    if (ret == -1) {
        close(listenfd);
        return -1;
    }

    return listenfd;
}

extern struct webmon_graph_t def_graphs[];

void *
webmon_create(const char *title, const char *ip, int port, int nsample, 
              int interval, int cpu, int mem, int netif)
{
    struct webmon_t *rets = NULL;

    rets = calloc(1, sizeof(struct webmon_t));
    if (rets == NULL) {
        return NULL;
    }
    /* init struct */
    rets->title = title;
    /* init listen socket */
    rets->listen_fd = listen_init(ip, port);
    if (rets->listen_fd == -1) {
        free(rets);
        return NULL;
    }
    rets->nsample = nsample;
    rets->interval = interval;
    /* add default graphs */
    if (cpu) {
        if (webmon_addgraph(rets, &def_graphs[CPU]) == -1) {
            free(rets);
            return NULL;
        }
    }
    if (mem) {
        if (webmon_addgraph(rets, &def_graphs[MEM]) == -1) {
            free(rets);
            return NULL;
        }
    }
    if (netif) {
        if (add_netif_graphs(rets, "eth") == -1) {
            free(rets);
            return NULL;
        }
    }
    return rets;
}

void
webmon_set_textinfo_callback(void *webmon, TEXTINFO_CB cb, void *arg)
{
    struct webmon_t *s = (struct webmon_t *)webmon;
    s->textinfo_cb = cb;
    s->textinfo_cb_arg = arg;
}

void
webmon_set_free_textinfo_callback(void *webmon, FREE_TEXTINFO_CB cb)
{
    struct webmon_t *s = (struct webmon_t *)webmon;
    s->free_textinfo_cb = cb;
}

int 
webmon_addgraph(void *webmon, const struct webmon_graph_t *graph)
{
    struct webmon_t *s = (struct webmon_t *)webmon;

    if (s->graph_count == GRAPH_MAX)
        return -1;
    s->graphs[s->graph_count] = graph;
    s->graph_count++;
    /* ok */
    return 0;
}

void
webmon_set_graph_callback(void *webmon, GRAPH_CB cb, void *arg)
{
    struct webmon_t *s = (struct webmon_t *)webmon;
    s->graph_cb = cb;
    s->graph_cb_arg = arg;
}

void 
webmon_set_free_graph_callback(void *webmon, FREE_GRAPH_CB cb)
{
    struct webmon_t *s = (struct webmon_t *)webmon;
    s->free_graph_cb = cb;
}

static int
webmon_dync_init(struct webmon_t *s)
{
    int i, j, k, l;

    /* init webmon's dync part */
    s->stat_begin_time = time(NULL);
    for (i = 0; i < s->graph_count; i++) {
        for (j = 0; j < s->graphs[i]->line_count; j++) {
            s->values[i][j] = malloc(s->nsample * sizeof(double));
            if (s->values[i][j] == NULL) {
                /* error */
                goto err;
            }
        }
    }
    s->begin_idx = 0;
    s->stat_count = 0;
    /* init connection list */
    s->conn_list.next = &s->conn_list;
    s->conn_list.prev = &s->conn_list;
    return 0;

err:
    for (k = 0; k <= i; k++) {
        for (l = 0; l < j; l++) {
            free(s->values[k][l]);
        }
    }
    return -1;
}

static void
clear_connection(struct connection_t *c)
{
    /* detach */
    c->list.prev->next = c->list.next;
    c->list.next->prev = c->list.prev;
    /* clear */
    close(c->fd);
    if (c->entity_dyn) {
        if (c->entity->header_dyn)
            free(c->entity->header);
        if (c->entity->body_dyn)
            free(c->entity->body);
        free(c->entity);
    }
    free(c);
}

/* from thttpd/2.25b. */
static int
got_request(struct connection_t *conn)
{
    char c;

    for (; conn->req_checked < conn->req_read; ++conn->req_checked) {
        c = conn->req_buf[conn->req_checked];
        switch (conn->checked_state) {
        case CHST_FIRSTWORD:
            switch (c) {
            case ' ':
            case '\t': 
                conn->checked_state = CHST_FIRSTWS;
                break;
            case '\n':
            case '\r': 
                conn->checked_state = CHST_BOGUS;
                return GR_BAD_REQUEST;
            }
            break;
        case CHST_FIRSTWS:
            switch (c) {
            case ' ':
            case '\t':
                break;
            case '\n':
            case '\r':
                conn->checked_state = CHST_BOGUS;
                return GR_BAD_REQUEST;
            default:
                conn->checked_state = CHST_SECONDWORD;
                break;
            }
            break;
        case CHST_SECONDWORD:
            switch (c) {
            case ' ':
            case '\t':
                conn->checked_state = CHST_SECONDWS;
                break;
            case '\n':
            case '\r':
                conn->checked_state = CHST_BOGUS;
                return GR_BAD_REQUEST;
            }
            break;
        case CHST_SECONDWS:
            switch (c) {
            case ' ':
            case '\t':
                break;
            case '\n':
            case '\r':
                conn->checked_state = CHST_BOGUS;
                return GR_BAD_REQUEST;
            default:
                conn->checked_state = CHST_THIRDWORD;
                break;
            }
            break;
        case CHST_THIRDWORD:
            switch (c) {
            case ' ':
            case '\t':
                conn->checked_state = CHST_THIRDWS;
                break;
            case '\n':
                conn->checked_state = CHST_LF;
                break;
            case '\r':
                conn->checked_state = CHST_CR;
                break;
            }
            break;
        case CHST_THIRDWS:
            switch (c) {
            case ' ':
            case '\t':
                break;
            case '\n':
                conn->checked_state = CHST_LF;
                break;
            case '\r':
                conn->checked_state = CHST_CR;
                break;
            default:
                conn->checked_state = CHST_BOGUS;
                return GR_BAD_REQUEST;
            }
            break;
        case CHST_LINE:
            switch (c) {
            case '\n':
                conn->checked_state = CHST_LF;
                break;
            case '\r':
                conn->checked_state = CHST_CR;
                break;
            }
            break;
        case CHST_LF:
            switch (c) {
            case '\n':
                /* Two newlines in a row - a blank line - end of request. */
                return GR_GOT_REQUEST;
            case '\r':
                conn->checked_state = CHST_CR;
                break;
            default:
                conn->checked_state = CHST_LINE;
                break;
            }
            break;
        case CHST_CR:
            switch (c) {
            case '\n':
                conn->checked_state = CHST_CRLF;
                break;
            case '\r':
                /* Two returns in a row - end of request. */
                return GR_GOT_REQUEST;
            default:
                conn->checked_state = CHST_LINE;
                break;
            }
            break;
        case CHST_CRLF:
            switch (c) {
            case '\n':
                /* Two newlines in a row - end of request. */
                return GR_GOT_REQUEST;
            case '\r':
                conn->checked_state = CHST_CRLFCR;
                break;
            default:
                conn->checked_state = CHST_LINE;
                break;
            }
            break;
        case CHST_CRLFCR:
            switch (c) {
            case '\n':
            case '\r':
                /* Two CRLFs or two CRs in a row - end of request. */
                return GR_GOT_REQUEST;
            default:
                conn->checked_state = CHST_LINE;
                break;
            }
            break;
        case CHST_BOGUS:
            return GR_BAD_REQUEST;
        }
    }
    return GR_NO_REQUEST;
}

static int
hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;           /* shouldn't happen, we're guarded by isxdigit() */
}

/* Copies and decodes a string.  It's ok for from and to to be the
** same string.
*/
static void
url_decode(char *to, char *from)
{
    for (; *from != '\0'; ++to, ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}

static void
de_dotdot(char *file)
{
    char *cp;
    char *cp2;
    int l;

    /* Collapse any multiple / sequences. */
    while ((cp = strstr(file, "//")) != (char *) 0) {
        for (cp2 = cp + 2; *cp2 == '/'; ++cp2)
            continue;
        (void) strcpy(cp + 1, cp2);
    }

    /* Remove leading ./ and any /./ sequences. */
    while (strncmp(file, "./", 2) == 0)
        (void) strcpy(file, file + 2);
    while ((cp = strstr(file, "/./")) != (char *) 0)
        (void) strcpy(cp, cp + 2);

    /* Alternate between removing leading ../ and removing xxx/../ */
    for (;;) {
        while (strncmp(file, "../", 3) == 0)
            (void) strcpy(file, file + 3);
        cp = strstr(file, "/../");
        if (cp == (char *) 0)
            break;
        for (cp2 = cp - 1; cp2 >= file && *cp2 != '/'; --cp2)
            continue;
        (void) strcpy(cp2 + 1, cp + 4);
    }

    /* Also elide any xxx/.. at the end. */
    while ((l = strlen(file)) > 3 && strcmp((cp = file + l - 3), "/..") == 0) {
        for (cp2 = cp - 1; cp2 >= file && *cp2 != '/'; --cp2)
            continue;
        if (cp2 < file)
            break;
        *cp2 = '\0';
    }
}

#ifdef COMPRESS

#include <zlib.h>

static int
compression(const char *inbuf, size_t inlen, char *outbuf, size_t outlen)
{
    int ret;
    z_stream strm;
    
    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    //////////////////////////////////////////////////
    // for zlib/deflate
    //ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    ////////////////////////////////////////////////////////////
    // for gzip/deflate
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       31, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return -1;
    }

    strm.avail_in = inlen;
    strm.next_in = (unsigned char *)inbuf;
    strm.avail_out = outlen;
    strm.next_out = (unsigned char *)outbuf;
    ret = deflate(&strm, Z_FINISH);    /* no bad return value */
    if (ret != Z_STREAM_END) {
        return -1;
    }
    ret = outlen - strm.avail_out;

    /* clean up and return */
    (void) deflateEnd(&strm);
    return ret;
}

#endif

struct entity_t *
make_statpage(struct webmon_t *s)
{
    struct entity_t *rets;
    struct string_t *out_str, *labels_str;
    char buf[4096], *comress_body;
    const char **pp, **pp2;
    int ret, i, j, k, l, m;
    static char *statpage_begin =
        "<html>\r\n"
        "    <head>\r\n"
        "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\r\n"
        "        <title>%s</title>\r\n"
        "    </head>\r\n"
        "    <body>\r\n"
        "        <table align=\"center\" border=\"0\">\r\n"
        "            <tr><td align=\"center\">\r\n"
        "                <hr>\r\n"
        "                <h1>%s</h1>\r\n"
        "                <hr>\r\n"
        "            </td></tr>\r\n"
        "            <tr><td align=\"right\">\r\n"
        "                <b>For the last %d seconds</b>\r\n"
        "                <hr>\r\n"
        "            </td></tr>\r\n"
        "            <tr><td align=\"left\">\r\n"
        "                <b>User defined information:</b>\r\n"
        "                <hr>\r\n"
        "            </td></tr>\r\n"
        "\r\n";
    static char *statpage_text_tpl = 
        "            <tr><td align=\"left\">%s</td></tr>\r\n";
    static char *statpage_graph_begin = 
        "            <tr><td align=\"left\">\r\n"
        "                <hr>\r\n"
        "            </td></tr>\r\n"
        "            <tr><td align=\"center\">\r\n"
        "                <applet code=\"com.objectplanet.chart.ChartApplet.class\" codebase=\"/local/\" archive=\"chart.jar\" width=\"%d\" height=\"320\">\r\n"
        "                <param name=\"chart\" value=\"line\">\r\n"
        "                <param name=\"seriesCount\" value=\"%d\">\r\n"
        "                <param name=\"sampleValues_0\" value=\"";
    static char *statpage_graph_mid = 
        "\">\r\n"
        "                <param name=\"sampleValues_%d\" value=\"";
    static char *statpage_graph_end = 
        "\">\r\n"
        "                <param name=\"sampleColors\" value=\"#FF0000,#00FF00,#0000FF,#FFFF00,#00FFFF,#FF00FF\">\r\n"
        "                <param name=\"range\" value=\"%0.2f\">\r\n"
        "                <param name=\"lowerRange\" value=\"%0.2f\">\r\n"
        "                <param name=\"legendOn\" value=\"true\">\r\n"
        "                <param name=\"legendPosition\" value=\"bottom\">\r\n"
        "                <param name=\"legendLabels\" value=\"%s\">\r\n"
        "                <param name=\"valueLinesOn\" value=\"true\">\r\n"
        "                </applet>\r\n"
        "            </td></tr>\r\n"
        "            <tr><td align=\"center\">\r\n"
        "                <b>%s</b>\r\n"
        "                <hr>\r\n"
        "            </td></tr>\r\n"
        "\r\n";
    static char *statpage_end =
        "            <tr><td align=\"right\">\r\n"
        "                <b>This page is automatically created by <a href=\"http://code.google.com/p/fortesting/\">%s-%s<a>.</b>\r\n"
        "            </td></tr>\r\n"
        "        </table>\r\n"
        "    </body>\r\n"
        "</html>\r\n";

    rets = malloc(sizeof(struct entity_t));
    if (rets == NULL) {
        return NULL;
    }
    out_str = string_create(0, 0);
    if (out_str == NULL) {
        free(rets);
        return NULL;
    }

    /* for page begin */
    i = time(NULL) - s->stat_begin_time;
    j = s->nsample * s->interval;
    ret = snprintf(buf, sizeof(buf), statpage_begin, s->title, s->title, 
                   i < j ? i : j);
    if (ret >= sizeof(buf))
        goto err;
    if (string_add(out_str, buf, ret) == -1)
        goto err;
    /* for user defined information */
    if (s->textinfo_cb != NULL) {
        pp = s->textinfo_cb(s->textinfo_cb_arg);
        /* backup pp */
        pp2 = pp;
        if (pp == NULL)
            goto err;
        while (*pp != NULL) {
            /* text info */
            ret = snprintf(buf, sizeof(buf), statpage_text_tpl, *pp);
            if (ret >= sizeof(buf)) {
                /* not error */
                ret = sizeof(buf) - 1;
            }
            if (string_add(out_str, buf, ret) == -1)
                goto err;
            /* next */
            pp++;
        }
        /* need free? */
        if (s->free_textinfo_cb != NULL) 
            s->free_textinfo_cb(pp2);
    }
    /* for each graph */
    for (i = 0; i < s->graph_count; i++) {
        /* statpage_graph_begin */
        ret = snprintf(buf, sizeof(buf), statpage_graph_begin, 
                       s->nsample > GRAPH_WIDTH_DEF ? s->nsample : GRAPH_WIDTH_DEF, 
                       s->graphs[i]->line_count); 
        if (ret >= sizeof(buf))
            goto err;
        if (string_add(out_str, buf, ret) == -1)
            goto err;
        /* values 0 */
        if (s->stat_count >= s->nsample) {
            l = s->begin_idx + s->nsample;
        } else {
            l = s->begin_idx + s->stat_count;
        }
        for (j = s->begin_idx; j < l; j++) {
            /* value */
            ret = snprintf(buf, sizeof(buf), "%0.2f,", 
                           s->values[i][0][j % s->nsample]);
            if (ret >= sizeof(buf))
                goto err;
            if (string_add(out_str, buf, ret) == -1)
                goto err;
        }
        /* have more than 1 line? */
        if (s->graphs[i]->line_count > 1) {
            for (k = 1; k < s->graphs[i]->line_count; k++) {
                /* statpage_graph_mid */
                ret = snprintf(buf, sizeof(buf), statpage_graph_mid, k);
                if (ret >= sizeof(buf))
                    goto err;
                if (string_add(out_str, buf, ret) == -1)
                    goto err;
                /* values k */
                for (j = s->begin_idx; j < l; j++) {
                    /* value */
                    ret = snprintf(buf, sizeof(buf), "%0.2f,", 
                                   s->values[i][k][j % s->nsample]);
                    if (ret >= sizeof(buf))
                        goto err;
                    if (string_add(out_str, buf, ret) == -1)
                        goto err;
                }
            }
        }
        /* statpage_graph_end */
        /* for labels */
        labels_str = string_create(0, 0);
        if (labels_str == NULL)
            goto err;
        for (m = 0; m < s->graphs[i]->line_count; m++) {
            if (string_add(labels_str, s->graphs[i]->line_labels[m], 0) 
                == -1) 
            {
                string_free(labels_str);
                goto err;
            }
            if (string_add(labels_str, ",", 1) == -1) {
                string_free(labels_str);
                goto err;
            }
        }
        ret = snprintf(buf, sizeof(buf), statpage_graph_end, 
                       s->graphs[i]->high_limit, 
                       s->graphs[i]->low_limit, 
                       string_get(labels_str),
                       s->graphs[i]->desc); 
        string_free(labels_str);
        if (ret >= sizeof(buf))
            goto err;
        if (string_add(out_str, buf, ret) == -1)
            goto err;
    }
    /* for each graph need callback */
    if (s->graph_cb != NULL) {
        const struct webmon_graph_data_t **gd, **gd_bak;

        gd = s->graph_cb(s->graph_cb_arg);
        /* backup */
        gd_bak = gd;
        if (gd == NULL) 
            goto err;
        /* draw */
        while (*gd != NULL) {
            /* statpage_graph_begin */
            ret = snprintf(buf, sizeof(buf), statpage_graph_begin, 
                (*gd)->nsample > GRAPH_WIDTH_DEF ? (*gd)->nsample : GRAPH_WIDTH_DEF, 
                (*gd)->line_count); 
            if (ret >= sizeof(buf))
                goto err;
            if (string_add(out_str, buf, ret) == -1)
                goto err;
            /* values 0 */
            for (j = 0; j < (*gd)->nsample; j++) {
                /* value */
                ret = snprintf(buf, sizeof(buf), "%0.2f,", 
                               (*gd)->values[0][j]);
                if (ret >= sizeof(buf))
                    goto err;
                if (string_add(out_str, buf, ret) == -1)
                    goto err;
            }
            /* have more than 1 line? */
            if ((*gd)->line_count > 1) {
                for (k = 1; k < (*gd)->line_count; k++) {
                    /* statpage_graph_mid */
                    ret = snprintf(buf, sizeof(buf), statpage_graph_mid, k);
                    if (ret >= sizeof(buf))
                        goto err;
                    if (string_add(out_str, buf, ret) == -1)
                        goto err;
                    /* values k */
                    for (j = 0; j < (*gd)->nsample; j++) {
                        /* value */
                        ret = snprintf(buf, sizeof(buf), "%0.2f,", 
                                       (*gd)->values[k][j]);
                        if (ret >= sizeof(buf))
                            goto err;
                        if (string_add(out_str, buf, ret) == -1)
                            goto err;
                    }
                }
            }
            /* statpage_graph_end */
            /* for labels */
            labels_str = string_create(0, 0);
            if (labels_str == NULL)
                goto err;
            for (m = 0; m < (*gd)->line_count; m++) {
                if (string_add(labels_str, (*gd)->line_labels[m], 0) 
                    == -1) 
                {
                    string_free(labels_str);
                    goto err;
                }
                if (string_add(labels_str, ",", 1) == -1) {
                    string_free(labels_str);
                    goto err;
                }
            }
            ret = snprintf(buf, sizeof(buf), statpage_graph_end, 
                           (*gd)->high_limit, (*gd)->low_limit, 
                           string_get(labels_str), (*gd)->desc); 
            string_free(labels_str);
            if (ret >= sizeof(buf))
                goto err;
            if (string_add(out_str, buf, ret) == -1)
                goto err;
            /* next */
            gd++;
        }
        /* need free? */
        if (s->free_graph_cb != NULL)
            s->free_graph_cb(gd_bak);
    }
    /* for page end */
    ret = snprintf(buf, sizeof(buf), statpage_end, SOFTWARE, VERSION);
    if (ret >= sizeof(buf))
        goto err;
    if (string_add(out_str, buf, ret) == -1)
        goto err;

    /* entity_t and body */
#ifdef COMPRESS
    /* for compress */
    comress_body = malloc(string_len(out_str) + COMPRESS_HDRLEN + 
                          CHUNK_HDRLEN_1 + CHUNK_HDRLEN_2);
    if (comress_body == NULL)
        goto err;
    ret = compression(string_get(out_str), string_len(out_str), 
                      comress_body + CHUNK_HDRLEN_1, 
                      string_len(out_str) + COMPRESS_HDRLEN);
    if (ret == -1 || ret >= string_len(out_str) + COMPRESS_HDRLEN) {
        free(comress_body);
        goto err;
    }
    string_free(out_str);
    /* make chunked header */
    snprintf(buf, sizeof(buf), "%08X\r\n", ret);
    memcpy(comress_body, buf, CHUNK_HDRLEN_1);
    /* append chunked tailer */
    strcpy(comress_body + CHUNK_HDRLEN_1 + ret, "\r\n0\r\n\r\n");
    /* assign */
    rets->body_length = ret + CHUNK_HDRLEN_1 + CHUNK_HDRLEN_2;
    rets->body_dyn = 1;
    rets->body = comress_body;
    /* header */
    ret = snprintf(buf, sizeof(buf),
                   "HTTP/1.1 200 OK\r\n"
                   "Server: %s-%s\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Encoding: gzip\r\n"
                   "Transfer-Encoding: chunked\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   SOFTWARE, VERSION);
#else
    rets->body_length = string_len(out_str);
    rets->body_dyn = 1;
    rets->body = string_get(out_str);
    string_free2(out_str);
    /* header */
    ret = snprintf(buf, sizeof(buf),
                   "HTTP/1.1 200 OK\r\n"
                   "Server: %s-%s\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   SOFTWARE, VERSION);
#endif
    rets->header = buf;
    rets->header_length = ret;
    rets->header_dyn = 0;

    return rets;

err:
    string_free(out_str);
    free(rets);
    return NULL;
}

struct entity_t *
make_chartjar(void)
{
    struct entity_t *rets;
    int len;
    time_t now;
    char nowbuf[100], expbuf[100], buf[4096];
    const char *rfc1123fmt = "%a, %d %b %Y %H:%M:%S GMT";

    extern char chart_jar[];
    extern int chart_jar_len;

    /* entity_t and body */
    rets = malloc(sizeof(struct entity_t));
    if (rets == NULL)
        return NULL;

    now = time(NULL);
    strftime(nowbuf, sizeof(nowbuf), rfc1123fmt, gmtime(&now));
    /* chart.jar will be expired after one year. */
    now += 365 * 24 * 3600; 
    strftime(expbuf, sizeof(expbuf), rfc1123fmt, gmtime(&now));

    /* header */
    len = snprintf(buf, sizeof(buf),
                   "HTTP/1.1 200 OK\r\n"
                   "Server: %s-%s\r\n"
                   "Content-Type: application/x-java-archive\r\n"
                   "Content-Length: %d\r\n"
                   "Last-Modified: Wed, 13 Feb 2008 10:41:12 GMT\r\n"
                   "Date: %s\r\n"
                   "Expires: %s\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n", 
                   SOFTWARE, VERSION, chart_jar_len, nowbuf, expbuf);
    rets->header = strdup(buf);
    if (rets->header == NULL) {
        free(rets);
        return NULL;
    }
    rets->header_length = len;
    rets->header_dyn = 1;

    /* body */
    rets->body = chart_jar;
    rets->body_length = chart_jar_len;
    rets->body_dyn = 0;

    return rets;
}

static int 
parse_request(struct connection_t *c, struct webmon_t *s)
{
    char *p, *q, *rest, url[URL_MAX];

    p = c->req_buf;
    /* get http method */
    p += strspn(p, " \t");
    q = strpbrk(p, " \t");
    if (q == NULL) {
        /* Bad Request */
        return -1;
    }
    if (q - p == 4 && p[0] == 'H' && p[1] == 'E' && p[2] == 'A' && p[3] == 'D') {
        /* HEAD */
        c->method = METHOD_HEAD;
    } else if (q - p == 3 && p[0] == 'G' && p[1] == 'E' && p[2] == 'T') {
        /* GET */
        c->method = METHOD_GET;
    } else {
        /* Not Implemented */
        return -1;
    }

    /* get dest url */
    p = q;
    p += strspn(p, " \t");
    q = strpbrk(p, " \t");
    if (q == NULL) {
        /* Bad Request */
        return -1;
    }
    *q = '\0';
    rest = q + 1;
    /* check url length */
    if (q - p >= URL_MAX) {
        /* Request-URI Too Long */
        return -1;
    }
    /* absolute url? */
    if (*p != '/') {
        /* Bad Request */
        return -1;
    }
    /* remove query part */
    q = strchr(p, '?');
    if (q != NULL)
        *q = '\0';
    /* url decode */
    url_decode(p, p);
    /* remove leading '/' */
    strcpy(url, p + 1);
    if (url[0] == '\0')
        strcpy(url, "index.html");

    /* for security */
    de_dotdot(url);
    if (url[0] == '/'
        || (url[0] == '.' && url[1] == '.' && (url[2] == '\0' || url[2] == '/')))
    {
        /* Bad Request */
        return -1;
    }

    if (strcmp(url, "index.html") == 0) {
        c->entity = make_statpage(s);
        if (c->entity == NULL) {
            return -1;
        } else {
            /* need free */
            c->entity_dyn = 1;
            return 0;
        }
    } else if (strcmp(url, "local/chart.jar") == 0) {
        /* conditional GET? */
        if (strcasestr(rest, "If-Modified-Since") != NULL) {
            /* always 304 */
            send_error(c, 304);
            return 0;
        } else {
            c->entity = make_chartjar();
            if (c->entity == NULL) {
                return -1;
            } else {
                /* need free */
                c->entity_dyn = 1;
                return 0;
            }
        }
    }

    /* Not Found */
    return -1;
}

static int
sample(struct webmon_t *s)
{
    time_t t;
    static time_t last_sample_time = -1;
    int i, j, idx;
    SAMPLE func;
    void *arg;
    double v;

    t = time(NULL);
    if (last_sample_time != -1) {
        /* not the first time */
        if (t - last_sample_time < s->interval)
            return 0;
    }

    /* sample */
    last_sample_time = t;
    idx = s->stat_count % s->nsample;
    for (i = 0; i < s->graph_count; i++) {
        for (j = 0; j < s->graphs[i]->line_count; j++) {
            func = s->graphs[i]->samples[j];
            arg = s->graphs[i]->args[j];
            if (func(arg, &v) == -1)
                return -1;
            if (v > s->graphs[i]->high_limit) {
                v = s->graphs[i]->high_limit;
            } else if (v < s->graphs[i]->low_limit) {
                v = s->graphs[i]->low_limit;
            } else if (v != v) {
                /* NaN */
                v = s->graphs[i]->low_limit;
            }
            s->values[i][j][idx] = v;
        }
    }
    /* update */
    if (s->stat_count >= s->nsample) {
        s->begin_idx++;
        if (s->begin_idx == s->nsample)
            s->begin_idx = 0;
    }
    s->stat_count++;

    return 0;
}

static void
webmon_dync_clean(struct webmon_t *s)
{
    int i, j;
    struct connection_t *conn;

    /* clean webmon's dync part */
    for (i = 0; i < s->graph_count; i++) {
        for (j = 0; j < s->graphs[i]->line_count; j++) {
            free(s->values[i][j]);
        }
    }
    /* clean all */
    while (s->conn_list.next != &s->conn_list) {
        conn = (struct connection_t *)s->conn_list.next;
        clear_connection(conn);
    }
}

int 
webmon_run(void *webmon)
{
    int epollfd = -1, ret, nfd, i, cur_fd, total_conns = 0, conn_fd;
    struct epoll_event epollevt, outevtarr[CONN_MAX + 1];
    struct webmon_t *s = (struct webmon_t *)webmon;
    struct connection_t *conn;

    if (webmon_dync_init(s) == -1)
        return -1;

    /* init epoll */
    epollfd = epoll_create(CONN_MAX + 1);
    if (epollfd == -1) { 
        goto err;
    }
    /* add listenfd to epoll array */
    memset(&epollevt, 0, sizeof(epollevt));
    epollevt.events = EPOLLIN;
    epollevt.data.fd = s->listen_fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, s->listen_fd, &epollevt);
    if (ret == -1) {
        goto err;
    }

    /* main loop */
    while (1) {
        nfd = epoll_wait(epollfd, outevtarr, CONN_MAX + 1, POLL_INTERVAL);
        if (nfd == -1) {
            if (errno == EINTR)
                continue;
            /* other error */
            goto err;
        }
        /* have some events or timeout */
        for (i = 0; i < nfd; i++) {
            cur_fd = outevtarr[i].data.fd;
            /* for listenfd */
            if (cur_fd == s->listen_fd) {
                if (outevtarr[i].events & EPOLLIN) {
                    /* new requests arrived */
                    while (1) {
                        if (total_conns == CONN_MAX) {
                            /* reach connections limit, ignore */
                            break;
                        }
                        /* accept */
                        conn_fd = accept(cur_fd, NULL, NULL);
                        if (conn_fd == -1) {
                            /* dont call accept again */
                            break;
                        }
                        /* no-blocking */
                        ret = set_noblock(conn_fd);
                        if (ret == -1) {
                            /* ignore and next */
                            close(conn_fd);
                            continue;
                        }
                        /* new connection_t */
                        conn = malloc(sizeof(struct connection_t));
                        if (conn == NULL) {
                            /* ignore and next */
                            close(conn_fd);
                            continue;
                        }
                        conn->fd = conn_fd;
                        conn->req_read = 0;
                        conn->req_checked = 0;
                        conn->checked_state = CHST_FIRSTWORD;
                        conn->entity = NULL;
                        conn->entity_dyn = 0;
                        conn->header_sent = 0;
                        conn->body_sent = 0;
                        /* add conn_fd to epoll array */
                        memset(&epollevt, 0, sizeof(epollevt));
                        epollevt.events = EPOLLIN;
                        epollevt.data.u64 = ((unsigned long long)(unsigned int)conn << 32)
                                            | conn_fd;
                        ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_fd, &epollevt);
                        if (ret == -1) {
                            free(conn);
                            close(conn_fd);
                            goto err;
                        }
                        /* attach this conn to list */
                        s->conn_list.prev->next = (struct dlist_t *)conn;
                        conn->list.prev = s->conn_list.prev;
                        s->conn_list.prev = (struct dlist_t *)conn;
                        conn->list.next = &s->conn_list;
                        /* update total_conns */
                        total_conns++;
                    }
                } else {
                    goto err;
                }
            } else {
                /* connection */
                conn = (struct connection_t *)((unsigned int)(outevtarr[i].data.u64 >> 32));
                if (outevtarr[i].events & EPOLLIN) {
                    /* read request */
                    while (1) {
                        if (conn->req_read == sizeof(conn->req_buf) - 1) {
                            /* Request Entity Too Large */
                            clear_connection(conn);
                            total_conns--;
                            break;
                        }
                        /* read */
                        ret = read(cur_fd, conn->req_buf + conn->req_read, 
                                   sizeof(conn->req_buf) - 1 - conn->req_read);
                        if (ret == -1) {
                            if (errno == EAGAIN) {
                                break;
                            } else {
                                /* error, clear connection */
                                clear_connection(conn);
                                total_conns--;
                                break;
                            }
                        } else if (ret == 0) {
                            /* Bad Request */
                            clear_connection(conn);
                            total_conns--;
                            break;
                        } else {
                            /* read some data */
                            conn->req_read += ret;
                            conn->req_buf[conn->req_read] = '\0';
                            /* got request header? */
                            ret = got_request(conn);
                            if (ret == GR_NO_REQUEST) {
                                continue;
                            } else if (ret == GR_BAD_REQUEST) {
                                /* Bad Request */
                                clear_connection(conn);
                                total_conns--;
                                break;
                            }
                            /* GR_GOT_REQUEST, parse it */
                            if (parse_request(conn, s) == -1) {
                                clear_connection(conn);
                                total_conns--;
                                break;
                            }
                            /* change to write mode */
                            memset(&epollevt, 0, sizeof(epollevt));
                            epollevt.events = EPOLLOUT;
                            epollevt.data.u64 = outevtarr[i].data.u64;
                            ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, cur_fd, &epollevt);
                            if (ret == -1) {
                                goto err;
                            }
                            break;
                        }
                    }
                } else if (outevtarr[i].events & EPOLLOUT) {
                    /* send response */
                    /* for header */
                    while (conn->header_sent < conn->entity->header_length) {
                        ret = write(cur_fd, conn->entity->header + conn->header_sent, 
                                    conn->entity->header_length - conn->header_sent);
                        if (ret < 0) {
                            if (errno == EAGAIN)
                                goto for_next;
                            /* error */
                            goto failed;
                        }
                        /* send some data */
                        conn->header_sent += ret;
                    }
                    /* for body */
                    if (conn->method == METHOD_GET) {
                        while (conn->body_sent < conn->entity->body_length) {
                            ret = write(cur_fd, conn->entity->body + conn->body_sent, 
                                        conn->entity->body_length - conn->body_sent);
                            if (ret < 0) {
                                if (errno == EAGAIN)
                                    goto for_next;
                                /* error */
                                goto failed;
                            }
                            /* send some data */
                            conn->body_sent += ret;
                        }
                    }
                    /* all sent */
                    shutdown(conn->fd, SHUT_WR);
                    /* completed */
failed:
                    clear_connection(conn);
                    total_conns--;
for_next:
                    continue;
                } else {
                    /* error, clear connection */
                    clear_connection(conn);
                    total_conns--;
                }
            }
        }
        /* sample */
        if (sample(s) == -1)
            goto err;
    }
err:
    /* clean all */
    webmon_dync_clean(s);
    /* close epollfd */
    if (epollfd != -1)
        close(epollfd);
    return -1;
}
