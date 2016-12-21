//=========================================//
//  Name:     William Young                //
//  File:     hw1.c                        //
//  Assgnmnt: Homework 1                   //
//  Date:     26 September 2016            //
//  Course:   CS6354 Computer Architecture //
//  Dscrptn:  Memory Hierarchy Benchmarks  //
//=========================================//

/* RESOURCES
   1)  CACHE SIZE - http://stackoverflow.com/questions/12675092/writing-a-program-to-get-l1-cache-line-size
   2)  POINTER CHASING - http://faculty.smcm.edu/acjamieson/s10/COSC251Lab2ish.pdf
   3)  POINTER CHASING - http://www.ece.umd.edu/courses/enee759h.S2003/lectures/LabDemo1.pdf
   4)  CACHE SIZE; POINTER CHASING - http://igoro.com/archive/gallery-of-processor-cache-effects/
   5)  CACHE SIZE - http://www.cplusplus.com/forum/general/35557/
   6)  BANDWIDTH - https://www.cs.virginia.edu/stream/FTP/Code/
   7)  TLB - lmbench /src/tlb.c
*/

/* REQUIREMENTS
   1)  Y  Data cache sizes
   2)  Y  Data cache latencies
   3)  Y  Data cache associativity
   4)  Y  Data cache throughput
   5)  Y  Data cache block size
   6)  Y  Data TLB size
   7)  N  Data TLB associativity
   8)  N  Instruction cache size
   9)  Y  Main Mem bandwidth (random)
   10) Y  Main Mem bandwidth (sequential)
   11) Y  Main Mem latency
   12) N  Prefetching?
   13) Y  Large page TLB size
   14) Y  Multi-core throughput
*/

// INLCLUDE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <x86intrin.h>
#include <stdint.h>
#include "src.h"

// MACROS
// (inspired by lat_mem_rd.c from lmbench)
#define ONE c = (char**) *c;
#define FIVE ONE ONE ONE ONE ONE
#define TEN FIVE FIVE
#define FIFTY TEN TEN TEN TEN TEN
#define HUNDRED FIFTY FIFTY

// CONSTANTS
#define MAX_ARRAY_SIZE 256 * 1024 * 1024 // 256MB
#define MAX_STRIDE_LEN 512 // 8KB
#define PAGE_SIZE 4096 // 4KB
#define LRG_PAGE_SIZE 4 * 1024 * 1024 // 4MB
#define AGGREGATE 1E9
#define LATENCY 1E6

// STRUCTS
typedef struct bigblock {
  double a;
  double b;
  double c;
  double d;
} bigblock;

// FUNC PROTOTYPES
void mem_lat_benchmark(int, int, int, register int);
int cache_size_test(void);
int cache_line_test(void);
int tlb_test(void);
int tlb_large_test(void);
int bw_test(register int, int);
int cache_assoc_test(void);
int cache_lat_test(void);
int mem_lat_test(void);
int prefetch_test(void);
void multi_core_bw(void);
void* bw_helper(void*);
int icache_test(void);

// MAIN
int main(int argc, char** argv, char** env) {
  int fd = open("results.txt", O_CREAT | O_WRONLY | O_TRUNC, 0600);
  dup2(fd, 1);

  fprintf(stdout, "\n  MEMORY HIERARCHY BENCHMARK SUITE  \n====================================\n");

  // Block size
  cache_line_test();

  // TLB entries
  tlb_test();
  tlb_large_test();

  // Random memory throughput
  fprintf(stdout, "\nMEMORY THROUGHPUT (RANDOM)\n\n");
  fprintf(stdout, "Test Size\tThroughput\n---------\t----------\n");
  srand(time(NULL));
  bw_test(512*1024, 1); // mem rand

  // Sequential memory throughput
  fprintf(stdout, "\nMEMORY THROUGHPUT (SEQUENTIAL)\n\n");
  fprintf(stdout, "Test Size\tThroughput\n---------\t----------\n");
  bw_test(512*1024, 0); // mem seq

  // Multi-core bandwidth
  multi_core_bw();

  // Cache throughput
  fprintf(stdout, "\nCACHE THROUGHPUT\n\n");
  fprintf(stdout, "Test Size\tThroughput\n---------\t----------\n");
  bw_test(1024, 0); // L1
  bw_test(8*1024, 0); // L2
  bw_test(256*1024, 0); // L3?

  // Cache latency
  cache_lat_test();

  // Memory latency
  mem_lat_test();

  // Associativity
  cache_assoc_test();

  // Instruction Cache
  icache_test();

  // Prefetching Test
  //prefetch_test();

  // Cache Size
  cache_size_test();

  close(fd);
  return 0;
}



// INSTRUCTION CACHE TEST
int icache_test(void) {
  fprintf(stdout, "\nINSTRUCTION CACHE TEST (not fully working)\n\n");
  long long start, total;

  fprintf(stdout, "Target Size\tTime Stamp\n-----------\t----------\n");
  for(int i = 1; i <= 64; i *= 2) { // test 2KB, 4KB, ... 128KB
    start = (long long) read_tsc();
    for(int j = 0; j < i*128; j++) {
      __asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
    }
    total = (long long) read_tsc() - start;
    fprintf(stdout, "%d KB\t\t%llu\n", i*2, (unsigned long long)(total));
  }
  return 0;
}




