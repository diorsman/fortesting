/**
 *  File: defgraphs.c
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
#include <sys/time.h>
#include <asm/param.h>

#include "webmon.h"
#include "defgraphs.h"

/* sample functions */
/* all for kernel-2.6 */
/* all these functions are thread-unsafe */
static int ncpu = -1;
static int
get_cpu_number(void)
{
    FILE *fp;
    int n = 0;
    char *p, linebuf[1024];
    
    /* read stat information */
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return -1;
    }
    /* "cpu" */
    p = fgets(linebuf, sizeof(linebuf), fp);
    if (p == NULL || strncasecmp(p, "cpu", 3) != 0) {
        fclose(fp);
        return -1;
    }
    /* cpu0 */
    p = fgets(linebuf, sizeof(linebuf), fp);
    if (p == NULL || strncasecmp(p, "cpu", 3) != 0) {
        fclose(fp);
        return -1;
    }
    n++;
    /* cpu1... */
    while ((p = fgets(linebuf, sizeof(linebuf), fp)) != NULL) {
        if (strncasecmp(p, "cpu", 3) == 0)
            n++;
        else
            break;
    }
    /* close */
    fclose(fp);

    return n;
}

static int
cpu_total_sample(void *arg, double *ret)
{
    FILE *fp;
    int n, idle, interval/* interval in ms */;
    char *tok, *saveptr, linebuf[1024];
    struct timeval tv;

    static int last_idle = -1;
    static struct timeval last_sample_time;

    /* get cpu count first */
    if (ncpu == -1) {
        ncpu = get_cpu_number();
        if (ncpu == -1) {
            return -1;
        }
    }
    
    /* read stat information */
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return -1;
    }
    /* "cpu" */
    fgets(linebuf, sizeof(linebuf), fp);
    n = 0;
    tok = strtok_r(linebuf, " \t", &saveptr);
    while (tok != NULL) {
        if (n == 4) {
            idle = atoi(tok);
            break;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        n++;
    }
    /* close */
    fclose(fp);
    if (n != 4) {
        return -1;
    }
    /* compute */
    if (last_idle == -1) {
        *ret = 0.0;
        /* for later sample */
        last_idle = idle;
        gettimeofday(&last_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        *ret = (double)(100 - (100 * (idle - last_idle) * 1000 / HZ / ncpu / interval));
        /* for later sample */
        last_idle = idle;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}

static int
cpu_process_sample(void *arg, double *ret)
{
    FILE *fp;
    int n, used, interval/* interval in ms */;
    char *tok, *saveptr, path[1024], linebuf[1024];
    struct timeval tv;

    static int last_used = -1;
    static struct timeval last_sample_time;

    /* get cpu count first */
    if (ncpu == -1) {
        ncpu = get_cpu_number();
        if (ncpu == -1) {
            return -1;
        }
    }

    /* read pid's stat information */
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)getpid());
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    /* get stat line */
    fgets(linebuf, sizeof(linebuf), fp);
    n = 0;
    tok = strtok_r(linebuf, " \t", &saveptr);
    while (tok != NULL) {
        if (n == 13) {
            used = atoi(tok);
        } else if (n == 14) {
            used += atoi(tok);
            break;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
        n++;
    }
    /* close */
    fclose(fp);
    if (n != 14) {
        return -1;
    }
    /* compute */
    if (last_used == -1) {
        *ret = 0.0;
        /* for later sample */
        last_used = used;
        gettimeofday(&last_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - last_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - last_sample_time.tv_usec) / 1000;
        *ret = (double)(100 * (used - last_used) * 1000 / HZ / ncpu / interval);
        /* for later sample */
        last_used = used;
        last_sample_time = tv;
    }
    /* ok */
    return 0;
}

static int mem_inited = 0;
static double nmem; /* mbytes */
static int page_size = 0;
static int
mem_init(void)
{
    FILE *fp;
    char *p, linebuf[1024];

    /* read memory information */
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1;
    }
    /* "MemTotal" */
    fgets(linebuf, sizeof(linebuf), fp);
    p = linebuf;
    p += strspn(p, " \t");
    p = strpbrk(p, " \t");
    if (!p) {
        fclose(fp);
        return -1;
    }
    p += strspn(p, " \t");
    nmem = ((double)atoi(p)) / 1024;
    /* ok */
    fclose(fp);

    /* get page_size */
    page_size = sysconf(_SC_PAGESIZE);

    return 0;
}

