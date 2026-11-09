/**
 * @file pgm.cpp
 * @brief Implementation of the PGM image writer declared in pgm.h.
 */
#include "pgm.h"
#include <filesystem>

int init_output(const char *folder) {
  std::error_code ec;
  std::filesystem::create_directories(folder, ec);
  if (ec) {
    printf("Nie moge utworzyc folderu %s: %s\n", folder, ec.message().c_str());
    return 1;
  }
  return 0;
}

/**
 * @brief Writes a square array of doubles as a binary PGM (P5) image.
 *
 * Every value is divided by @p vmax, clamped to [0, 1] and scaled to a byte
 * in 0..255. Pixels are written row by row, the same order in which a C 2D
 * array is stored in memory.
 *
 * @param path Path of the file to create (overwritten if it already exists).
 * @param data Pointer to the first of size * size values.
 * @param size Width (and height) of the image in pixels.
 * @param vmax Value that becomes white (1.0 for the board, the maximum
 *             weight for the mask).
 * @return 0 on success, 1 if the file could not be opened.
 */
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

int save_board_frame(const char *folder, int frame_number,
                     const double *current_board_prt, size_t size_fo_board) {
  char path[512];
  snprintf(path, sizeof(path), "%s/frame_%05d.pgm", folder, frame_number);
  return save_pgm(path, current_board_prt, size_fo_board, 1.0);
}