// PREFETCHING TEST
int prefetch_test(void) {
  register int sum, i, len = 1024*1024;
  char* arr = malloc(sizeof(char)*len);
  long long time, start;

  fprintf(stdout, "\nPREFETCHING TEST\n\nStride\tAccess Time\n------\t-----------\n");
  for(int j = 1; j <= 256; j *= 2) {
    start = precision_timer();
    for(i = 0; i < len; i++) {
      sum += arr[(i * j) % len]; }
    time = precision_timer() - start;
    fprintf(stdout, "%d\t%1.5f\n", j, (double)time/1000000000);
  }

  FILE* file = fopen("/dev/null", "w");
  fprintf(file, "%d", sum);
  fclose(file);

  free(arr);

  return 0;
}





// CACHE LATENCY TEST
int cache_lat_test(void) {
  fprintf(stdout, "\nCACHE LATENCY TEST\n\n");
  fprintf(stdout, "Target\tArray Size\tLatency (ns)\n------\t----------\t-------------\n");
  // vary length by size of cache to observe latency
  int test[] = {32*1024, 256*1024, 4*1024*1024};
  for(int i = 0; i < 3; i++) {
    fprintf(stdout, "%dKB\t", test[i]/1024);
    mem_lat_benchmark(4096, test[i], LATENCY, 1000000); }
  return 0;
}






// MEMORY LATENCY TEST
int mem_lat_test(void) {
  fprintf(stdout, "\nMAIN MEMORY LATENCY TEST\n\n");
  fprintf(stdout, "Array Size\t\tLatency (ns)\n----------\t\t------------\n");
  mem_lat_benchmark(4096, 128*1024*1024, LATENCY, 1000000);
  return 0;
}




// CACHE ASSOCIATIVITY TEST
int cache_assoc_test(void) {

  fprintf(stdout, "\nCACHE ASSOCIATIVITY TEST\n\n");
  int stride[] = {32*1024, 256*1024};
  for(int i = 0; i < 2; i++) {

    // Associativity (size/stride[i]) in this case referes to the number
    // of unique items accessed repeatedly. At first, with size = stride,
    // only one item will be continuously accessed.  As size grows,
    // the number of items accessed repeatedly will double. when we surpass
    // the level of associativity of the cache, we should observe an increase
    // in latency.
    fprintf(stdout, "Target cache: %dKB\n", stride[i]/1024);
    fprintf(stdout, "Assoc\tArray Size\tAccess Time\n-----\t----------\t-----------\n");

    for(int size = stride[i]; size <= 64*stride[i]; size *= 2) {
      fprintf(stdout, "%d\t", size/stride[i]);
      mem_lat_benchmark(stride[i], size, AGGREGATE, 10000000); }

  }
  return 0;
}








// CACHE BLOCK TEST
int cache_line_test(void) {
  fprintf(stdout, "\nCACHE BLOCK SIZE TEST\n\nBlock\tArray Size\tAccess Time\n-----\t----------\t-----------\n");
  for(int i = 16; i <= 512; i += 4) {
    fprintf(stdout, "%d\t", i);
    mem_lat_benchmark(i, 8*1024*1024, AGGREGATE, 1000000); }
  return 0; }








// TLB ENTRY SIZE TEST
/* General Idea: We don't know how many entries the TLB holds, but we DO
   know that we can find it by modifying our array size to be slightly larger
   than expected page sizes.  (We know the page size is 4KB.)  When the array
   size crosses the boundary between the maximum possible number of entries,
   we will observe a jump in latency.                                      */
int tlb_test(void) {
  fprintf(stdout, "\nTLB ENTRY TEST\n\nEntries\tArray Size\tAccess Time\n-------\t----------\t-----------\n");
  //access one page beyond possible TLB to cause a miss and load
  // i represents the number of pages being tested
  for(int i = 4; i < 4096; i*=2) {
    fprintf(stdout, "%d\t", i*2);
    // we expect the page
    mem_lat_benchmark(PAGE_SIZE, (i+1)*PAGE_SIZE, AGGREGATE, 1000000); }
  return 0; }





// LARGE PAGE TLB ENTRY SIZE TEST
int tlb_large_test(void) {
  fprintf(stdout, "\nTLB LARGE PAGE ENTRY TEST\n\nEntries\tArray Size\tAccess Time\n-------\t----------\t-----------\n");
  for(int i = 2; i <= 128; i*=2) {
    fprintf(stdout, "%d\t", i*2);
    mem_lat_benchmark(LRG_PAGE_SIZE, (i+1)*LRG_PAGE_SIZE, AGGREGATE, 1000000); }
  return 0; }



