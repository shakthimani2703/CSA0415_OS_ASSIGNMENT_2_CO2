#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Times the Banker's safety algorithm (same loop as q6_banker.c, sized dynamically)
   and the deadlock-detection algorithm, for n = 10, 100, 1000 processes and m resource types.
   Worst case: process p can only finish after p+1..n-1 have finished, so each outer pass
   scans the whole list but releases just one process -> n passes of n checks of m resources. */
static int n, m, *avail, *maxm, *alloc, *req;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static long checks;
static int is_safe(int *seq, int use_request)   /* use_request=1 -> detection algorithm */
{
    int *work = malloc(m * sizeof *work), *finish = calloc(n, sizeof *finish), count = 0;
    memcpy(work, avail, m * sizeof *work);
    if (use_request)                             /* detection: idle processes finish at once */
        for (int p = 0; p < n; p++) {
            int zero = 1; for (int r = 0; r < m; r++) if (alloc[p*m+r]) zero = 0;
            if (zero) finish[p] = 1, count++;
        }
    while (count < n) {
        int found = 0;
        for (int p = 0; p < n; p++) {
            if (finish[p]) continue;
            int ok = 1;
            for (int r = 0; r < m; r++) {
                checks++;
                int need = use_request ? req[p*m+r] : maxm[p*m+r] - alloc[p*m+r];
                if (need > work[r]) { ok = 0; break; }
            }
            if (ok) {
                for (int r = 0; r < m; r++) work[r] += alloc[p*m+r];
                finish[p] = 1; seq[count++] = p; found = 1;
            }
        }
        if (!found) { free(work); free(finish); return 0; }
    }
    free(work); free(finish); return 1;
}
static void setup(int worst)
{
    avail = malloc(m * sizeof(int)); maxm = malloc((size_t)n*m*sizeof(int));
    alloc = malloc((size_t)n*m*sizeof(int)); req = malloc((size_t)n*m*sizeof(int));
    for (int r = 0; r < m; r++) avail[r] = 1;
    for (int p = 0; p < n; p++)
        for (int r = 0; r < m; r++) {
            alloc[p*m+r] = 1;
            int need = worst ? n - p : 1;        /* worst: P(n-1) needs 1, P(n-2) needs 2, ... */
            maxm[p*m+r] = 1 + need; req[p*m+r] = need;
        }
}
int main(void)
{
    int sizes[] = {10, 100, 1000}, ms[] = {3, 10};
    printf("%-6s %4s %-6s %14s %14s %16s\n", "n", "m", "case", "safety (us)", "detection (us)", "need checks");
    for (int a = 0; a < 2; a++)
        for (int worst = 0; worst <= 1; worst++)
            for (int k = 0; k < 3; k++) {
                n = sizes[k]; m = ms[a]; setup(worst);
                int *seq = malloc(n * sizeof *seq), reps = n <= 100 ? 2000 : 20;
                double t = now(); checks = 0;
                for (int i = 0; i < reps; i++) if (!is_safe(seq, 0)) { puts("unsafe?"); return 1; }
                double ts = (now() - t) / reps * 1e6; long c = checks / reps;
                t = now();
                for (int i = 0; i < reps; i++) is_safe(seq, 1);
                double td = (now() - t) / reps * 1e6;
                printf("%-6d %4d %-6s %14.2f %14.2f %16ld\n", n, m, worst ? "worst" : "best", ts, td, c);
                free(seq); free(avail); free(maxm); free(alloc); free(req);
            }
    return 0;
}
