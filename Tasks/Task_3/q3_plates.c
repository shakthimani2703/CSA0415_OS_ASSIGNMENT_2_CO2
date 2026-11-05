#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* 64 cameras, each delivers FRAMES frames. "Reading a plate" = CPU-heavy hash of a
   320x240 grey frame; plates whose hash ends in 0 are counted as "flagged". */
#define CAMS 64
#define FRAMES 20
#define W 320
#define H 240

static unsigned char frame[CAMS][W * H];      /* shared (read-only) input */
static long flagged = 0;                      /* shared result */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static int next_cam = 0;                      /* shared work index for the pool */

static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}

static int read_plate(int cam, int f)
{
    uint32_t h = 2166136261u ^ (uint32_t)(cam * 131 + f);
    for (int pass = 0; pass < 6; pass++)              /* "OCR" passes */
        for (int i = 0; i < W * H; i++) h = (h ^ frame[cam][i]) * 16777619u;
    return (h % 10) == 0;
}
static long process_camera(int cam)
{
    long local = 0;
    for (int f = 0; f < FRAMES; f++) local += read_plate(cam, f);
    return local;
}
static void *per_camera(void *arg)
{
    long local = process_camera((int)(intptr_t)arg);
    pthread_mutex_lock(&mtx); flagged += local; pthread_mutex_unlock(&mtx);
    return NULL;
}
static void *pool_worker(void *arg)
{
    (void)arg;
    long local = 0;
    for (;;) {
        pthread_mutex_lock(&mtx);
        int cam = next_cam < CAMS ? next_cam++ : -1;
        pthread_mutex_unlock(&mtx);
        if (cam < 0) break;
        local += process_camera(cam);
    }
    pthread_mutex_lock(&mtx); flagged += local; pthread_mutex_unlock(&mtx);
    return NULL;
}
static double run_pool(int n)
{
    pthread_t th[64]; flagged = 0; next_cam = 0;
    double t = now();
    for (int i = 0; i < n; i++) pthread_create(&th[i], NULL, pool_worker, NULL);
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    return now() - t;
}
int main(void)
{
    srand(42);
    for (int c = 0; c < CAMS; c++) for (int i = 0; i < W * H; i++) frame[c][i] = rand() & 255;
    printf("CPUs online: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    double t = now(); long seq = 0;
    for (int c = 0; c < CAMS; c++) seq += process_camera(c);
    double ts = now() - t;
    printf("%-26s %7.3f s  speed-up %.2f  flagged=%ld\n", "sequential", ts, 1.0, seq);
    pthread_t th[CAMS]; flagged = 0; t = now();
    for (int c = 0; c < CAMS; c++) pthread_create(&th[c], NULL, per_camera, (void *)(intptr_t)c);
    for (int c = 0; c < CAMS; c++) pthread_join(th[c], NULL);
    double tp = now() - t;
    printf("%-26s %7.3f s  speed-up %.2f  flagged=%ld\n", "one thread per camera (64)", tp, ts / tp, flagged);
    int sizes[] = {1, 2, 4, 8, 16};
    for (int k = 0; k < 5; k++) {
        double tt = run_pool(sizes[k]);
        char lbl[32]; snprintf(lbl, sizeof lbl, "pool of %d threads", sizes[k]);
        printf("%-26s %7.3f s  speed-up %.2f  flagged=%ld\n", lbl, tt, ts / tt, flagged);
    }
    return 0;
}
