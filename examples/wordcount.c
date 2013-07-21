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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "taskqueue.h"

struct job {
    const char *ptr;
    size_t len;
};

static void die(const char *, ...)
    __attribute__((format(printf, 1, 2)));
static void usage(const char *, int);

static void map_file(const char *, char **, size_t *);
static int job_func(void *);

static const char *get_next_word_boundary(const char *, size_t);

static size_t word_count;

int
main(int argc, char **argv) {
    const char *path;
    int opt;

    char *map, *ptr;
    size_t mapsz, len;
    size_t chunk_size;

    struct tq_queue *taskqueue;
    int nb_threads;

    nb_threads = 4;

    opterr = 0;
    while ((opt = getopt(argc, argv, "ht:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0], 0);
                break;

            case 't':
                {
                    unsigned long lval;

                    errno = 0;
                    lval = strtoul(optarg, NULL, 10);
                    if (errno)
                        die("invalid number of threads: %m");
                    if (lval > INT_MAX)
                        die("invalid number of threads");

                    nb_threads = (int)lval;
                    break;
                }

            case '?':
                usage(argv[0], 1);
        }
    }

    if (optind >= argc)
        usage(argv[0], 1);

    path = argv[optind];

    map_file(path, &map, &mapsz);

    taskqueue = tq_queue_new(nb_threads);
    if (!taskqueue)
        die("cannot create task queue: %s", tq_get_error());

    chunk_size = 4 * 1024;
    ptr = map;
    len = mapsz;

    if (tq_queue_start(taskqueue) == -1)
        die("cannot start task queue: %s", tq_get_error());

    while (len > 0) {
        struct job *job;
        const char *boundary;

        job = malloc(sizeof(struct job));
        if (!job)
            die("cannot allocate job: %m");

        job->ptr = ptr;
        job->len = (len >= chunk_size) ? chunk_size : len;

        /* Make sure words are not split */
        boundary = get_next_word_boundary(job->ptr + job->len, len - job->len);
        if (boundary)
            job->len = (size_t)(boundary - job->ptr);

        len -= job->len;
        ptr += job->len;

        if (tq_queue_add_job(taskqueue, job_func, job) == -1)
            die("cannot add job: %s", tq_get_error());
    }

    if (tq_queue_drain(taskqueue) == -1)
        die("cannot drain task queue: %s", tq_get_error());
    if (tq_queue_stop(taskqueue) == -1)
        die("cannot stop task queue: %s", tq_get_error());

    tq_queue_delete(taskqueue);

    printf("%zu words read\n", word_count);

    munmap(map, mapsz);
    return 0;
}

static void
usage(const char *argv0, int exit_code) {
    printf("Usage: %s [-ht] <file>\n"
            "\n"
            "Options:\n"
            "  -h         display help\n"
            "  -t         number of threads used\n",
            argv0);
    exit(exit_code);
}

static void
die(const char *fmt, ...) {
    va_list ap;

    fprintf(stderr, "fatal error: ");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    putc('\n', stderr);
    exit(1);
}

static void
map_file(const char *path, char **pmap, size_t *psz) {
    int fd;
    struct stat st;
    void *map;
    size_t sz;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        die("cannot open %s: %m", path);

    if (fstat(fd, &st) == -1)
        die("cannot stat %s: %m", path);

    sz = (size_t)st.st_size;

    map = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
        die("cannot map %s: %m", path);

    close(fd);

    *pmap = map;
    *psz = sz;
}

static int
job_func(void *arg) {
    struct job *job;
    size_t nb_words;

    job = arg;

    nb_words = 0;
    while (job->len > 0) {
        const char *boundary;
        size_t word_len;

        boundary = get_next_word_boundary(job->ptr, job->len);
        if (boundary) {
            word_len = (size_t)(boundary - job->ptr);
        } else {
            word_len = job->len;
        }

        nb_words++;

        job->ptr += word_len + 1;
        job->len -= word_len;
    }

    __sync_fetch_and_add(&word_count, nb_words);

    free(job);
    return 0;
}

static const char *
get_next_word_boundary(const char *ptr, size_t len) {
    /* Skip until white space */
    while (len > 0) {
        if (isspace((unsigned char)*ptr))
            break;

        ptr++;
        len--;
    }

    /* Skip white space until the next word */
    while (len > 0) {
        if (!isspace((unsigned char)*ptr))
            return ptr;

        ptr++;
        len--;
    }

    return NULL;
}