// CACHE SIZE TEST
int cache_size_test(void) {
  fprintf(stdout, "\nCACHE SIZE TEST\n\n");
  for(int i = 32 ; i < MAX_STRIDE_LEN ; i *= 2) {
    fprintf(stdout, "Stride: %d\nArray Size\tAccess Time\n----------\t-----------\n", i);
    for(int j = 1024 ; j < MAX_ARRAY_SIZE ; j += array_step(j)) {
      mem_lat_benchmark(i, j, AGGREGATE, 1000000); } }
  return 0; }







// CACHE BENCHMARK
void mem_lat_benchmark(int stride, int size, int flag, register int REPS){
  // INITIALZIE VARIABLES
  register int i;
  long long start, total; // timer variables

  // SETUP POINTER CHASING
  char* arr = malloc(size * sizeof(char)); // worker array
  char** head_node = (char**)arr; // head of linked list should point to first item
  register char** c = head_node; // c should point to head of linked list; this is our pointer
  for(int k = 0; k < size; k += stride) {
    (*c) = &arr[k + stride]; // pointer value = address of next node to visit
    c += (stride/sizeof(c)); } // move pointer to next node
  *c = (char*) head_node; // don't forget the final node! make sure we loop back around

  // POINTER CHASING
  start = precision_timer();
  for(i = 0; i < REPS; i++)
    HUNDRED; // do 100 chases
  total = (precision_timer() - start);

  // OUTPUT and CLEANUP
  fprintf(stdout, "%1.4f\t\t%1.4f\n", (double)size/(1024*1024), (double)(total)/flag);
  free(arr);
}








// THROUGHPUT
int bw_test(register int len, int rnd) {
  register int i, r, j = 0;
  register int iters = 2;
  double size_in_MB = (double)((double)len * sizeof(bigblock)/(1024*1024));
  bigblock* a = malloc(len * sizeof(bigblock));
  bigblock* b = malloc(len * sizeof(bigblock));
  long long time, start, *times, rt;
  times = (long long*)malloc(sizeof(long long)*iters*len);

  // initialize
  for(i = 0; i < len; i++) {
    a[i].a = 1.0;
    a[i].b = 1.0;
    a[i].c = 1.0;
    a[i].d = 1.0;
    b[i].a = 2.0;
    b[i].b = 2.0;
    b[i].c = 2.0;
    b[i].d = 2.0;
  }

  if(rnd == 1) {
    start = precision_timer();
    r = (rand()*iters) & (len-1);
    rt = precision_timer() - start; }

  // measurements
  while(j++ < iters) {
    register bigblock* x = a;
    register bigblock* y = b;
    a = b;
    b = x;

    if(rnd == 1) {
      start = precision_timer();
      for(i = 0; i < len; i++) {
	r = rand();
	x[(i * r) & (len-1)] = y[(i * r) & (len-1)]; }
      times[j] = (precision_timer() - start - rt);
    }
    else {
      start = precision_timer();
      for(i = 0; i < len; i++) {
	x[i] = y[i]; }
      times[j] = (precision_timer() - start);
    }
  }

  // output: mbytes/sec
  time = arr_avg_long(times, iters);
  fprintf(stdout, "%dKB\t\t%1.1f MB/s\n", (int)(size_in_MB*1024), (size_in_MB/((float)(time)/1000000000)));

  free(a);
  free(b);

  return 0;
}

void multi_core_bw(void) {
  fprintf(stdout, "\nMULTICORE BANDWIDTH\n\nTwo cores:\n");
  int r = 0;
  pthread_t thread1, thread2;
  pthread_create(&thread1, NULL, bw_helper, (void*)r);
  pthread_create(&thread2, NULL, bw_helper, (void*)r);
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
}

void* bw_helper(void* r) {
  register int i;
  register int j = 0;
  register int iters = 2;
  register int len = 512 * 1024;
  double size_in_MB = (double)((double)len * sizeof(bigblock)/(1024*1024));
  bigblock* a = malloc(len * sizeof(bigblock));
  bigblock* b = malloc(len * sizeof(bigblock));
  long long time, start, *times;
  times = (long long*)malloc(sizeof(long long)*iters*len);

  // initialize
  for(i = 0; i < len; i++) {
    a[i].a = 1.0;
    a[i].b = 1.0;
    a[i].c = 1.0;
    a[i].d = 1.0;
    b[i].a = 2.0;
    b[i].b = 2.0;
    b[i].c = 2.0;
    b[i].d = 2.0;
  }

  // measurements
  while(j++ < iters) {
    register bigblock* x = a;
    register bigblock* y = b;
    a = b;
    b = x;
    start = precision_timer();
    for(i = 0; i < len; i++) {
      x[i] = y[i]; }
    times[j] = (precision_timer() - start);
  }

  // output: bytes/sec
  time = arr_avg_long(times, iters);
  fprintf(stdout, "%dKB\t\t%1.1f MB/s\n", (int)(size_in_MB*1024), (size_in_MB/((float)(time)/1000000000)));

  free(a);
  free(b);

  pthread_exit(NULL);
}
