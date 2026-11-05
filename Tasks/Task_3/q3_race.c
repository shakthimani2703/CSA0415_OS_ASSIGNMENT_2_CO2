#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
/* Two threads each add 1,000,000 to shared counters. */
#define N 1000000
long plain = 0;
long locked = 0;
atomic_long atom = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
void *work(void *a)
{
    (void)a;
    for (int i = 0; i < N; i++) {
        plain++;                                   /* load, add, store: not atomic */
        pthread_mutex_lock(&m); locked++; pthread_mutex_unlock(&m);
        atomic_fetch_add(&atom, 1);
    }
    return NULL;
}
int main(void)
{
    for (int run = 1; run <= 5; run++) {
        plain = locked = 0; atom = 0;
        pthread_t a, b;
        pthread_create(&a, NULL, work, NULL);
        pthread_create(&b, NULL, work, NULL);
        pthread_join(a, NULL); pthread_join(b, NULL);
        printf("run %d: unprotected=%ld  mutex=%ld  atomic=%ld  (expected %d)\n",
               run, plain, locked, (long)atom, 2 * N);
    }
    return 0;
}
