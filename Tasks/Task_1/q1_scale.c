#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
/* Create 2000 idle sessions as processes, then as threads; report memory. */
static long meminfo(const char*k){FILE*f=fopen("/proc/meminfo","r");char l[128];long v=0;while(fgets(l,sizeof l,f))if(!strncmp(l,k,strlen(k)))v=atol(l+strlen(k));fclose(f);return v;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void*idle(void*a){char buf[4096];memset(buf,1,sizeof buf);sleep(4);return a;}
int main(void){
  int N=2000; static pid_t k[2000];
  char *heap=malloc(8<<20); memset(heap,1,8<<20);   /* server state, 8 MB */
  long a=meminfo("MemAvailable:"); double t=now();
  for(int i=0;i<N;i++){k[i]=fork(); if(k[i]==0){char buf[4096];memset(buf,1,sizeof buf);heap[i*4096]=2;sleep(4);_exit(0);}}
  double ct=now()-t; sleep(2); long b=meminfo("MemAvailable:");
  printf("2000 processes: created in %.3f s, memory used %.1f MB (%.0f KB/session)\n",ct,(a-b)/1024.0,(double)(a-b)/N);
  for(int i=0;i<N;i++)waitpid(k[i],NULL,0);
  sleep(1);
  static pthread_t th[2000]; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at,64*1024);
  a=meminfo("MemAvailable:"); t=now();
  for(int i=0;i<N;i++)pthread_create(&th[i],&at,idle,NULL);
  ct=now()-t; sleep(2); b=meminfo("MemAvailable:");
  printf("2000 threads (64 KB stacks): created in %.3f s, memory used %.1f MB (%.0f KB/session)\n",ct,(a-b)/1024.0,(double)(a-b)/N);
  for(int i=0;i<N;i++)pthread_join(th[i],NULL);
  return 0;}
