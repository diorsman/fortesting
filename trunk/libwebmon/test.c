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

#include "webmon.h"

int
main(int argc, char *argv[])
{
    void *sd;

    if (webmon_init() == -1) {
        printf("webmon_init() failed.\n");
        exit(1);
    }

    sd = webmon_create("libwebmon test", "0.0.0.0", 80, 800, 1, 1, 1, 1);
    if (sd != NULL) {
        printf("test is listening on 0.0.0.0:80 now.\n");
        webmon_run(sd);
    } else {
        fprintf(stderr, "webmon_create() failed, did you launch test by root?\n");
    }
    return 0;
}
