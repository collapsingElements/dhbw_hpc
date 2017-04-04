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


void writeVTK2Thread(long timestep, double *data, char pathString[1024], char prefix[1024], int startX, int endX, int startY, int endY, long w, long h, int thread_num) {
  char filename[2048];
  int x,y;

  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);

  snprintf(filename, sizeof(filename), "%s/%s_%05ld-%02d%s", pathString, prefix, timestep, thread_num, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n",
    offsetX, offsetX + w, offsetY, offsetY + h, 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\">\n", startX, endX +1, startY, endY + 1);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</Piece>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = startY; y <= endY; y++) {
    for (x = startX; x <= endX; x++) {
      float value = data[calcIndex(w, x, y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }

  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}

void writeVTK2Main(long timestep, double *data, char pathString[1024], char prefix[1024], long w, long h, int *bounds, int num_threads) {
  char filename[2048];
  int x,y;

  long offsetX=0;
  long offsetY=0;
  float deltax=1.0;
  float deltay=1.0;
  long  nxy = w * h * sizeof(float);

  snprintf(filename, sizeof(filename), "%s/%s-%05ld%s", pathString, prefix, timestep, ".pvti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"PImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<PImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\" GhostLevel=\"0\">\n", offsetX, offsetX + w, offsetY, offsetY + h, 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<PCellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<PDataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</PCellData>\n");

  for (int i = 0; i < num_threads; i ++) {
    fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\" Source=\"%s_%05ld-%02d%s\"/>\n",
    bounds[i * 4], bounds[i * 4 + 1] + 1, bounds[i * 4 + 2], bounds[i * 4 + 3] + 1, prefix, timestep, i, ".vti");
  }

  fprintf(fp,"</PImageData>\n");
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

      // Genau 3 Nachbarn -> Bleibt am Leben/Neues LEben
      if (n == 3) {
        newfield[calcIndex(w, x, y)] = 1;
      }
      // Genau 2 Nachbarn -> Bleib so
      else if (n == 2) {
        newfield[calcIndex(w, x, y)] = currentfield[calcIndex(w, x, y)];
      }
      // Weniger als 2, mehr als 3 NAchbarn => Stirb
      else {
        newfield[calcIndex(w, x, y)] = 0;
      }

    }
  }
}

int countNeighbours(double* currentfield, int x, int y, int w, int h) {
  int n = 0;
  int xl=x-1,xr=x+1,yu=y-1,yo=y+1; // Umliegende Felder
  if (xr == w) xr=0; //Ränder
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

void game() {
  int w, h;
  int* width = &w;
  int* height = &h;
  double *currentfield = readFromFile("gol.seed", width, height);
  double *newfield     = calloc(w*h, sizeof(double));

  long t;
  int startX, startY, endX, endY;
  int xSplit = 2;
  int ySplit = 2;
  int number_of_fields = xSplit * ySplit;
  int *bounds = calloc(number_of_fields * 4, sizeof(int));
  int fieldWidth = (w/xSplit) + (w % xSplit > 0 ? 1 : 0);
  int fieldHeight = (h/ySplit) + (h % ySplit > 0 ? 1 : 0);


  for (t=0;t<TimeSteps;t++) {
    show(currentfield, w, h);

    // 4 Felder -> 4 Threads, jeder Thread bekommt eigene InstanzVariabeln
    #pragma omp parallel private(startX, startY, endX, endY) firstprivate(fieldWidth, fieldHeight, xSplit, ySplit, w, h) num_threads(number_of_fields)
    {

      //Aufteilung der Felder auf die Threads.
      startX = fieldWidth * (omp_get_thread_num() % xSplit);
      endX = (fieldWidth * ((omp_get_thread_num() % xSplit) + 1)) - 1;
      startY = fieldHeight * (omp_get_thread_num() / xSplit);
      endY = (fieldHeight * ((omp_get_thread_num() / xSplit) + 1)) - 1;

      if(omp_get_thread_num() % xSplit == (xSplit - 1)) {
        endX = w - 1;
      }

      if(omp_get_thread_num() / ySplit == (h - 1)) {
        endY = h - 1;
      }

      evolve(currentfield, newfield, startX, endX, startY, endY, w, h);
      writeVTK2Thread(t, currentfield, "/Users/CHeizmann/Documents/workspace_hpc/dhbw_hpc/gameoflife/vti", "gol", startX, endX, startY, endY, w, h, omp_get_thread_num());

      bounds[omp_get_thread_num() * 4] = startX;
      bounds[omp_get_thread_num() * 4 + 1] = endX;
      bounds[omp_get_thread_num() * 4 + 2] = startY;
      bounds[omp_get_thread_num() * 4 + 3] = endY;

    }

    writeVTK2Main(t, currentfield, "/Users/CHeizmann/Documents/workspace_hpc/dhbw_hpc/gameoflife/vti", "gol", w, h, bounds, number_of_fields);

    printf("%ld timestep\n",t);

    usleep(20000);

    //SWAP
    //double *temp = currentfield;
    currentfield = newfield;
    //newfield = temp;
    newfield = calloc(w*h, sizeof(double)); //clear new field
  }

  free(currentfield);
  free(newfield);
  free(bounds);
}

double* readFromFile(char filename[256], int* w, int* h) {
    FILE* file = fopen(filename, "r");

    int size = 10*10;
    int character;
    size_t len = 0;
    size_t width = 0;
    size_t height = 0;
    double* field = calloc(size, sizeof(double));

    while ((character = fgetc(file)) != EOF){
      if (character == '\n') {
        if (!width) width = len;
        height++;
        continue;
      }
      if (character == 'x') field[len++] = 1;
      if (character == '_') field[len++] = 0;
      // resize
      if(len==size){
          field = realloc(field, sizeof(double) * (size += 10));
      }
    }
    height++;

    field = realloc(field, sizeof(*field) * len);
    *w = width;
    *h = height;

    fclose(file);
    return field;
}

int main(int c, char **v) {
  game();
}
