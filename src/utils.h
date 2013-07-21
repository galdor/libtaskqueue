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

#ifndef LIBTASKQUEUE_UTILS_H
#define LIBTASKQUEUE_UTILS_H

void tq_set_error(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

void *tq_malloc(size_t sz);
void tq_free(void *ptr);
void *tq_calloc(size_t nb, size_t sz);
void *tq_realloc(void *ptr, size_t sz);

#ifdef NDEBUG
#   define tq_trace(...) 0
#else
void tq_trace(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#endif

int tq_mutex_init(pthread_mutex_t *mutex);
int tq_mutex_free(pthread_mutex_t *mutex);
int tq_mutex_lock(pthread_mutex_t *mutex);
int tq_mutex_unlock(pthread_mutex_t *mutex);

#endif

