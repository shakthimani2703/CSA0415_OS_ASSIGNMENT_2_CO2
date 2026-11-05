#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
/* Railway booking: TRAINS trains x COACHES coaches x SEATS seats. AGENTS threads each try
   BOOKINGS bookings of a random seat. Each booking holds the lock while it checks and marks
   the seat and does a little "ticket printing" work (the critical section).
   Granularity: 0 = one global lock, 1 = lock per train, 2 = lock per coach. */
#define TRAINS 50
#define COACHES 20
#define SEATS 72
#define BOOKINGS 400000
static int seat[TRAINS][COACHES][SEATS];
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t train_lock[TRAINS];
static pthread_mutex_t coach_lock[TRAINS][COACHES];
static int mode;
static long sold[64];
static pthread_mutex_t *pick(int t, int c)
{
    if (mode == 0) return &global_lock;
    if (mode == 1) return &train_lock[t];
    return &coach_lock[t][c];
}
static void *agent(void *arg)
{
    long id = (long)arg; unsigned s = 1234 + id; long ok = 0;
    for (int b = 0; b < BOOKINGS; b++) {
        int t = rand_r(&s) % TRAINS, c = rand_r(&s) % COACHES, k = rand_r(&s) % SEATS;
        pthread_mutex_t *m = pick(t, c);
        pthread_mutex_lock(m);
        if (seat[t][c][k] == 0) { seat[t][c][k] = (int)id + 1; ok++; }
        volatile int w = 0; for (int i = 0; i < 200; i++) w += i;   /* ticket work */
        pthread_mutex_unlock(m);
    }
    sold[id] = ok;
    return NULL;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void)
{
    for (int t = 0; t < TRAINS; t++) {
        pthread_mutex_init(&train_lock[t], NULL);
        for (int c = 0; c < COACHES; c++) pthread_mutex_init(&coach_lock[t][c], NULL);
    }
    const char *names[] = {"one global lock", "lock per train", "lock per coach"};
    int agents[] = {1, 2, 4, 16};
    printf("%-16s %7s %12s %14s %10s\n", "granularity", "agents", "time (s)", "bookings/s", "check");
    for (mode = 0; mode < 3; mode++)
        for (int a = 0; a < 4; a++) {
            int n = agents[a];
            for (int t = 0; t < TRAINS; t++) for (int c = 0; c < COACHES; c++)
                for (int k = 0; k < SEATS; k++) seat[t][c][k] = 0;
            pthread_t th[64]; double el = 1e9;
            for (int rep = 0; rep < 3; rep++) {          /* best of 3 runs */
                for (int t = 0; t < TRAINS; t++) for (int c = 0; c < COACHES; c++)
                    for (int k = 0; k < SEATS; k++) seat[t][c][k] = 0;
                double t0 = now();
                for (long i = 0; i < n; i++) pthread_create(&th[i], NULL, agent, (void *)i);
                for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
                if (now() - t0 < el) el = now() - t0;
            }
            long total = 0, filled = 0;
            for (int i = 0; i < n; i++) total += sold[i];
            for (int t = 0; t < TRAINS; t++) for (int c = 0; c < COACHES; c++)
                for (int k = 0; k < SEATS; k++) filled += seat[t][c][k] != 0;
            printf("%-16s %7d %12.3f %14.0f %10s\n", names[mode], n, el,
                   (double)n * BOOKINGS / el, total == filled ? "no double sale" : "DOUBLE SALE");
        }
    return 0;
}
