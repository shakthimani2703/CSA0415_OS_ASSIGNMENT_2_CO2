#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
/* Peterson's algorithm for two threads, run twice:
   (a) with plain volatile variables - the CPU may reorder the store to flag[i] after the
       load of flag[j] (store buffer), so both threads can enter together;
   (b) with seq_cst atomics (which emit a full fence on x86) - correct. */
#define N 5000000
volatile int vflag[2], vturn; volatile long vcount;
atomic_int aflag[2], aturn; volatile long acount;
void *plain(void *arg)
{
    int i = (int)(long)arg, j = 1 - i;
    for (int k = 0; k < N; k++) {
        vflag[i] = 1; vturn = j;
        while (vflag[j] && vturn == j) ;
        vcount++;                               /* critical section */
        vflag[i] = 0;
    }
    return NULL;
}
void *fenced(void *arg)
{
    int i = (int)(long)arg, j = 1 - i;
    for (int k = 0; k < N; k++) {
        atomic_store(&aflag[i], 1); atomic_store(&aturn, j);
        while (atomic_load(&aflag[j]) && atomic_load(&aturn) == j) ;
        acount++;
        atomic_store(&aflag[i], 0);
    }
    return NULL;
}
int main(void)
{
    pthread_t a, b;
    pthread_create(&a, NULL, plain, (void *)0L); pthread_create(&b, NULL, plain, (void *)1L);
    pthread_join(a, NULL); pthread_join(b, NULL);
    printf("Peterson, plain volatile : count = %ld (expected %d) -> %s\n", vcount, 2 * N,
           vcount == 2 * N ? "mutual exclusion held" : "MUTUAL EXCLUSION VIOLATED");
    pthread_create(&a, NULL, fenced, (void *)0L); pthread_create(&b, NULL, fenced, (void *)1L);
    pthread_join(a, NULL); pthread_join(b, NULL);
    printf("Peterson, seq_cst atomics: count = %ld (expected %d) -> %s\n", acount, 2 * N,
           acount == 2 * N ? "mutual exclusion held" : "MUTUAL EXCLUSION VIOLATED");
    return 0;
}
