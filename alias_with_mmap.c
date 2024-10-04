#define _GNU_SOURCE
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <sched.h>

#ifndef O_TMPFILE
#define O_TMPFILE (__O_TMPFILE | O_DIRECTORY)
#endif

uint8_t buf[4096] = {0};

#define NREGIONS 32
#define BASE 0x1000000UL

int* ptrs[NREGIONS] = {NULL};

#define TRIES ((1<<20)-7)

static void reset_counter(int fd) {
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
}
static void enable_counter(int fd) {
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}
static void disable_counter(int fd) {
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}
static int cycle_counter() {
  int fd = 0;
  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(pe));
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(pe);
  pe.config = PERF_COUNT_HW_CPU_CYCLES;
  pe.disabled = 1;
  //pe.exclude_kernel = 1;
  //pe.exclude_hv = 1;
  int cpu = sched_getcpu();
  fd = syscall(__NR_perf_event_open, &pe, 0, cpu, -1, 0);
  if(fd < 0) perror("WTF");
  assert(fd > -1);
  return fd;
}

static uint64_t read_cycles(int fd) {
  uint64_t count = 0;
  ssize_t rc = read(fd, &count, sizeof(count));
  assert(rc != -1);
  return count;
}


int main(int argc, char *argv[]) {
  int *b = (int*)buf;
  int idx = 0, t= 0, iters = TRIES;  
  int cfd,fd,pgsz;
  pgsz = getpagesize();
  
  if(pgsz != 4096) {
    printf("this micro assumes a 4k page and this machine is using %d byte pages\n", pgsz);
    return -1;
  }
  
  fd = open(".", O_RDWR|O_TMPFILE, 0600);
  cfd = cycle_counter();
  
  for(int i = 0; i < 1024; i++) {
    b[i] = (i+1) & (1023);
  }
  assert(write(fd, buf, sizeof(buf)) == sizeof(buf));

  for(int i = 0; i < NREGIONS; i++) {
    ptrs[i] = (int*)mmap((void*)(BASE+(4096*i)), 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    assert(ptrs[i] != ((void*)-1L));
  }
  enable_counter(cfd);
  reset_counter(cfd);
  uint64_t now = read_cycles(cfd);
  while(iters) {
    idx = ptrs[t][idx];
    t = (t+1) & (NREGIONS-1);
    --iters;
  }
  now = read_cycles(cfd) - now;  
  printf("idx = %d, took %lu cycles\n", idx, now);
  printf("%g cycles per load\n", ((double)now) / TRIES);
  for(int i = 0; i < NREGIONS; i++) {
    munmap(ptrs[i], 4096);
  }
  
  close(fd);
  close(cfd);
	      
  return 0;
}
