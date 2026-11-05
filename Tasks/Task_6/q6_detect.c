#include <stdio.h>
#include <string.h>
/* Deadlock detection + recovery for the lab cluster.
   Resources: GPU (2 units), LIC = simulation licence (1), STO = fast storage volume (1).
   J0 holds a GPU and the licence, wants storage.  J1 holds storage, wants a GPU.
   J2 holds a GPU, wants the licence.  J3 holds nothing, wants a GPU.            */
#define P 4
#define R 3
const char *res[R] = {"GPU", "LIC", "STO"};
int avail[R]      = {0, 0, 0};
int alloc[P][R]   = {{1, 1, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, 0}};
int request[P][R] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {1, 0, 0}};
int hours_done[P] = {30, 2, 12, 0};            /* work lost if the job is aborted */

int detect(int dead[P])
{
    int work[R], finish[P], changed = 1, n = 0;
    memcpy(work, avail, sizeof work);
    for (int p = 0; p < P; p++) {
        finish[p] = 1;
        for (int r = 0; r < R; r++) if (alloc[p][r]) finish[p] = 0;
    }
    while (changed) {
        changed = 0;
        for (int p = 0; p < P; p++) {
            if (finish[p]) continue;
            int ok = 1;
            for (int r = 0; r < R; r++) if (request[p][r] > work[r]) ok = 0;
            if (ok) {
                for (int r = 0; r < R; r++) work[r] += alloc[p][r];
                finish[p] = 1; changed = 1;
            }
        }
    }
    for (int p = 0; p < P; p++) { dead[p] = !finish[p]; n += dead[p]; }
    return n;
}
int main(void)
{
    int dead[P];
    while (detect(dead)) {
        int victim = -1;
        printf("deadlocked jobs:");
        for (int p = 0; p < P; p++) if (dead[p]) {
            printf(" J%d", p);
            if (victim < 0 || hours_done[p] < hours_done[victim]) victim = p;
        }
        printf("\nrecovery: abort J%d (loses %d GPU-hours), releasing", victim, hours_done[victim]);
        for (int r = 0; r < R; r++) if (alloc[victim][r]) printf(" %s", res[r]);
        printf("\n");
        for (int r = 0; r < R; r++) { avail[r] += alloc[victim][r]; alloc[victim][r] = 0; request[victim][r] = 0; }
    }
    printf("no deadlock remains - J3 and the restarted job can now proceed\n");
    return 0;
}
