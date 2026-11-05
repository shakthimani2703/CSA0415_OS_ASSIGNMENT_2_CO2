#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
/* Same workload with a pool of worker PROCESSES; results returned through a pipe. */
#define CAMS 64
#define FRAMES 20
#define W 320
#define H 240
static unsigned char frame[CAMS][W * H];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int read_plate(int cam, int f)
{
    uint32_t h = 2166136261u ^ (uint32_t)(cam * 131 + f);
    for (int pass = 0; pass < 6; pass++)
        for (int i = 0; i < W * H; i++) h = (h ^ frame[cam][i]) * 16777619u;
    return (h % 10) == 0;
}
int main(void)
{
    srand(42);
    for (int c = 0; c < CAMS; c++) for (int i = 0; i < W * H; i++) frame[c][i] = rand() & 255;
    int sizes[] = {1, 2, 4, 8, 16};
    for (int k = 0; k < 5; k++) {
        int n = sizes[k], fd[2];
        if (pipe(fd)) return 1;
        double t = now();
        for (int w = 0; w < n; w++) {
            if (fork() == 0) {            /* static split: worker w takes cams w, w+n, ... */
                close(fd[0]);
                long local = 0;
                for (int c = w; c < CAMS; c += n)
                    for (int f = 0; f < FRAMES; f++) local += read_plate(c, f);
                if (write(fd[1], &local, sizeof local) != sizeof local) _exit(1);
                _exit(0);
            }
        }
        close(fd[1]);
        long total = 0, v;
        while (read(fd[0], &v, sizeof v) == sizeof v) total += v;   /* IPC combine */
        close(fd[0]);
        while (wait(NULL) > 0) ;
        printf("pool of %2d processes: %.3f s  flagged=%ld\n", n, now() - t, total);
    }
    return 0;
}
