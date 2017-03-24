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

void writeVTK2(long timestep, double *data, char prefix[1024], int xStart, int xEnd, int yStart, int yEnd, long w, long h, int thread_num) {
  char filename[2048];
  int x,y;

  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);

  snprintf(filename, sizeof(filename), "%s-%05ld-%02d%s", prefix, timestep, thread_num, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX, offsetX + (xEnd - xStart), offsetY, offsetY + (yEnd - yStart), 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = yStart; y <= yEnd; y++) {
    for (x = xStart; x <= xEnd; x++) {
      float value = data[calcIndex(h, x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }

  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}


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

      if (currentfield[calcIndex(w, x,y)] && n < 2) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
      if (currentfield[calcIndex(w, x,y)] && (n == 2 || n == 3)) { newfield[calcIndex(w, x,y)] = currentfield[calcIndex(w, x,y)]; }
      if (currentfield[calcIndex(w, x,y)] && n > 3) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
      if (!currentfield[calcIndex(w, x,y)] && n == 3) { newfield[calcIndex(w, x,y)] = !currentfield[calcIndex(w, x,y)]; }
    }
  }
}

int countNeighbours(double* currentfield, int x, int y, int w, int h) {
  int n = 0;
  int xl=x-1,xr=x+1,yu=y-1,yo=y+1;
  if (xr == w) xr=0;
  if (yo == h) yo=0;
  if (xl < 0) xl=w-1;
  if (yu < 0) yu=h-1;
  if (currentfield[calcIndex(w, xl,yo)]) { n++; }
  if (currentfield[calcIndex(w, x,yo)]) { n++; }
  if (currentfield[calcIndex(w, xr,yo)]) { n++; }
  if (currentfield[calcIndex(w, xr,y)]) { n++; }
  if (currentfield[calcIndex(w, xr,yu)]) { n++; }
  if (currentfield[calcIndex(w, x,yu)]) { n++; }
  if (currentfield[calcIndex(w, xl,yu)]) { n++; }
  if (currentfield[calcIndex(w, xl,y)]) { n++; }
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
  int xFactor = 2;  // :-)
  int yFactor = 2;
  int number_of_areas = xFactor * yFactor;
  int fieldWidth = (w/xFactor) + (w % xFactor > 0 ? 1 : 0);
  int fieldHeight = (h/yFactor) + (h % yFactor > 0 ? 1 : 0);

  for (t=0;t<TimeSteps;t++) {
    show(currentfield, w, h);

    #pragma omp parallel private(startX, startY, endX, endY) firstprivate(fieldWidth, fieldHeight, xFactor, yFactor, w, h) num_threads(number_of_areas)
    {
      startX = fieldWidth * (omp_get_thread_num() % xFactor);
      endX = (fieldWidth * ((omp_get_thread_num() % xFactor) + 1)) - 1;
      startY = fieldHeight * (omp_get_thread_num() / xFactor);
      endY = (fieldHeight * ((omp_get_thread_num() / xFactor) + 1)) - 1;

      if(omp_get_thread_num() % xFactor == (xFactor - 1)) {
        endX = w - 1;
      }

      if(omp_get_thread_num() / xFactor == (h - 1)) {
        endY = h - 1;
      }

      evolve(currentfield, newfield, startX, endX, startY, endY, w, h);
      writeVTK2(t,currentfield,"gol", startX, endX, startY, endY, w, h, omp_get_thread_num());
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
