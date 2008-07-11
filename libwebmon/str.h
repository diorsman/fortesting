/**
 *  File: str.h
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

#ifndef _STR_H_
#define _STR_H_

struct string_t {
    char *buf;
#define LENGTH_DEF  1024
    int length;
    int index;
    int rest;
#define ADDSTEP_DEF 1024
    int addstep;
};

struct string_t *string_create(size_t length, size_t addstep);
int string_add(struct string_t *string, const char *add, size_t add_len);
char *string_get(struct string_t *string);
size_t string_len(struct string_t *string);
void string_trunc(struct string_t *string, size_t len);
void string_clear(struct string_t *string);
void string_free(struct string_t *string);
void string_free2(struct string_t *string);

#endif  /* _STR_H_ */
