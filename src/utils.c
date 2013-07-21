/*
 * Copyright (c) 2013 Nicolas Martyanoff
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "taskqueue.h"
#include "utils.h"

#define TQ_ERROR_BUFSZ 1024U

static __thread char tq_error_buf[TQ_ERROR_BUFSZ];

#define TQ_DEFAULT_ALLOCATOR \
    {                        \
        .malloc = malloc,    \
        .free = free,        \
        .calloc = calloc,    \
        .realloc = realloc   \
    }


static const struct tq_memory_allocator tq_default_allocator =
    TQ_DEFAULT_ALLOCATOR;

static struct tq_memory_allocator tq_allocator = TQ_DEFAULT_ALLOCATOR;

struct tq_memory_allocator *tq_default_memory_allocator;


const char *
tq_get_error(void) {
    return tq_error_buf;
}

void
tq_set_error(const char *fmt, ...) {
    char buf[TQ_ERROR_BUFSZ];
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vsnprintf(buf, TQ_ERROR_BUFSZ, fmt, ap);
    va_end(ap);

    if ((size_t)ret >= TQ_ERROR_BUFSZ) {
        memcpy(tq_error_buf, buf, TQ_ERROR_BUFSZ);
        tq_error_buf[TQ_ERROR_BUFSZ - 1] = '\0';
        return;
    }

    strncpy(tq_error_buf, buf, (size_t)ret + 1);
    tq_error_buf[ret] = '\0';
}


void
tq_set_memory_allocator(const struct tq_memory_allocator *allocator) {
    tq_allocator = *allocator;
}

void *
tq_malloc(size_t sz) {
    return tq_allocator.malloc(sz);
}

void
tq_free(void *ptr) {
    tq_allocator.free(ptr);
}

void *
tq_calloc(size_t nb, size_t sz) {
    return tq_allocator.calloc(nb, sz);
}

void *
tq_realloc(void *ptr, size_t sz) {
    return tq_allocator.realloc(ptr, sz);
}


#ifndef NDEBUG
static pthread_mutex_t tq_trace_mutex = PTHREAD_MUTEX_INITIALIZER;

void tq_trace(const char *fmt, ...) {
    va_list ap;

    if (tq_mutex_lock(&tq_trace_mutex) == -1)
        fprintf(stderr, "%s\n", tq_get_error());

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    fputc('\n', stdout);

    fflush(stdout);

    if (tq_mutex_unlock(&tq_trace_mutex) == -1)
        fprintf(stderr, "%s\n", tq_get_error());
}
#endif


int
tq_mutex_init(pthread_mutex_t *mutex) {
    int err;

    err = pthread_mutex_init(mutex, NULL);
    if (err) {
        tq_set_error("cannot create mutex: %s", strerror(err));
        return -1;
    }

    return 0;
}

int
tq_mutex_free(pthread_mutex_t *mutex) {
    int err;

    err = pthread_mutex_destroy(mutex);
    if (err) {
        tq_set_error("cannot destroy mutex: %s", strerror(err));
        return -1;
    }

    return 0;
}

int
tq_mutex_lock(pthread_mutex_t *mutex) {
    int err;

    err = pthread_mutex_lock(mutex);
    if (err) {
        const char *errstr;

        errstr = strerror(err);

        if (mutex != &tq_trace_mutex)
            tq_trace("cannot lock mutex: %s", errstr);

        tq_set_error("cannot lock mutex: %s", errstr);
        return -1;
    }

#if 0
    if (mutex != &tq_trace_mutex)
        tq_trace("LOCK   %p", mutex);
#endif

    return 0;
}

int
tq_mutex_unlock(pthread_mutex_t *mutex) {
    int err;

#if 0
    if (mutex != &tq_trace_mutex)
        tq_trace("UNLOCK %p", mutex);
#endif

    err = pthread_mutex_unlock(mutex);
    if (err) {
        const char *errstr;

        errstr = strerror(err);

        if (mutex != &tq_trace_mutex)
            tq_trace("cannot unlock mutex: %s", errstr);

        tq_set_error("cannot unlock mutex: %s", errstr);
        return -1;
    }

    return 0;
}
