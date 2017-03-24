#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>

int main(int argc, char **argv) {

  #pragma omp parallel
  printf("Hello World from Thread %d of %d\n", omp_get_thread_num(), omp_get_num_threads());

  return 0;
}
