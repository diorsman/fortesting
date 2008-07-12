/**
 *  File: test.c
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
#include <webmon.h>

static const char **
textinfo_cb(void *arg)
{
    char **rets;

    rets = malloc(3 * sizeof(char *));
    if (rets == NULL)
        return NULL;
    *rets = strdup("A test string");
    *(rets + 1) = strdup("Another test string");
    *(rets + 2) = NULL;

    return (const char **)rets;
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
first_sample(void *arg, double *ret)
{
    *ret = (double)(rand() % 100);
    return 0;
}

static int
second_sample(void *arg, double *ret)
{
    *ret = (double)(rand() % 100);
    return 0;
}

struct webmon_graph_t test_graphs[] = {
    /* test graph */
    {"Test Graph", 2, 100, 0, 
     {"first line", "second line"}, 
     {first_sample, second_sample}, 
     {NULL, NULL}, 
    },
};

int
main(int argc, char *argv[])
{
    void *sd;

    /* init webmon */
    if (webmon_init() == -1) {
        printf("webmon_init() failed.\n");
        exit(1);
    }
    /* create an instance */
    sd = webmon_create("libwebmon test", "0.0.0.0", 80, 800, 1, 1, 1, 1);
    if (sd == NULL) {
        fprintf(stderr, "webmon_create() failed, did you launch test by root?\n");
        exit(1);
    }
    /* set textinfo callback */
    webmon_set_textinfo_callback(sd, textinfo_cb, NULL);
    webmon_set_free_textinfo_callback(sd, free_textinfo_cb);
    /* add a user defined graph */
    if (webmon_addgraph(sd, &test_graphs[0]) == -1) {
        fprintf(stderr, "webmon_addgraph() failed.\n");
        exit(1);
    }
    /* run */
    printf("test is listening on 0.0.0.0:80 now.\n");
    webmon_run(sd);

    return 0;
}
