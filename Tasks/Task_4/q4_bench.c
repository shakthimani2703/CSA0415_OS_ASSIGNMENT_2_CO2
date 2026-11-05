#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/wait.h>

/* Transfers SIZE bytes of int readings from a producer process to a consumer process
   using (1) a pipe, (2) POSIX shared memory ring buffer + semaphores, (3) a POSIX message
   queue. The consumer sums the readings so we can check nothing is lost. */
#define CHUNK 65536                   /* bytes per pipe write / shm slot */
#define SLOTS 8
#define MQMSG 8192                    /* default Linux msgsize_max */

static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void fill(int *b, size_t n, uint32_t *seq){for(size_t i=0;i<n;i++)b[i]=(int)((*seq)++ & 1023);}
static uint64_t expected(size_t bytes){uint64_t s=0;for(uint32_t i=0;i<bytes/4;i++)s+=i&1023;return s;}

static double via_pipe(size_t bytes, uint64_t *sum)
{
    int fd[2], res[2]; if (pipe(fd) || pipe(res)) exit(1);
    double t = now();
    if (fork() == 0) {                                   /* consumer */
        close(fd[1]); static int buf[CHUNK/4]; uint64_t s = 0; ssize_t n;
        while ((n = read(fd[0], buf, CHUNK)) > 0)
            for (ssize_t i = 0; i < n / 4; i++) s += buf[i];
        if (write(res[1], &s, sizeof s) < 0) _exit(1);
        _exit(0);
    }
    close(fd[0]); static int buf[CHUNK/4]; uint32_t seq = 0;
    for (size_t sent = 0; sent < bytes; sent += CHUNK) {
        fill(buf, CHUNK/4, &seq);
        if (write(fd[1], buf, CHUNK) != CHUNK) exit(1);
    }
    close(fd[1]);
    if (read(res[0], sum, sizeof *sum) < 0) exit(1);
    wait(NULL); close(res[0]); close(res[1]);
    return now() - t;
}

struct ring { sem_t empty, full; int done; size_t len[SLOTS]; int data[SLOTS][CHUNK/4]; };
static double via_shm(size_t bytes, uint64_t *sum)
{
    struct ring *r = mmap(NULL, sizeof *r, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    sem_init(&r->empty, 1, SLOTS); sem_init(&r->full, 1, 0);
    int res[2]; if (pipe(res)) exit(1);
    double t = now();
    if (fork() == 0) {                                   /* consumer: reads in place */
        uint64_t s = 0; int slot = 0;
        for (;;) {
            sem_wait(&r->full);
            size_t n = r->len[slot];
            if (n == 0) break;                           /* length 0 = end of data */
            for (size_t i = 0; i < n / 4; i++) s += r->data[slot][i];
            sem_post(&r->empty); slot = (slot + 1) % SLOTS;
        }
        if (write(res[1], &s, sizeof s) < 0) _exit(1);
        _exit(0);
    }
    uint32_t seq = 0; int slot = 0;
    for (size_t sent = 0; sent < bytes; sent += CHUNK) {
        sem_wait(&r->empty);
        fill(r->data[slot], CHUNK/4, &seq);              /* producer writes in place */
        r->len[slot] = CHUNK;
        sem_post(&r->full); slot = (slot + 1) % SLOTS;
    }
    sem_wait(&r->empty); r->len[slot] = 0; sem_post(&r->full);
    if (read(res[0], sum, sizeof *sum) < 0) exit(1);
    wait(NULL); close(res[0]); close(res[1]);
    sem_destroy(&r->empty); sem_destroy(&r->full); munmap(r, sizeof *r);
    return now() - t;
}

static double via_mq(size_t bytes, uint64_t *sum)
{
    struct mq_attr a = {0}; a.mq_maxmsg = 10; a.mq_msgsize = MQMSG;
    mq_unlink("/wx"); mqd_t q = mq_open("/wx", O_CREAT|O_RDWR, 0600, &a);
    if (q == (mqd_t)-1) { perror("mq_open"); exit(1); }
    int res[2]; if (pipe(res)) exit(1);
    double t = now();
    if (fork() == 0) {
        static int buf[MQMSG/4]; uint64_t s = 0; ssize_t n;
        while ((n = mq_receive(q, (char *)buf, MQMSG, NULL)) > 0)
            for (ssize_t i = 0; i < n / 4; i++) s += buf[i];
        if (write(res[1], &s, sizeof s) < 0) _exit(1);
        _exit(0);
    }
    static int buf[MQMSG/4]; uint32_t seq = 0;
    for (size_t sent = 0; sent < bytes; sent += MQMSG) {
        fill(buf, MQMSG/4, &seq);
        if (mq_send(q, (char *)buf, MQMSG, 0)) { perror("mq_send"); exit(1); }
    }
    mq_send(q, (char *)buf, 0, 0);                       /* empty message = end of data */
    if (read(res[0], sum, sizeof *sum) < 0) exit(1);
    wait(NULL); mq_close(q); mq_unlink("/wx"); close(res[0]); close(res[1]);
    return now() - t;
}

int main(void)
{
    size_t mb[] = {1, 10, 100};
    printf("%-8s %12s %12s %12s   check\n", "size", "pipe (ms)", "shm (ms)", "mqueue (ms)");
    for (int k = 0; k < 3; k++) {
        size_t bytes = mb[k] << 20; uint64_t s1, s2, s3, e = expected(bytes);
        double best[3] = {1e9, 1e9, 1e9};
        for (int rep = 0; rep < 5; rep++) {
            double a = via_pipe(bytes, &s1), b = via_shm(bytes, &s2), c = via_mq(bytes, &s3);
            if (a < best[0]) best[0] = a;
            if (b < best[1]) best[1] = b;
            if (c < best[2]) best[2] = c;
        }
        printf("%3zu MB   %12.2f %12.2f %12.2f   %s\n", mb[k], best[0]*1e3, best[1]*1e3, best[2]*1e3,
               (s1 == e && s2 == e && s3 == e) ? "all sums correct" : "MISMATCH");
    }
    return 0;
}
