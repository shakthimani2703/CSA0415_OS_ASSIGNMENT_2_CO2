
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

#define ROUNDS 100000
#define N 2000

static int p1[2], p2[2];

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static int write_byte(int fd, char c)
{
    ssize_t n;
    do {
        n = write(fd, &c, 1);
    } while (n == -1 && errno == EINTR);
    return n == 1 ? 0 : -1;
}

static int read_byte(int fd, char *c)
{
    ssize_t n;
    do {
        n = read(fd, c, 1);
    } while (n == -1 && errno == EINTR);
    return n == 1 ? 0 : -1;
}

static void *echo(void *arg)
{
    (void)arg;
    char c;

    for (int i = 0; i < ROUNDS; i++) {
        if (read_byte(p1[0], &c) != 0)
            break;
        if (write_byte(p2[1], c) != 0)
            break;
    }
    return NULL;
}

static void *nothing(void *arg)
{
    return arg;
}

int main(void)
{
    /* Select an available CPU */
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_getaffinity");
        return 1;
    }

    int cpu;
    for (cpu = 0; cpu < CPU_SETSIZE; cpu++) {
        if (CPU_ISSET(cpu, &set))
            break;
    }

    if (cpu == CPU_SETSIZE) {
        fprintf(stderr, "No available CPU\n");
        return 1;
    }

    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
        return 1;
    }

    printf("Running on CPU %d\n", cpu);

    /* Process creation and destruction */
    double t = now();

    for (int i = 0; i < N; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0)
            _exit(0);

        if (waitpid(pid, NULL, 0) == -1) {
            perror("waitpid");
            return 1;
        }
    }

    double fk = (now() - t) / N * 1e6;

    /* Thread creation and destruction */
    t = now();

    for (int i = 0; i < N; i++) {
        pthread_t th;
        int err = pthread_create(&th, NULL, nothing, NULL);

        if (err != 0) {
            fprintf(stderr, "pthread_create: %d\n", err);
            return 1;
        }

        err = pthread_join(th, NULL);
        if (err != 0) {
            fprintf(stderr, "pthread_join: %d\n", err);
            return 1;
        }
    }

    double tc = (now() - t) / N * 1e6;

    printf("\nCreation + destruction cost:\n");
    printf("Process (fork + wait): %.2f us\n", fk);
    printf("Thread (create + join): %.2f us\n", tc);
    printf("Process/thread ratio: %.2fx\n", fk / tc);

    char c = 'x';

    /* Process context-switch experiment */
    if (pipe(p1) == -1 || pipe(p2) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        close(p1[1]);
        close(p2[0]);
        echo(NULL);
        close(p1[0]);
        close(p2[1]);
        _exit(0);
    }

    close(p1[0]);
    close(p2[1]);

    t = now();
    int completed = 0;

    for (int i = 0; i < ROUNDS; i++) {
        if (write_byte(p1[1], c) != 0 ||
            read_byte(p2[0], &c) != 0)
            break;
        completed++;
    }

    double ps = completed ?
        (now() - t) / (2.0 * completed) * 1e6 : 0;

    close(p1[1]);
    close(p2[0]);
    waitpid(pid, NULL, 0);

    printf("\nProcess ping-pong:\n");
    printf("Completed rounds: %d\n", completed);
    printf("Average time per switch: %.2f us\n", ps);

    /* Thread context-switch experiment */
    if (pipe(p1) == -1 || pipe(p2) == -1) {
        perror("pipe");
        return 1;
    }

    pthread_t th;
    int err = pthread_create(&th, NULL, echo, NULL);

    if (err != 0) {
        fprintf(stderr, "pthread_create: %d\n", err);
        return 1;
    }

    t = now();
    completed = 0;

    for (int i = 0; i < ROUNDS; i++) {
        if (write_byte(p1[1], c) != 0 ||
            read_byte(p2[0], &c) != 0)
            break;
        completed++;
    }

    double ts = completed ?
        (now() - t) / (2.0 * completed) * 1e6 : 0;

    pthread_join(th, NULL);

    printf("\nThread ping-pong:\n");
    printf("Completed rounds: %d\n", completed);
    printf("Average time per switch: %.2f us\n", ts);

    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]);

    return 0;
}
