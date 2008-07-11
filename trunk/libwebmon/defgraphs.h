/**
 *  File: defgraphs.h
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

#ifndef _DEFGRAPHS_H_
#define _DEFGRAPHS_H_

#define CPU 0
#define MEM 1

#define NETIF_MAX   16

/**
 *  if you want to add eth* to page, you should set name_pattern 
 *  pointer to "eth".
 *
 *  "eth:vmnet" will add eth* and vmnet* to page.
 *
 *  name_pattern is case insensetive.
 */
int add_netif_graphs(void *s, const char *name_pattern);

#endif  /* _DEFGRAPHS_H_ */
