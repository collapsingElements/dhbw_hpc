#include <machine/endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>

#define calcIndex(width, x,y)  ((y)*(width) + (x))

long TimeSteps = 100;

void show(double* currentfield, int w, int h) {
  printf("\033[H");
  int x,y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x,y)] ? "\033[07m  \033[m" : "  ");
    //printf("\033[E");
    printf("\n");
  }
  fflush(stdout);
}


void evolve(double* currentfield, double* newfield, int startX, int endX, int startY, int endY, int w, int h) {
  int x,y;
  for (y = startY; y <= endY; y++) {
    for (x = startX; x <= endX; x++) {

      int n = countNeighbours(currentfield, x, y, w, h);

      //Aktive Zelle mit weniger als 2 Nachbarn: stirb
      if (currentfield[calcIndex(w, x,y)] && n < 2) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
      //Aktive Zelle mit 2 oder 3 Nachbarn: bleib am Leben
      if (currentfield[calcIndex(w, x,y)] && (n == 2 || n == 3)) { newfield[calcIndex(w, x,y)] = currentfield[calcIndex(w, x,y)]; }
      //Aktive Zelle mit mehr als 3 Nachbarn: stirb
      if (currentfield[calcIndex(w, x,y)] && n > 3) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
      //Inaktive Zelle mit genau 3 Nachbarn: Neues Leben!
      if (!currentfield[calcIndex(w, x,y)] && n == 3) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
    }
  }
}

int countNeighbours(double* currentfield, int x, int y, int w, int h) {
  int n = 0;
  int xl=x-1,xr=x+1,yu=y-1,yo=y+1; // Umliegende Felder
  if (xr == w) xr=0; //Randbedingungen
  if (yo == h) yo=0;
  if (xl < 0) xl=w-1;
  if (yu < 0) yu=h-1;

  if (currentfield[calcIndex(w, xl,yo)]) n++; //links oben
  if (currentfield[calcIndex(w, x,yo)]) n++; //oben
  if (currentfield[calcIndex(w, xr,yo)]) n++; //rechts oben
  if (currentfield[calcIndex(w, xr,y)]) n++; // rechts
  if (currentfield[calcIndex(w, xr,yu)]) n++; // rechts unten
  if (currentfield[calcIndex(w, x,yu)]) n++; // unten
  if (currentfield[calcIndex(w, xl,yu)]) n++; // links unten
  if (currentfield[calcIndex(w, xl,y)]) n++; // links
  return n;
}

void filling(double* currentfield, int w, int h) {
  int i;
  for (i = 0; i < h*w; i++) {
    currentfield[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
  }
}

void game(int w, int h) {
  double *currentfield = calloc(w*h, sizeof(double));
  double *newfield     = calloc(w*h, sizeof(double));

  //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));

  filling(currentfield, w, h);
  long t;
  int startX, startY, endX, endY;
  int xSplit = 2; // 4 Threads
  int ySplit = 2;
  int numberOfFields = xSplit * ySplit;
  int fieldWidth = (w/xSplit) + (w % xSplit > 0 ? 1 : 0);
  int fieldHeight = (h/ySplit) + (h % ySplit > 0 ? 1 : 0);

  for (t=0;t<TimeSteps;t++) {
    show(currentfield, w, h);

    // 4 Felder -> 4 Threads, jeder Thread bekommt eigene InstanzVariabeln
    #pragma omp parallel private(startX, startY, endX, endY) firstprivate(fieldWidth, fieldHeight, xSplit, ySplit, w, h) num_threads(numberOfFields)
    {

      //Aufteilung der Felder auf die Threads.
      startX = fieldWidth * (omp_get_thread_num() % xSplit);
      startY = fieldHeight * (omp_get_thread_num() / xSplit);
      endX = (fieldWidth * ((omp_get_thread_num() % ySplit) + 1)) - 1;
      endY = (fieldHeight * ((omp_get_thread_num() / ySplit) + 1)) - 1;

      if(omp_get_thread_num() % xSplit == (xSplit - 1)) {
        endX = w - 1;
      }

      if(omp_get_thread_num() / ySplit == (h - 1)) {
        endY = h - 1;
      }

      evolve(currentfield, newfield, startX, endX, startY, endY, w, h);
    }

    printf("%ld timestep\n",t);

    usleep(200000);

    //SWAP
    //double *temp = currentfield;
    currentfield = newfield;
    //newfield = temp;
    newfield = calloc(w*h, sizeof(double)); //clear new field
  }

  free(currentfield);
  free(newfield);

}

int main(int c, char **v) {
  int w = 0, h = 0;
  if (c > 1) w = atoi(v[1]); ///< read width
  if (c > 2) h = atoi(v[2]); ///< read height
  if (w <= 0) w = 30; ///< default width
  if (h <= 0) h = 30; ///< default height
  game(w, h);
}
