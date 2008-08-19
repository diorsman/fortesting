/**
 *  File: httpd.c
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
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <webmon.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define SOFTWARE        "TestHttpd"
#define VERSION         "0.1.1"
#define LISTEN_BACKLOG  1024
#define REQ_MAX         8192
#define URL_MAX         4096
#define BLOCK_MAX       0x100000    /* 1Mbytes */
#define WAIT_BF_RESET   600         /* ms */
#define URL_DEF         "index.html"

#define CONNS_LIMIT_DEF 10000
#define ROOT_DIR_DEF    "/var/www/html"
#define LISTEN_PORT     80
#define WEBMON_PORT     8080

#define ERR(string) \
        do { \
            syslog(LOG_ERR, \
                    "Unexpected error occurred(%s:%d), errno = %d.\n" \
                    "  AUX-Message: %s.\n", \
                    __FILE__, __LINE__, errno, string); \
            errno = 0; \
        } while (0);

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

struct dlist_t {
    struct dlist_t *prev;
    struct dlist_t *next;
};

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

/* request entity header */
#define HDR_REFERER     0
#define HDR_USER_AGENT  1
#define HDR_NUM         2

struct http_hdr_t {
    const char *hdr;
    int hdr_len;
};  

struct connection_t {
    /* wait queue */
    struct dlist_t list;
    unsigned long long end_time; /* ms */
    /* connecttion data */
    int fd;
    struct sockaddr_in from;
    /* request */
    char req_buf[REQ_MAX];
    size_t req_read;
    size_t req_checked;
    int checked_state;
    /* http request compnoents */
#define METHOD_HEAD     0
#define METHOD_GET      1
    int method;
    char *orig_filename;
    char *req_hdrs[HDR_NUM];
    /* response */
    struct entity_t *entity;
    int entity_dyn;
    size_t header_sent;
    size_t body_sent;
    /* for optmize */
    int req_hdrs_got;
};

static struct entity_t *entity_arr = NULL;
static size_t entity_count, entity_alloced;
static int ignore_len;
static struct entity_t *error_arr = NULL;
static size_t error_count, error_alloced;
static int conns_limit = CONNS_LIMIT_DEF;
static char *root_dir = ROOT_DIR_DEF;
static int listen_port = LISTEN_PORT;
static int webmon_port = WEBMON_PORT;
static int enable_fake_page = 0;
static int enable_reset = 0;
static int enable_request_logging = 0;
static struct dlist_t wait_queue = {&wait_queue, &wait_queue};
static struct http_hdr_t http_req_hdrs[HDR_NUM] = {
    {"Referer",     7}, 
    {"User-Agent",  10},
};

/* for statistics */
static unsigned long long accepted = 0;
static int cur_conns = 0;

/* connection_t list management */
struct mem_node_t {
    void *next;
    struct connection_t c;
};
static struct mem_node_t *clist = NULL;

static int
init_conn_man(int max)
{
    struct mem_node_t *mn;

    /* alloc */
    clist = malloc(max * sizeof(struct mem_node_t));
    if (clist == NULL)
        return -1;
    /* make list */
    mn = clist;
    while (--max > 0) {
        mn->next = mn + 1;
        mn++;
    }
    mn->next = NULL;
    return 0;
}

static struct connection_t *
new_conn(void)
{
    struct mem_node_t *mn;

    if (clist == NULL) {
        ERR("null");
        exit(1);
    }
    /* detach from list */
    mn = clist;
    clist = clist->next;
    return &(mn->c);
}

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

static void
free_conn(struct connection_t *conn)
{
    struct mem_node_t *mn;

    mn = container_of(conn, struct mem_node_t, c);
    mn->next = clist;
    clist = mn;
}

static void 
show_help(void)
{
    printf("usage: testhttpd [-c(%d): allowed max connections]\n"
           "                 [-d(%s): www root directory]\n"
           "                 [-p(%d): TestHttpd's listen port]\n"
           "                 [-w(%d): WebMonitor's listen port]\n"
           "                 [-f: enable fake page]\n"
           "                 [-r: enable reset]\n"
           "                 [-l: enable request logging]\n",
           conns_limit, root_dir, listen_port, webmon_port);
}

