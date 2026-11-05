#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
/* Time per update of one shared counter: mutex, spin lock, C11 atomic_fetch_add.
   Also a "long" critical section (about 2 us of work) to show where spinning wastes CPU. */
#define TOTAL 4000000
static long counter; static atomic_long acounter;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t sp;
static int method, per_thread, work;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static double cpu(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void crit(void){ volatile int w = 0; for (int i = 0; i < work; i++) w += i; }
static void *run(void *a)
{
    (void)a;
    for (int i = 0; i < per_thread; i++) {
        if (method == 0) { pthread_mutex_lock(&m); counter++; crit(); pthread_mutex_unlock(&m); }
        else if (method == 1) { pthread_spin_lock(&sp); counter++; crit(); pthread_spin_unlock(&sp); }
        else atomic_fetch_add(&acounter, 1);
    }
    return NULL;
}
int main(void)
{
    pthread_spin_init(&sp, PTHREAD_PROCESS_PRIVATE);
    const char *nm[] = {"mutex", "spin lock", "atomic_fetch_add"};
    int th[] = {1, 4, 16};
    printf("Short critical section (counter++ only), %d updates in total\n", TOTAL);
    printf("%-17s %8s %14s %12s %10s\n", "method", "threads", "ns/update", "CPU/wall", "correct");
    for (work = 0, method = 0; method < 3; method++)
        for (int k = 0; k < 3; k++) {
            int n = th[k]; per_thread = TOTAL / n; counter = 0; acounter = 0;
            pthread_t t[16]; double w0 = now(), c0 = cpu();
            for (int i = 0; i < n; i++) pthread_create(&t[i], NULL, run, NULL);
            for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
            double w = now() - w0, c = cpu() - c0;
            long v = method == 2 ? (long)acounter : counter;
            printf("%-17s %8d %14.1f %12.2f %10s\n", nm[method], n, w / TOTAL * 1e9, c / w,
                   v == TOTAL ? "yes" : "NO");
        }
    printf("\nLong critical section (~5000-iteration loop inside the lock), 40000 updates\n");
    printf("%-17s %8s %14s %12s %14s\n", "method", "threads", "us/update", "CPU/wall", "CPU time (s)");
    for (work = 5000, method = 0; method < 2; method++)
        for (int k = 0; k < 3; k++) {
            int n = th[k]; per_thread = 40000 / n; counter = 0;
            pthread_t t[16]; double w0 = now(), c0 = cpu();
            for (int i = 0; i < n; i++) pthread_create(&t[i], NULL, run, NULL);
            for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
            double w = now() - w0, c = cpu() - c0;
            printf("%-17s %8d %14.2f %12.2f %14.2f\n", nm[method], n, w / 40000 * 1e6, c / w, c);
        }
    return 0;
}