static int
mem_used_sample(void *arg, double *ret)
{
    FILE *fp;
    char *p, linebuf[1024];

    /* get total memory */
    if (!mem_inited) {
        if (mem_init() == -1)
            return -1;
        mem_inited = 1;
    }

    /* read memory information */
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1;
    }
    /* "MemTotal" */
    fgets(linebuf, sizeof(linebuf), fp);
    /* "MemFree" */
    fgets(linebuf, sizeof(linebuf), fp);
    p = linebuf;
    p += strspn(p, " \t");
    p = strpbrk(p, " \t");
    if (!p) {
        fclose(fp);
        return -1;
    }
    p += strspn(p, " \t");
    *ret = (nmem - ((double)atoi(p)) / 1024) * 100 / nmem;
    /* ok */
    fclose(fp);
    return 0;
}

static int
mem_cached_sample(void *arg, double *ret)
{
    FILE *fp;
    char *p, linebuf[1024];

    /* get total memory */
    if (!mem_inited) {
        if (mem_init() == -1)
            return -1;
        mem_inited = 1;
    }

    /* read memory information */
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1;
    }
    /* "MemTotal" */
    fgets(linebuf, sizeof(linebuf), fp);
    /* "MemFree" */
    fgets(linebuf, sizeof(linebuf), fp);
    /* "Buffers" */
    fgets(linebuf, sizeof(linebuf), fp);
    /* "Cached" */
    fgets(linebuf, sizeof(linebuf), fp);
    p = linebuf;
    p += strspn(p, " \t");
    p = strpbrk(p, " \t");
    if (!p) {
        fclose(fp);
        return -1;
    }
    p += strspn(p, " \t");
    *ret = ((double)atoi(p)) / 1024 * 100 / nmem;
    /* ok */
    fclose(fp);
    return 0;
}

static int
mem_process_sample(void *arg, double *ret)
{
    FILE *fp;
    char *p, path[1024], linebuf[1024];

    /* get process memory information */
    snprintf(path, sizeof(path), "/proc/%d/statm", (int)getpid());
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    /* the first line contains "rss" at col 2 */
    fgets(linebuf, sizeof(linebuf), fp);
    p = linebuf;
    p += strspn(p, " \t");
    p = strpbrk(p, " \t");
    if (!p) {
        fclose(fp);
        return -1;
    }
    p += strspn(p, " \t");
    *ret = ((double)atoi(p) * page_size) / 1024 / 1024 * 100 / nmem;
    /* ok */
    fclose(fp);
    return 0;
}

struct webmon_graph_t def_graphs[] = {
    /* for cpu */
    {"CPU Usage", 2, 100, 0, 
     {"Total used(%)", "Used by myself(%)"}, 
     {cpu_total_sample, cpu_process_sample}, 
     {NULL, NULL}, 
    },
    /* for mem */
    {"Memory Usage", 3, 100, 0, 
     {"Total used(%)", "Cache used(%)", "Used by myself(%)"}, 
     {mem_used_sample, mem_cached_sample, mem_process_sample}, 
     {NULL, NULL, NULL}, 
    },
};

/* add network device dynamically */
struct netif_t {
    char name[128];
    long long last_trans;
    struct timeval last_trans_sample_time;
    long long last_recv;
    struct timeval last_recv_sample_time;
};

static int
netif_trans_sample(void *arg, double *ret)
{
    char *p, *tok, *saveptr, linebuf[1024];
    FILE *fp;
    int n = 0, interval;
    long long trans;
    struct timeval tv;
    struct netif_t *ni = (struct netif_t *)arg;
    
    /* read stat information */
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return -1;
    }
    /* parse */
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        p = linebuf;
        p += strspn(p, " \t");
        if (strncmp(p, ni->name, strlen(ni->name)) == 0) {
            n = 0;
            tok = strtok_r(linebuf, " \t:", &saveptr);
            while (tok != NULL) {
                if (n == 9) {
                    trans = atoll(tok);
                    break;
                }
                tok = strtok_r(NULL, " \t:", &saveptr);
                n++;
            }
        }
    }
    /* close */
    fclose(fp);
    if (n != 9) {
        return -1;
    }
    /* compute */
    if (ni->last_trans == -1) {
        *ret = 0.0;
        /* for later sample */
        ni->last_trans = trans;
        gettimeofday(&ni->last_trans_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - ni->last_trans_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - ni->last_trans_sample_time.tv_usec) / 1000;
        /* mbytes/s */
        *ret = ((double)(trans - ni->last_trans) * 1000) / interval / 1024 / 1024;
        /* for later sample */
        ni->last_trans = trans;
        ni->last_trans_sample_time= tv;
    }
    /* ok */
    return 0;
}