#define ERROR_ALLOC_STEP    16
static void
error_install(int status, const char *text)
{
    int ret;
    char header[4096];

    if (error_count == error_alloced) {
        error_alloced += ERROR_ALLOC_STEP;
        error_arr = realloc(error_arr, 
                            sizeof(struct entity_t) * error_alloced);
        if (error_arr == NULL) {
            ERR("null");
            exit(1);
        }
    }
    /* status */
    error_arr[error_count].u.status = status;
    /* header */
    ret = snprintf(header, sizeof(header), 
                   "HTTP/1.1 %d %s\r\n"
                   "Server: %s/%s\r\n"
                   "Content-Length: 0\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n", 
                   status, text, SOFTWARE, VERSION);
    if (ret >= sizeof(header)) {
        ERR("null");
        exit(1);
    }
    error_arr[error_count].header_dyn = 0;
    error_arr[error_count].header_length = ret;
    error_arr[error_count].header = strdup(header);
    if (error_arr[error_count].header == NULL) {
        ERR("null");
        exit(1);
    }
    /* body */
    error_arr[error_count].body_dyn = 0;
    error_arr[error_count].body_length = 0;
    error_arr[error_count].body = NULL;
    error_count++;
}

static int
error_cmp(const void *a, const void *b)
{
    struct entity_t *ea, *eb;

    ea = (struct entity_t*)a;
    eb = (struct entity_t*)b;
    return (ea->u.status - eb->u.status);
}

static void
error_init(void)
{
    error_count = 0;
    error_alloced = ERROR_ALLOC_STEP;
    error_arr = malloc(sizeof(struct entity_t) * error_alloced);
    if (error_arr == NULL) {
        ERR("null");
        exit(1);
    }
    /* install */
    error_install(304, "Not Modified");
    error_install(400, "Bad Request");
    error_install(404, "Not Found");
    error_install(413, "Request Entity Too Large");
    error_install(414, "Request-URI Too Long");
    error_install(500, "Internal Server Error");
    error_install(501, "Not Implemented");
    /* sort */
    qsort(error_arr, error_count, sizeof(struct entity_t), error_cmp);
}

#define ENTITY_ALLOC_STEP   16
static void 
_get_file_list(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    int len, ret, fd;
    char file[PATH_MAX], header[4096];
    struct stat st;

    dir = opendir(path);
    if (dir == NULL) {
        syslog(LOG_ERR, 
               "Failed to open directory(%s), errno = %d.\n", 
               path, errno);
        exit(1);
    }

    len = strlen(path);
    /* for each file */
    while (1) {
        entry = readdir(dir);
        if (entry == NULL)
            break;
        /* ignore ./.. */
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0 )
        {
            continue;
        }
        /* file */
        if (path[len - 1] == '/')
            snprintf(file, PATH_MAX, "%s%s", path, entry->d_name);
        else
            snprintf(file, PATH_MAX, "%s/%s", path, entry->d_name);
        /* get more info */
        ret = stat(file, &st);
        if (ret == -1) {
            syslog(LOG_ERR, 
                   "Failed to stat file(%s), errno = %d.\n", 
                   file, errno);
            exit(1);
        }
        if (S_ISDIR(st.st_mode)) {
            /* dir */
            _get_file_list(file);
        } else if (S_ISREG(st.st_mode)) {
            /* normal file, including target of symbol link */
            if (entity_count == entity_alloced) {
                entity_alloced += ENTITY_ALLOC_STEP;
                entity_arr = realloc(entity_arr, 
                    sizeof(struct entity_t) * entity_alloced);
                if (entity_arr == NULL) {
                    ERR("null");
                    exit(1);
                }
            }
            /* name */
            entity_arr[entity_count].u.name = strdup(&file[ignore_len]);
            if (entity_arr[entity_count].u.name == NULL) {
                ERR("null");
                exit(1);
            }
            /* header */
            ret = snprintf(header, sizeof(header), 
                           "HTTP/1.1 200 OK\r\n"
                           "Server: %s/%s\r\n"
                           "Content-Length: %d\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Accept-Ranges: none\r\n"
                           "Connection: close\r\n"
                           "\r\n", 
                           SOFTWARE, VERSION, (int)st.st_size);
            if (ret >= sizeof(header)) {
                ERR("null");
                exit(1);
            }
            entity_arr[entity_count].header_dyn = 0;
            entity_arr[entity_count].header_length = ret;
            entity_arr[entity_count].header = strdup(header);
            if (entity_arr[entity_count].header == NULL) {
                ERR("null");
                exit(1);
            }
            /* body */
            entity_arr[entity_count].body_dyn = 0;
            entity_arr[entity_count].body_length = st.st_size;
            entity_arr[entity_count].body = malloc(st.st_size);
            if (entity_arr[entity_count].body == NULL) {
                ERR("null");
                exit(1);
            }
            fd = open(file, O_RDONLY);
            if (fd == -1) {
                syslog(LOG_ERR, 
                       "Failed to open file(%s), errno = %d.\n", 
                       file, errno);
                exit(1);
            }
            ret = read(fd, (void *)entity_arr[entity_count].body, 
                       st.st_size);
            if (ret != st.st_size) {
                ERR("null");
                exit(1);
            }
            close(fd);
            entity_count++;
        }
    }
    closedir(dir);
}

