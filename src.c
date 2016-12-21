#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <time.h>
#include "src.h"
  
// provides our graph with plenty of good data points
// inspired by LMBENCH
int array_step(int size) {
  if(size < 4096)
    return 1024;
  else if(size < (32 * 1024))
    return 4096;
  else if(size < (128 * 1024))
    return 16384;
  else if(size < (512 * 1024))
    return (64 * 1024);
  else if(size < (1024 * 1024))
    return (128 * 1024);
  else if(size < (4 * 1024 * 1024))
    return (256 * 1024);
  else if(size < (8 * 1024 * 1024))
    return (512 * 1024);
  else if(size < (16 * 1024 * 1024))
    return (2 * 1024 * 1024);
  else
    return (32 * 1024 * 1024);
}
 
long long precision_timer() {
  // reference:  Linux man page for clock_gettime
  struct timespec timer;
  clock_gettime(CLOCK_MONOTONIC_RAW, &timer);
  return (long long)(timer.tv_nsec + (long long)timer.tv_sec * 1000000000);
}

long long arr_avg_long(long long* arr, int num_items) {
  long long sum = 0;
  for(int i = 0; i < num_items; i++) {
    sum += arr[i];
  }
  return (long long)(sum/num_items);
}

unsigned long read_tsc(void) {
  unsigned long hi, lo;
  __asm__ volatile ( "rdtsc" : "=a"(lo), "=d"(hi));
  return lo;
}
