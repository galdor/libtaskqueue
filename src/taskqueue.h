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

#ifndef LIBTASKQUEUE_TASKQUEUE_H
#define LIBTASKQUEUE_TASKQUEUE_H

#include <stdbool.h>

struct tq_memory_allocator {
   void *(*malloc)(size_t sz);
   void (*free)(void *ptr);
   void *(*calloc)(size_t nb, size_t sz);
   void *(*realloc)(void *ptr, size_t sz);
};

extern struct tq_memory_allocator *tq_default_memory_allocator;

typedef int (*tq_job_func)(void *);

typedef void (*tq_job_started_hook)(void *);
typedef void (*tq_job_done_hook)(void *);


const char *tq_get_error(void);

void tq_set_memory_allocator(const struct tq_memory_allocator *allocator);


struct tq_queue *tq_queue_new(int nb_workers);
void tq_queue_delete(struct tq_queue *queue);

void tq_queue_set_job_started_hook(struct tq_queue *queue,
                                   tq_job_started_hook hook);
void tq_queue_set_job_done_hook(struct tq_queue *queue,
                                tq_job_done_hook hook);
int tq_queue_get_nb_jobs(struct tq_queue *queue);

int tq_queue_start(struct tq_queue *queue);
int tq_queue_stop(struct tq_queue *queue);
int tq_queue_add_job(struct tq_queue *queue, tq_job_func func, void *arg);
int tq_queue_drain(struct tq_queue *queue);

#endif
