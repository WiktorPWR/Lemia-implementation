#include "pgm.h"

// Tworzy folder na wyniki (razem z nadrzędnymi), jeśli jeszcze nie istnieje.
// Zwraca 0, gdy wszystko OK, 1 przy błędzie.
int init_output(const char *folder) {
  std::error_code ec;
  std::filesystem::create_directories(folder, ec);
  if (ec) {
    printf("Nie moge utworzyc folderu %s: %s\n", folder, ec.message().c_str());
    return 1;
  }
  return 0;
}

// Zapisuje tablicę size x size jako obrazek PGM (format P5, binarny).
// data - wskaźnik na pierwszy element tablicy, np. &current_board[0][0]
// vmax - wartość, która ma być biała (plansza: 1.0, maska: jej maksimum)
static int save_pgm(const char *path, const double *data, int size,
                    double vmax) {
  FILE *f = fopen(path, "wb"); // "wb" = binarnie, ważne na Windowsie
  if (f == NULL) {
    printf("Nie moge otworzyc pliku %s\n", path);
    return 1;
  }

  fprintf(f, "P5\n%d %d\n255\n", size, size); // nagłówek

  for (int k = 0; k < size * size; k++) { // piksele wiersz po wierszu
    double v = data[k] / vmax;
    if (v < 0.0)
      v = 0.0;
    if (v > 1.0)
      v = 1.0;
    unsigned char pixel = (unsigned char)(v * 255.0 + 0.5);
    fputc(pixel, f);
  }

  fclose(f);
  return 0;
}

// Zapisuje aktualny stan planszy jako folder/frame_00000.pgm, frame_00001.pgm,
// ...
int save_board_frame(const char *folder, int frame_number,
                     const double *current_board_prt, size_t size_fo_board) {
  char path[512];
  snprintf(path, sizeof(path), "%s/frame_%05d.pgm", folder, frame_number);
  return save_pgm(path, current_board_prt, size_fo_board, 1.0);
}