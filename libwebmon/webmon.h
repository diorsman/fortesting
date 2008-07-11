/**
 *  File: webmon.h
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

#ifndef _WEBMON_H_
#define _WEBMON_H_

#define LINE_COUNT_MAX  6

typedef int (*SAMPLE)(void *arg, double *ret);
struct webmon_graph_t {
    /* for entire graph */
    const char *desc;
    int line_count;
    double high_limit;
    double low_limit;
    /* for each lines */
    const char *line_labels[LINE_COUNT_MAX];
    SAMPLE samples[LINE_COUNT_MAX];
    void *args[LINE_COUNT_MAX];
};

int webmon_init(void);
void *webmon_create(const char *title, const char *ip, int port, int nsample, 
                    int interval, int cpu, int mem, int netif);
int webmon_addgraph(void *webmon, const struct webmon_graph_t *graph);
int webmon_run(void *webmon);

#endif  /* _WEBMON_H_ */
