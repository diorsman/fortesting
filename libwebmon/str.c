/**
 *  File: str.c
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

#include <stdlib.h>
#include <string.h>

#include "str.h"

struct string_t *
string_create(size_t length, size_t addstep)
{
    struct string_t *ret;

    if (length == 0)
        length = LENGTH_DEF;
    if (addstep == 0)
        addstep = ADDSTEP_DEF;

    ret = malloc(sizeof(struct string_t));
    if (ret == NULL)
        return NULL;
    ret->length = length;
    ret->index = 0;
    ret->rest = length - 1;
    ret->addstep = addstep;
    ret->buf = malloc(ret->length);
    if (ret->buf == NULL) {
        free(ret);
        return NULL;
    }
    ret->buf[0] = '\0';
    return ret;
}

int
string_add(struct string_t *string, const char *add, size_t add_len)
{
    int n;
    void *tmpp;

    /* verify */
    if (string == NULL)
        return -1;
    if (add == NULL)
        return 0;
    /* zero length */
    if (add_len == 0)
        n = strlen(add);
    else
        n = add_len;
    if (n == 0)
        return 0;

    /* is space enough? */
    while (n > string->rest) {
        tmpp = realloc(string->buf, string->length + string->addstep);
        if (tmpp == NULL) {
            return -1;
        } else {
            string->buf = tmpp;
            string->length += string->addstep;
            string->rest += string->addstep;
        }
    }

    /* strcat */
    memcpy(string->buf + string->index, add, n);
    string->index += n;
    string->rest -= n;
    string->buf[string->index] = '\0';
    return 0;
}

char *
string_get(struct string_t *string)
{
    return string->buf;
}

size_t 
string_len(struct string_t *string)
{
    return string->index;
}

void
string_trunc(struct string_t *string, size_t len)
{
    string->index = len;
    string->buf[len] = '\0';
}

void
string_clear(struct string_t *string)
{
    if (string != NULL) {
        string->buf[0] = '\0';
        string->index = 0;
        string->rest = string->length - 1;
    }
}

void
string_free(struct string_t *string)
{
    if (string != NULL) {
        free(string->buf);
        free(string);
    }
}

void
string_free2(struct string_t *string)
{
    if (string != NULL) {
        free(string);
    }
}
