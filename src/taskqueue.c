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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include "taskqueue.h"
#include "utils.h"

struct tq_worker {
    pthread_t thread;
    int id;

    struct tq_queue *queue;

    bool exit;
};

struct tq_job {
    tq_job_func func;
    void *arg;

    struct tq_job *prev;
    struct tq_job *next;
};

struct tq_queue {
    struct tq_job *jobs;
    struct tq_job *next_job;
    int nb_jobs;

    struct tq_worker *workers;
    int nb_workers;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    tq_job_started_hook job_started_hook;
    tq_job_done_hook job_done_hook;
};

static void *tq_worker_func(void *);

struct tq_queue *
tq_queue_new(int nb_workers) {
    struct tq_queue *queue;
    int err;

    queue = tq_malloc(sizeof(struct tq_queue));
    if (!queue) {
        tq_set_error("cannot allocate task queue: %m");
        return NULL;
    }

    memset(queue, 0, sizeof(struct tq_queue));

    queue->nb_workers = nb_workers;
    queue->workers = tq_calloc((size_t)nb_workers, sizeof(struct tq_worker));
    if (!queue->workers) {
        tq_set_error("cannot allocate workers: %m");
        tq_free(queue);
        return NULL;
    }

    for (int i = 0; i < queue->nb_workers; i++) {
        struct tq_worker *worker;

        worker = queue->workers + i;
        worker->id = i;
        worker->queue = queue;
    }

    if (tq_mutex_init(&queue->mutex) == -1) {
        tq_free(queue->workers);
        tq_free(queue);
        return NULL;
    }

    err = pthread_cond_init(&queue->cond, NULL);
    if (err) {
        tq_mutex_free(&queue->mutex);
        tq_free(queue->workers);
        tq_free(queue);
        return NULL;
    }

    return queue;
}

void
tq_queue_delete(struct tq_queue *queue) {
    struct tq_job *job;

    if (!queue)
        return;

    job = queue->jobs;
    while (job) {
        struct tq_job *next;

        next = job->next;
        tq_free(job);
        job = next;
    }

    tq_free(queue->workers);

    tq_mutex_free(&queue->mutex);
    pthread_cond_destroy(&queue->cond);

    tq_free(queue);
}

void
tq_queue_set_job_started_hook(struct tq_queue *queue,
                              tq_job_started_hook hook) {
    queue->job_started_hook = hook;
}

void
tq_queue_set_job_done_hook(struct tq_queue *queue,
                           tq_job_done_hook hook) {
    queue->job_done_hook = hook;
}

int
tq_queue_get_nb_jobs(struct tq_queue *queue) {
    return queue->nb_jobs;
}

int
tq_queue_start(struct tq_queue *queue) {
    int err;

    if (tq_mutex_lock(&queue->mutex) == -1)
        return -1;

    for (int i = 0; i < queue->nb_workers; i++) {
        struct tq_worker *worker;

        worker = queue->workers + i;

        err = pthread_create(&worker->thread, NULL, tq_worker_func, worker);
        if (err) {
            tq_set_error("cannot create thread: %s", strerror(err));

            /* Cancel all previously created threads */
            for (int j = 0; j < i; j++) {
                struct tq_worker *prev_worker;

                prev_worker = queue->workers + j;
                pthread_cancel(prev_worker->thread);
            }

            /* Let the workers get to the cancellation point */
            tq_mutex_unlock(&queue->mutex);

            /* We can now join all previously created threads */
            for (int j = 0; j < i; j++) {
                struct tq_worker *prev_worker;

                prev_worker = queue->workers + j;
                pthread_join(prev_worker->thread, NULL);
            }

            return -1;
        }
    }

    tq_mutex_unlock(&queue->mutex);
    return 0;
}

