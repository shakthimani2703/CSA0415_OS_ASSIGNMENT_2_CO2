#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
/* Bounded buffer with NP producers and NC consumers. Each item is unique
   (producer id * ITEMS + k) so we can check that every item is consumed exactly once.
   Usage: q5_multi NP NC use_mutex(1/0) */
#define SIZE 5
#define ITEMS 100000
int buffer[SIZE], in = 0, out = 0, np, nc, use_mutex;
sem_t empty, full;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
unsigned char *seen;
static void L(void){ if (use_mutex) pthread_mutex_lock(&lock); }
static void U(void){ if (use_mutex) pthread_mutex_unlock(&lock); }
void *producer(void *arg)
{
    int id = (int)(long)arg;
    for (int k = 0; k < ITEMS; k++) {
        sem_wait(&empty); L();
        buffer[in] = id * ITEMS + k; in = (in + 1) % SIZE;
        U(); sem_post(&full);
    }
    return NULL;
}
void *consumer(void *arg)
{
    long quota = (long)arg;
    for (long k = 0; k < quota; k++) {
        sem_wait(&full); L();
        int item = buffer[out]; out = (out + 1) % SIZE;
        U(); sem_post(&empty);
        __atomic_add_fetch(&seen[item], 1, __ATOMIC_RELAXED);   /* tally, itself race-free */
    }
    return NULL;
}
int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s NP NC mutex\n", argv[0]); return 1; }
    np = atoi(argv[1]); nc = atoi(argv[2]); use_mutex = atoi(argv[3]);
    long total = (long)np * ITEMS;
    seen = calloc(total, 1);
    sem_init(&empty, 0, SIZE); sem_init(&full, 0, 0);
    pthread_t t[64];
    for (int i = 0; i < np; i++) pthread_create(&t[i], NULL, producer, (void *)(long)i);
    for (int i = 0; i < nc; i++) pthread_create(&t[np + i], NULL, consumer,
                                  (void *)(long)(total / nc + (i < total % nc)));
    for (int i = 0; i < np + nc; i++) pthread_join(t[i], NULL);
    long once = 0, missing = 0, dup = 0;
    for (long i = 0; i < total; i++) {
        if (seen[i] == 1) once++; else if (seen[i] == 0) missing++; else dup += seen[i] - 1;
    }
    printf("%d producers, %d consumers, mutex %-3s: %ld items, exactly once=%ld, lost=%ld, duplicated=%ld -> %s\n",
           np, nc, use_mutex ? "on" : "off", total, once, missing, dup,
           (once == total) ? "CORRECT" : "WRONG");
    return 0;
}
