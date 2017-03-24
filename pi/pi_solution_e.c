#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <omp.h>

#define TRYS 5000000

static int throw() {
  double x, y;
  x = (double)rand() / (double)RAND_MAX;
  y = (double)rand() / (double)RAND_MAX;
  if ((x*x + y*y) <= 1.0) return 1;

  return 0;
}

int main(int argc, char **argv) {
  int globalCount = 0, globalSamples=TRYS;

  /*
        1. Wie können Sie dies den Benutzer steueren lassen?
            1.1 Der User kann beim Ausführen den Parametere OMP_NUM_THREADS=%THREAD_COUNT% mitgeben um zu definieren, mit wie vielen Threads das Programm laufen soll.
        2. Wie können Sie dies den Benutzer unterbinden?
            2.2 Man kann in der #pragma omp parallel die optionale Anweisung num_threads(6) anfügen, die festlegt mit wie vielen Threads der parallele Teil durchgeführt wird
  */
  #pragma omp parallel reduction( +: globalCount) num_threads(6)
  {
    #pragma omp for
    for(int i = 0; i < globalSamples; ++i) {
  		globalCount += throw();
    }

    printf("Thread %d: treffer %d\n", omp_get_thread_num(), globalCount);
  }
  double pi = 4.0 * (double)globalCount / (double)(globalSamples);

  printf("pi is %.9lf\n", pi);

  return 0;
}