static int
entity_cmp(const void *a, const void *b)
{
    struct entity_t *ea, *eb;

    ea = (struct entity_t*)a;
    eb = (struct entity_t*)b;
    return strcmp(ea->u.name, eb->u.name);
}

static void
entity_init(const char *path)
{
    ignore_len = strlen(path);
    if (path[ignore_len - 1] != '/')
        ignore_len++;

    entity_count = 0;
    entity_alloced = ENTITY_ALLOC_STEP;
    entity_arr = malloc(sizeof(struct entity_t) * entity_alloced);
    if (entity_arr == NULL) {
        ERR("null");
        exit(1);
    }
    /* loop all */
    _get_file_list(path);
    /* sort */
    qsort(entity_arr, entity_count, sizeof(struct entity_t), entity_cmp);
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

#if 0
static int 
set_nodelay(int fd)
{
    int on, ret;

    /* no delay */
    on = 1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *)&on, 
                     sizeof(on));
    if (ret == -1) {
        return -1;
    }
    return 0;
}
#endif

static int
listen_init(void)
{
    int listenfd, ret, on;
    struct sockaddr_in localaddr;

    /* init listen port */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        ERR("null");
        exit(1);
    }
    /* set noblock */
    ret = set_noblock(listenfd);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
    /* SO_REUSEADDR */
    on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
               (const void *)&on, sizeof(on));
    /* bind */
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    localaddr.sin_port = htons(listen_port);
    ret = bind(listenfd, (struct sockaddr *)&localaddr, sizeof(localaddr));
    if (ret == -1) {
        syslog(LOG_ERR, 
               "Failed to bind socket on (0.0.0.0: %d), errno = %d.\n",
               listen_port, errno);
        exit(1);
    }
    /* listen */
    ret = listen(listenfd, LISTEN_BACKLOG);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }

    return listenfd;
}

/* for webmon */
static time_t begin_time;

static const char **
textinfo_cb(void *arg)
{
    static char str1[1024];
    static char *rets[2] = {str1, NULL};

    /* running time */
    snprintf(str1, sizeof(str1),
             "I have been running for %u seconds and have accepted %llu connections.",
             (int)(time(NULL) - begin_time), accepted);
    return (const char **)rets;
}