static int
netif_recv_sample(void *arg, double *ret)
{
    char *p, *tok, *saveptr, linebuf[1024];
    FILE *fp;
    int n = 0, interval;
    long long recv;
    struct timeval tv;
    struct netif_t *ni = (struct netif_t *)arg;
    
    /* read stat information */
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return -1;
    }
    /* parse */
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        p = linebuf;
        p += strspn(p, " \t");
        if (strncmp(p, ni->name, strlen(ni->name)) == 0) {
            n = 0;
            tok = strtok_r(linebuf, " \t:", &saveptr);
            while (tok != NULL) {
                if (n == 1) {
                    recv = atoll(tok);
                    break;
                }
                tok = strtok_r(NULL, " \t:", &saveptr);
                n++;
            }
        }
    }
    /* close */
    fclose(fp);
    if (n != 1) {
        return -1;
    }
    /* compute */
    if (ni->last_recv == -1) {
        *ret = 0.0;
        /* for later sample */
        ni->last_recv = recv;
        gettimeofday(&ni->last_recv_sample_time, NULL);
    } else {
        /* compute */
        gettimeofday(&tv, NULL);
        interval = (tv.tv_sec - ni->last_recv_sample_time.tv_sec) * 1000 
                   + (tv.tv_usec - ni->last_recv_sample_time.tv_usec) / 1000;
        /* mbytes/s */
        *ret = ((double)(recv - ni->last_recv) * 1000) / interval / 1024 / 1024;
        /* for later sample */
        ni->last_recv = recv;
        ni->last_recv_sample_time= tv;
    }
    /* ok */
    return 0;
}

static int
in_pattern(const char *s, const char *pattern)
{
    const char *p, *q;
    int len;

    if (pattern == NULL || strlen(pattern) == 0)
        return 1;

    p = pattern;
    while (1) {
        /* get a pattern */
        q = strchr(p, ':');
        if (q != NULL)
            len = q - p;
        else
            len = strlen(p);
        /* match? */
        if (strncasecmp(s, p, len) == 0)
            return 1;
        /* for next pattern */
        if (q == NULL)
            break;
        p = q + 1;
    }
    return 0;
}

/**
 *  if you want to add eth* to page, you should set name_pattern 
 *  pointer to "eth".
 *
 *  "eth:vmnet" will add eth* and vmnet* to page.
 *
 *  name_pattern is case insensetive.
 */
int
add_netif_graphs(void *s, const char *name_pattern)
{
    char *p, *q, linebuf[1024], netif_name[128];
    FILE *fp;
    int i;
    struct netif_t *ni;

    static struct webmon_graph_t netif_graphs[NETIF_MAX];
    
    memset(netif_graphs, 0, sizeof(netif_graphs));
    /* read stat information */
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return -1;
    }
    /* parse */
    /* ignore Inter- ... */
    fgets(linebuf, sizeof(linebuf), fp);
    /* ignore face ... */
    fgets(linebuf, sizeof(linebuf), fp);
    /* for netif */
    i = 0;
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        /* get each netif_name */
        p = linebuf;
        p += strspn(p, " \t");
        q = strpbrk(p, " \t:");
        if (q - p >= sizeof(netif_name)) {
            /* netif's name is too long, ignore it */
            continue;
        }
        memcpy(netif_name, p, q - p);
        netif_name[q - p] = '\0';
        /* should be add to page? */
        if (!in_pattern(netif_name, name_pattern))
            continue;
        /* init graph struct */
        snprintf(linebuf, sizeof(linebuf), "Network Usage(%s)", netif_name);
        netif_graphs[i].desc = strdup(linebuf);
        if (netif_graphs[i].desc == NULL)
            goto err;
        netif_graphs[i].line_count = 2;
        netif_graphs[i].high_limit = 15;
        netif_graphs[i].low_limit = 0;
        netif_graphs[i].line_labels[0] = "Trans speed(MBytes/S)";
        netif_graphs[i].line_labels[1] = "Recv speed(MBytes/S)";
        netif_graphs[i].samples[0] = netif_trans_sample;
        netif_graphs[i].samples[1] = netif_recv_sample;
        ni = (struct netif_t *)malloc(sizeof(struct netif_t));
        if (ni == NULL)
            goto err;
        strcpy(ni->name, netif_name);
        ni->last_trans = -1;
        ni->last_recv = -1;
        netif_graphs[i].args[0] = ni;
        netif_graphs[i].args[1] = ni;
        /* add graph */
        if (webmon_addgraph(s, &netif_graphs[i]) == -1)
            goto err;
        i++;
    }
    /* close */
    fclose(fp);
    /* ok */
    return 0;

err:
    fclose(fp);
    for (; i >= 0; i--) {
        if (netif_graphs[i].desc != NULL)
            free((void *)netif_graphs[i].desc);
        if (netif_graphs[i].args[0] != NULL)
            free(netif_graphs[i].args[0]);
    }
    return -1;
}
