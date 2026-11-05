#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

/* Simulates an exam server: one child process per student session. */
static void show_proc(pid_t pid)
{
    char path[64], line[256];
    snprintf(path, sizeof path, "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "Name:", 5) || !strncmp(line, "State:", 6) ||
            !strncmp(line, "PPid:", 5) || !strncmp(line, "VmRSS:", 6) ||
            !strncmp(line, "voluntary_ctxt_switches:", 24))
            printf("    %s", line);
    fclose(f);
}

int main(int argc, char *argv[])
{
    int n = argc > 1 ? atoi(argv[1]) : 3;
    pid_t kids[64];
    printf("server (parent) PID %d creating %d student sessions\n", (int)getpid(), n);
    fflush(stdout);              /* avoid duplicating the buffer in children */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {                 /* child = one student session */
            printf("  session %d: PID %d, parent %d\n", i + 1, (int)getpid(), (int)getppid());
            fflush(stdout);
            sleep(3);                   /* waiting state: blocked on "student input" */
            _exit(i + 1);
        }
        kids[i] = pid;
    }
    sleep(1);
    printf("\n--- ps while sessions are blocked ---\n");
    fflush(stdout);
    char cmd[128];
    snprintf(cmd, sizeof cmd, "ps -o pid,ppid,stat,rss,cmd -C q1_fork");
    if (system(cmd) == -1) perror("system");
    printf("\n--- /proc/<pid>/status of first session ---\n");
    show_proc(kids[0]);
    for (int i = 0; i < n; i++) {
        int st;
        waitpid(kids[i], &st, 0);
        printf("session PID %d terminated, exit status %d\n", (int)kids[i], WEXITSTATUS(st));
    }
    return 0;
}