static int
accepted_sample(void *arg, double *ret)
{
    int interval/* interval in ms */;
    struct timeval tv;

    static int inited = 0;
    static unsigned long long last_count;
    static struct timeval last_sample_time;

    /* compute */
    if (!inited) {
        *ret = 0.0;
        /* for later sample */
        last_count = accepted;
        gettimeofday(&last_sample_time, NULL);
        /* inited */
        inited = 1;
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        if (interval <= 0)
            *ret = 0.0;
        else
            *ret = ((double)(accepted - last_count) * 1000) / interval;
        /* for next sample */
        last_count = accepted;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}

static int
conns_sample(void *arg, double *ret)
{
    *ret = (double)cur_conns;
    return 0;
}

struct webmon_graph_t my_graphs[] = {
    /* for accepted */
    {"每秒接受新连接", 1, 10000, 0,
     {"n/second"},
     {accepted_sample},
     {NULL},
    },
    /* for cur_conns */
    {"并发连接数", 1, 10000, 0,
     {"n"},
     {conns_sample},
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

static void 
send_error(struct connection_t *c, int status)
{
    struct entity_t s, *rets;
    
    s.u.status = status;
    rets = (struct entity_t*)bsearch(&s, error_arr, error_count, 
                                     sizeof(struct entity_t), error_cmp);
    if (rets == NULL) {
        ERR("null");
        exit(1);
    }
    c->entity = rets;
}

static void 
reset(int fd)
{
    /* Set lingering on a socket if needed */
    struct linger l;

    l.l_onoff = 1; 
    l.l_linger = 0; 
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&l, sizeof(l));
}

static void
clear_connection(struct connection_t *c)
{
    /* reset connection? */
    if (enable_reset)
        reset(c->fd);
    close(c->fd);
    /* free */
    if (c->entity_dyn) {
        if (c->entity->header_dyn)
            free(c->entity->header);
        if (c->entity->body_dyn)
            free(c->entity->body);
        free(c->entity);
    }
    //free(c);
    free_conn(c);
    cur_conns--;
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
    while ((l = strlen(file)) > 3 
           && strcmp((cp = file + l - 3), "/..") == 0) 
    {
        for (cp2 = cp - 1; cp2 >= file && *cp2 != '/'; --cp2)
            continue;
        if (cp2 < file)
            break;
        *cp2 = '\0';
    }
}

static struct entity_t *
make_blockdata(int size)
{
    struct entity_t *rets;
    char hbuf[1024];
    int ret;

    static int block_inited = 0;
    static char block_buf[BLOCK_MAX];

    if (!block_inited) {
        memset(block_buf, '.', sizeof(block_buf));
        block_inited = 1;
    }
    /* check size */
    if (size > sizeof(block_buf))
        return NULL;
    /* entity_t and body */
    rets = malloc(sizeof(struct entity_t));
    if (rets == NULL)
        return NULL;
    /* header */
    ret = snprintf(hbuf, sizeof(hbuf), 
                   "HTTP/1.1 200 OK\r\n"
                   "Server: %s/%s\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Length: %d\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Accept-Ranges: none\r\n"
                   "Connection: close\r\n"
                   "\r\n", 
                   SOFTWARE, VERSION, size);
    /* let all data as HTTP header */
    rets->header = strdup(hbuf);
    if (rets->header == NULL) {
        free(rets);
        return NULL;
    }
    rets->header_length = ret;
    rets->header_dyn = 1;
    rets->body = block_buf;
    rets->body_length = size;
    rets->body_dyn = 0;

    return rets;
}

static struct entity_t *
entity_get(const char *path)
{
    struct entity_t s, *rets;
    
    if (*path == '\0')
        s.u.name = URL_DEF;
    else
        s.u.name = (char*)path;
    rets = (struct entity_t*)bsearch(&s, entity_arr, entity_count, 
                                     sizeof(struct entity_t), entity_cmp);
    return rets;
}

static int 
parse_request(struct connection_t *c)
{
    char *p, *q, *r, *s;
    int size, i;

    p = c->req_buf;
    /* get http method */
    p += strspn(p, " \t");
    q = strpbrk(p, " \t");
    if (q == NULL) {
        /* Bad Request */
        send_error(c, 400);
        return -1;
    }
    if (q - p == 4 && p[0] == 'H' && p[1] == 'E' 
        && p[2] == 'A' && p[3] == 'D') 
    {
        /* HEAD */
        c->method = METHOD_HEAD;
    } else if (q - p == 3 && p[0] == 'G' && p[1] == 'E' && p[2] == 'T') {
        /* GET */
        c->method = METHOD_GET;
    } else {
        /* Not Implemented */
        send_error(c, 501);
        return -1;
    }
    /* get dest url */
    p = q;
    p += strspn(p, " \t");
    q = strpbrk(p, " \t");
    if (q == NULL) {
        /* Bad Request */
        send_error(c, 400);
        return -1;
    }
    *q = '\0';
    r = q + 1;
    /* check url length */
    if (q - p >= URL_MAX) {
        /* Request-URI Too Long */
        send_error(c, 414);
        return -1;
    }
    /* absolute url? */
    if (*p != '/') {
        /* Bad Request */
        send_error(c, 400);
        return -1;
    }
    /* remove query part */
    q = strchr(p, '?');
    if (q != NULL)
        *q = '\0';
    /* url decode */
    url_decode(p, p);
    /* remove leading '/' */
    c->orig_filename = p + 1;
    /* for security */
    de_dotdot(c->orig_filename);
    if (c->orig_filename[0] == '/'
        || (c->orig_filename[0] == '.' && c->orig_filename[1] == '.' 
            && (c->orig_filename[2] == '\0' || c->orig_filename[2] == '/')))
    {
        /* Bad Request */
        send_error(c, 400);
        return -1;
    }

    /* for request headers */
    p = strchr(r, '\n');
    if (p == NULL) {
        /* Bad Request */
        send_error(c, 400);
        return -1;
    }
    p++;
    /* p: new line */
    while (1) {
        p += strspn(p, " \t");
        /* get line */
        r = strchr(p, '\n');
        if (r == NULL) {
            /* Bad Request */
            send_error(c, 400);
            return -1;
        } else if (p + strspn(p, "\r") == r) {
            /* request end */
            break;
        }
        *r = '\0';
        /* for line */
        q = strchr(p, ':');
        if (q != NULL) {
            *q++ = '\0';
            /* have ':', parse it */
            for (i = 0; i < HDR_NUM; i++) {
                if (strncasecmp(http_req_hdrs[i].hdr, p, 
                                http_req_hdrs[i].hdr_len) == 0) 
                {
                    /* match */
                    q += strspn(q, " \t");
                    /* discard '\r' if exist */
                    s = strchr(q, '\r');
                    if (s != NULL)
                        *s = '\0';
                    if (c->req_hdrs[i] == NULL)
                        c->req_hdrs_got++;
                    c->req_hdrs[i] = q;
                    /* get all? */
                    if (c->req_hdrs_got == HDR_NUM)
                        goto got_all_hdrs;
                    break;
                }
            }
        }
        /* for next */
        p = r + 1;
    }

got_all_hdrs:
    /* request memory block or stat page? */
    if (enable_fake_page) {
        if (strncmp(c->orig_filename, "local/block/", 12) == 0) {
            size = atoi(c->orig_filename + 12);
            if (size <= 0 || size > BLOCK_MAX) {
                send_error(c, 400);
                return -1;
            }
            c->entity = make_blockdata(size);
            if (c->entity == NULL) {
                send_error(c, 500);
                return -1;
            } else {
                /* need free */
                c->entity_dyn = 1;
                return 0;
            }
        }
    }
    /* normal file */
    c->entity = entity_get(c->orig_filename);
    if (c->entity == NULL) {
        /* Not Found */
        send_error(c, 404);
        return -1;
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    int ret, max_fd, listenfd, epollfd, nfd, i, cur_fd, conn_fd;
    struct rlimit rlmt;
    struct epoll_event epollevt, *outevtarr;
    struct connection_t *conn;
    void *sd;
    pthread_t ptid;
    struct timeval tv;
    unsigned long long cur_time;
    struct dlist_t *dl;
    struct sockaddr_in from;
    socklen_t from_len;

    /* parse arguments line */
    while (1) {
        ret = getopt(argc, argv, "c:d:p:w:frl");
        if (ret == -1) {
            if (argv[optind] != NULL) {
                show_help();
                exit(1);
            }
            break;
        }
        switch (ret) {
        case 'c':
            conns_limit = atoi(optarg);
            if (conns_limit < 1 || conns_limit > 60000) {
                fprintf(stderr, 
                        "The allowed max connection number should be in [1, 60000].\n");
                exit(1);
            }
            break;
        case 'd':
            if (optarg[0] != '/') {
                fprintf(stderr, 
                        "The www root directory should be an absolute path.\n");
                exit(1);
            }
            root_dir = strdup(optarg);
            if (root_dir == NULL) {
                fprintf(stderr, "Out of memory.\n");
                exit(1);
            }
            break;
        case 'p':
            listen_port = atoi(optarg);
            if (listen_port < 1 || listen_port > 65535) {
                fprintf(stderr, 
                        "TestHttpd's listen port should be in [1, 65535].\n");
                exit(1);
            }
            break;
        case 'w':
            webmon_port = atoi(optarg);
            if (webmon_port < 1 || webmon_port > 65535) {
                fprintf(stderr, 
                        "webmon's listen port should be in [1, 65535].\n");
                exit(1);
            }
            if (webmon_port == listen_port) {
                fprintf(stderr, 
                        "webmon's listen port must be different with TestHttpd's listen port.\n");
                exit(1);
            }
            break;
        case 'f':
            enable_fake_page = 1;
            break;
        case 'r':
            enable_reset = 1;
            break;
        case 'l':
            enable_request_logging = 1;
            break;
        default:
            show_help();
            exit(1);
        }
    }

    /* daemonrize */
    openlog(SOFTWARE, LOG_NDELAY | LOG_PID, LOG_DAEMON);
    daemon(0, 0);

    /* set max fd */
    max_fd = conns_limit + 10;
    ret = getrlimit(RLIMIT_NOFILE, &rlmt);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
    if (max_fd > rlmt.rlim_cur) {
        rlmt.rlim_cur = max_fd;
        rlmt.rlim_max = max_fd;
        ret = setrlimit(RLIMIT_NOFILE, &rlmt);
        if (ret == -1) {
            if (errno == EPERM)
                syslog(LOG_ERR, 
                       "Failed to change resource limit, operation disallowed.\n");
            else
                ERR("null");
            exit(1);
        }
    }

    /* signal */
    signal(SIGPIPE, SIG_IGN);
    /* initialize error cache */
    error_init();
    /* initialize entity cache */
    entity_init(root_dir);
    /* init listen socket */
    listenfd = listen_init();
    /* init epoll */
    epollfd = epoll_create(max_fd);
    if (epollfd == -1) { 
        ERR("null");
        exit(1);
    }
    /* alloc outevtarr */
    outevtarr = malloc(max_fd * sizeof(struct epoll_event));
    if (outevtarr == NULL) {
        ERR("null");
        exit(1);
    }
    /* add listenfd to epoll array */
    memset(&epollevt, 0, sizeof(epollevt));
    epollevt.events = EPOLLIN | EPOLLET;
    epollevt.data.fd = listenfd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &epollevt);
    if (ret == -1) {
        ERR("null");
        exit(1);
    }
    /* conn list init */
    if (init_conn_man(conns_limit) == -1) {
        ERR("null");
        exit(1);
    }

    /* init and create webmon */
    if (webmon_init() == -1) {
        ERR("null");
        exit(1);
    }
    sd = webmon_create(SOFTWARE, "0.0.0.0", webmon_port, 
                       900, 4, 1, 1, 1);
    if (sd == NULL) {
        ERR("null");
        exit(1);
    }
    /* set textinfo callback */
    webmon_set_textinfo_callback(sd, textinfo_cb, NULL);
    /* udpate uplimit for max connection */
    my_graphs[1].high_limit = conns_limit;
    /* add graphs */
    for (i = 0; i < sizeof(my_graphs) / sizeof(struct webmon_graph_t); 
         i++) 
    {
        if (webmon_addgraph(sd, &my_graphs[i]) == -1) {
            ERR("null");
            exit(1);
        }
    }
    /* start new thread */
    if (pthread_create(&ptid, NULL, wrap_webmon_run, sd) != 0) {
        ERR("null");
        exit(1);
    }

    syslog(LOG_INFO, 
           "Now TestHttpd is listening on (0.0.0.0:%d).\n", 
           listen_port);

    /* main loop */
    while (1) {
        nfd = epoll_wait(epollfd, outevtarr, max_fd, WAIT_BF_RESET);
        if (nfd == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            /* other error */
            ERR("null");
            exit(1);
        }

        /* have some events */
        for (i = 0; i < nfd; i++) {
            cur_fd = outevtarr[i].data.fd;
            /* for listenfd */
            if (cur_fd == listenfd) {
                if (outevtarr[i].events & EPOLLIN) {
                    /* new requests arrived */
                    while (1) {
                        if (cur_conns == conns_limit) {
                            /* reach connections limit, ignore */
                            break;
                        }
                        /* accept */
                        from_len = sizeof(from);
                        conn_fd = accept(listenfd, 
                                         (struct sockaddr *)&from, 
                                         &from_len);
                        if (conn_fd == -1) {
                            /* accept error, ignore request */
                            if (errno != EAGAIN) {
                                ERR("null");
                                errno = 0;
                            }
                            /* dont call accept again */
                            break;
                        }
                        /* accepted */
                        accepted++;
                        /* no-blocking */
                        ret = set_noblock(conn_fd);
                        if (ret == -1) {
                            ERR("null");
                            /* ignore and next */
                            close(conn_fd);
                            continue;
                        }
#if 0
                        /* no-delay */
                        ret = set_nodelay(conn_fd);
                        if (ret == -1) {
                            ERR("null");
                            /* ignore and next */
                            close(conn_fd);
                            continue;
                        }
#endif
                        /* new connection_t */
                        //conn = calloc(1, sizeof(struct connection_t));
                        //conn = malloc(sizeof(struct connection_t));
                        conn = new_conn();
#if 0
                        if (conn == NULL) {
                            ERR("null");
                            /* ignore and next */
                            close(conn_fd);
                            continue;
                        }
#endif
                        /* init */
                        conn->fd = conn_fd;
                        memcpy(&conn->from, &from, sizeof(from));
                        conn->req_read = 0;
                        conn->req_checked = 0;
                        conn->checked_state = CHST_FIRSTWORD;
                        conn->orig_filename = NULL;
                        memset(conn->req_hdrs, 0, sizeof(conn->req_hdrs));
                        conn->entity = NULL;
                        conn->entity_dyn = 0;
                        conn->header_sent = 0;
                        conn->body_sent = 0;
                        conn->req_hdrs_got = 0;
                        /* add conn_fd to epoll array */
                        memset(&epollevt, 0, sizeof(epollevt));
                        epollevt.events = EPOLLIN | EPOLLET;
                        epollevt.data.u64 = ((unsigned long long)(unsigned int)conn << 32)
                                            | conn_fd;
                        ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_fd, 
                                        &epollevt);
                        if (ret == -1) {
                            ERR("null");
                            exit(1);
                        }
                        /* update cur_conns */
                        cur_conns++;
                    }
                } else {
                    /* error */
                    ERR("null");
                    exit(1);
                }
            } else {
                /* connection */
                conn = (struct connection_t *)((unsigned int)(outevtarr[i].data.u64 >> 32));
                if (outevtarr[i].events & EPOLLIN) {
                    /* read request */
                    while (1) {
                        if (conn->req_read == sizeof(conn->req_buf) - 1) {
                            /* Request Entity Too Large */
                            send_error(conn, 413);
                            /* change to write mode */
                            memset(&epollevt, 0, sizeof(epollevt));
                            epollevt.events = EPOLLOUT | EPOLLET;
                            epollevt.data.u64 = outevtarr[i].data.u64;
                            ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, 
                                            cur_fd, &epollevt);
                            if (ret == -1) {
                                ERR("null");
                                exit(1);
                            }
                            break;
                        }
                        /* read */
                        ret = read(cur_fd, conn->req_buf + conn->req_read, 
                                   sizeof(conn->req_buf) - 1 - conn->req_read);
                        if (ret == -1) {
                            if (errno == EAGAIN) {
                                errno = 0;
                                break;
                            } else {
                                /* error, clear connection */
                                ERR("null");
                                clear_connection(conn);
                                break;
                            }
                        } else if (ret == 0) {
                            /* Bad Request */
                            send_error(conn, 400);
                            /* change to write mode */
                            memset(&epollevt, 0, sizeof(epollevt));
                            epollevt.events = EPOLLOUT | EPOLLET;
                            epollevt.data.u64 = outevtarr[i].data.u64;
                            ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, 
                                            cur_fd, &epollevt);
                            if (ret == -1) {
                                ERR("null");
                                exit(1);
                            }
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
                                send_error(conn, 400);
                                /* change to write mode */
                                memset(&epollevt, 0, sizeof(epollevt));
                                epollevt.events = EPOLLOUT | EPOLLET;
                                epollevt.data.u64 = outevtarr[i].data.u64;
                                ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, 
                                                cur_fd, &epollevt);
                                if (ret == -1) {
                                    ERR("null");
                                    exit(1);
                                }
                                break;
                            }
                            /* GR_GOT_REQUEST, parse it */
                            parse_request(conn);
                            /* change to write mode */
                            memset(&epollevt, 0, sizeof(epollevt));
                            epollevt.events = EPOLLOUT | EPOLLET;
                            epollevt.data.u64 = outevtarr[i].data.u64;
                            ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, 
                                            cur_fd, &epollevt);
                            if (ret == -1) {
                                ERR("null");
                                exit(1);
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
                            if (errno == EAGAIN) {
                                errno = 0;
                                goto for_next;
                            }
                            /* error */
                            ERR("null");
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
                                if (errno == EAGAIN) {
                                    errno = 0;
                                    goto for_next;
                                }
                                /* error */
                                ERR("null");
                                goto failed;
                            }
                            /* send some data */
                            conn->body_sent += ret;
                        }
                    }
                    /* log */
                    if (enable_request_logging && conn->orig_filename != NULL) {
                        syslog(LOG_INFO, 
                               "%s /%s %d [from: %s] [user_agent: %s], [referer: %s].\n", 
                               (conn->method == METHOD_GET) ? "GET" : "HEAD",
                               conn->orig_filename,
                               ((unsigned int)conn->entity->u.status < 1000) ? conn->entity->u.status : 200, 
                               inet_ntoa(conn->from.sin_addr),
                               (conn->req_hdrs[HDR_USER_AGENT] != NULL) ? conn->req_hdrs[HDR_USER_AGENT] : "Unknown", 
                               (conn->req_hdrs[HDR_REFERER] != NULL) ? conn->req_hdrs[HDR_REFERER] : "Unknown");
                    }
                    /* all sent */
                    if (enable_reset) {
                        /* delete */
                        /* epollevt for kernel-2.6.9, see epoll bugs */
                        memset(&epollevt, 0, sizeof(epollevt));
                        ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, 
                                        cur_fd, &epollevt);
                        if (ret == -1) {
                            ERR("null");
                            exit(1);
                        }
                        /* append to wait_queue */
                        gettimeofday(&tv, NULL);
                        conn->end_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                        wait_queue.prev->next = (struct dlist_t *)conn;
                        conn->list.prev = wait_queue.prev;
                        wait_queue.prev = (struct dlist_t *)conn;
                        conn->list.next = &wait_queue;
                    } else {
                        /* close immediately */
                        clear_connection(conn);
                    }
for_next:
                    continue;
failed:
                    clear_connection(conn);
                } else {
                    /* error, clear connection */
                    ERR("null");
                    clear_connection(conn);
                }
            }
        }

        if (enable_reset) {
            /* for wait_queue */
            gettimeofday(&tv, NULL);
            cur_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            /* for each entry */
            while ((dl = wait_queue.next) != &wait_queue) {
                /* expire? */
                if (cur_time - ((struct connection_t *)dl)->end_time
                    >= WAIT_BF_RESET)
                {
                    /* detach from wait_queue */
                    dl->prev->next = dl->next;
                    dl->next->prev = dl->prev;
                    /* do really close */
                    clear_connection((struct connection_t *)dl);
                } else {
                    break;
                }
            }
        }
    }

    return 0;
}