int
tq_queue_stop(struct tq_queue *queue) {
    int ret, err;

    if (tq_mutex_lock(&queue->mutex) == -1)
        return -1;

    for (int i = 0; i < queue->nb_workers; i++) {
        struct tq_worker *worker;

        worker = queue->workers + i;
        worker->exit = true;
    }

    tq_mutex_unlock(&queue->mutex);

    /* Wake up all workers that may be waiting for a job */
    pthread_cond_broadcast(&queue->cond);

    ret = 0;
    for (int i = 0; i < queue->nb_workers; i++) {
        struct tq_worker *worker;
        void *res;

        worker = queue->workers + i;

        err = pthread_join(worker->thread, &res);
        if (err) {
            tq_set_error("cannot join thread: %s", strerror(err));
            ret = -1;
            continue;
        }
    }

    return ret;
}

int
tq_queue_add_job(struct tq_queue *queue, tq_job_func func, void *arg) {
    struct tq_job *job;

    job = tq_malloc(sizeof(struct tq_job));
    if (!job) {
        tq_set_error("cannot allocate job: %m");
        return -1;
    }

    memset(job, 0, sizeof(struct tq_job));

    job->func = func;
    job->arg = arg;

    if (tq_mutex_lock(&queue->mutex) == -1) {
        tq_free(job);
        return -1;
    }

    if (queue->jobs) {
        queue->jobs->prev = job;
    } else {
        queue->next_job = job;
    }
    job->next = queue->jobs;

    queue->jobs = job;
    queue->nb_jobs++;

    if (queue->nb_jobs == 1) {
        /* The queue is not empty anymore */
        pthread_cond_broadcast(&queue->cond);
    }

    tq_mutex_unlock(&queue->mutex);
    return 0;
}

int
tq_queue_drain(struct tq_queue *queue) {
    int err;

    for (;;) {
        if (tq_mutex_lock(&queue->mutex) == -1)
            return -1;

        /* Check before waiting in case there are currently no job in the
         * queue */
        if (queue->nb_jobs == 0)
            break;

        err = pthread_cond_wait(&queue->cond, &queue->mutex);
        if (err) {
            tq_set_error("cannot wait for condition: %s", strerror(err));
            tq_mutex_unlock(&queue->mutex);
            return -1;
        }

        /* Check after waiting in case the last job was just removed */
        if (queue->nb_jobs == 0)
            break;

        tq_mutex_unlock(&queue->mutex);
    }

    tq_mutex_unlock(&queue->mutex);
    return 0;
}

static void *
tq_worker_func(void *arg) {
    struct tq_worker *worker;
    struct tq_queue *queue;

    worker = arg;
    queue = worker->queue;

    /* Wait for the initialization to complete */
    if (tq_mutex_lock(&queue->mutex) == -1) {
        tq_trace("%s", tq_get_error());
        return NULL;
    }

    tq_mutex_unlock(&queue->mutex);

    /* If initialization fails, the thread is going to be cancelled */
    pthread_testcancel();

    for (;;) {
        struct tq_job *job;

        /* Take the next job */
        if (tq_mutex_lock(&queue->mutex) == -1) {
            tq_trace("%s", tq_get_error());
            return NULL;
        }

        do {
            int err;

            if (worker->exit) {
                tq_mutex_unlock(&queue->mutex);
                pthread_exit(NULL);
            }

            job = queue->next_job;
            if (!job) {
                err = pthread_cond_wait(&queue->cond, &queue->mutex);
                if (err) {
                    tq_trace("cannot wait for condition: %s", strerror(err));
                    tq_mutex_unlock(&queue->mutex);
                    return NULL;
                }
            }
        } while (!job);

        if (job->prev) {
            job->prev->next = NULL;
            queue->next_job = job->prev;
        } else {
            queue->jobs = NULL;
            queue->next_job = NULL;
        }
        queue->nb_jobs--;

        if (queue->nb_jobs == 0)
            pthread_cond_broadcast(&queue->cond);

        tq_mutex_unlock(&queue->mutex);

        /* Process the job */
        if (queue->job_started_hook)
            queue->job_started_hook(job->arg);

        job->func(job->arg);

        if (queue->job_done_hook)
            queue->job_done_hook(job->arg);

        tq_free(job);
    }

    return NULL;
}
