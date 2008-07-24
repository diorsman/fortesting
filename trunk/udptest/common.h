/**
 *  File: common.h
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

#ifndef _COMMON_H_
#define _COMMON_H_

#define SERV_PORT_BEGIN     8000
#define SERV_PORT_RANGE     1000
#define PACKET_LENGTH_MAX   2048
#define ERR \
        do { \
            fprintf(stderr, \
                    "Exception from (%s:%d), errno = %d.\n", \
                    __FILE__, __LINE__, errno); \
        } while (0);

struct udpdata_t {
#define REQ         0
#define BIDIR_REQ   1
    char type;
    unsigned long seqno;
    char data[0];
};

int setlimits(int maxfd);
int setnoblock(int fd);

#endif  /* _COMMON_H_ */
