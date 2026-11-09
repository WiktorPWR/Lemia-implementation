// main.cpp - punkt startowy programu
#include "pgm.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define SIZE_OF_BOARD 256
#define RADIUS 13
#define SEED 42
#define NUMBER_OF_INTERATION 1000
#define NUMBER_OF_RINGS 1
#define SIZE_OF_MASK (RADIUS * 2 + 1)
#define SIZE_OF_SQUARE 40

const char *OUTPUT_FOLDER = "Results/";

#define SIGMA 0.043
#define MU 0.3
#define DLUGOSC_KROKU 0.1

const double height_of_section[NUMBER_OF_RINGS] = {
    1.0}; // to  jest wysokosc pierscienia
double current_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
double new_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
double mask[SIZE_OF_MASK][SIZE_OF_MASK];

void init_board(void);
void init_mask(void);

int main() {
  init_board();
  init_mask();
  // Zapisujemy aktualny stan planszy jako obrazek PGM
  if (init_output(OUTPUT_FOLDER) != 0) {
    return 1; // błąd przy tworzeniu folderu
  }

  // Idąc po naszych iteracjach
  for (int iteration = 0; iteration < NUMBER_OF_INTERATION; iteration++) {
    // Przeprowadzamy aktualizację planszy
    for (int x = 0; x < SIZE_OF_BOARD; x++) {
      for (int y = 0; y < SIZE_OF_BOARD; y++) {
        new_board[x][y] = 0.0;
        double U = 0.0;
        // Dla kazdje komorki liczymy jej nowa wartosc na podstawei  maski
        for (int i = 0; i < SIZE_OF_MASK; i++) {
          for (int j = 0; j < SIZE_OF_MASK; j++) {
            int x_index = (x + i - RADIUS + SIZE_OF_BOARD) % SIZE_OF_BOARD;
            int y_index = (y + j - RADIUS + SIZE_OF_BOARD) % SIZE_OF_BOARD;
            U += current_board[x_index][y_index] * mask[i][j];
          }
        }
        double G = (2 * exp((-pow((U - MU), 2)) / (2 * SIGMA * SIGMA))) - 1;
        double before_cut_value = current_board[x][y] + DLUGOSC_KROKU * G;
        if (before_cut_value > 1.0) {
          new_board[x][y] = 1.0;
        } else if (before_cut_value < 0.0) {
          new_board[x][y] = 0.0;
        } else {
          new_board[x][y] = before_cut_value;
        }
      }
    }

    // Przenosimy nowa plansze na stara
    for (int i = 0; i < SIZE_OF_BOARD; i++) {
      for (int j = 0; j < SIZE_OF_BOARD; j++) {
        current_board[i][j] = new_board[i][j];
      }
    }

    if (iteration % 10 == 0) {
      if (save_board_frame(OUTPUT_FOLDER, iteration, &current_board[0][0],
                           SIZE_OF_BOARD) != 0) {
        return 1; // błąd przy zapisie pliku
      }
    }
  }
  return 0;
}

// Funckja zwraca nam nie  jako kształt garbu
//  Czyli wartosc jaks przyjmuje wedlug wzoru
inline double calculate_kc(double r) {
  if (r >= 1 || r <= 0) {
    return 0;
  } else {
    return exp(4 - 1 / (r * (1 - r)));
  }
}

inline double calcualte_ks(double r) {
  if (r >= 1) {
    return 0;
  } else {
    double Br = NUMBER_OF_RINGS * r; // odleglosc ale w odcinakch
    double p = floor(Br);            // numer odcinak
    double rr = Br - p;              // miejsce w tym ze odcinku
    return height_of_section[(int)p] *
           calculate_kc(rr); // zwraca wagę tej że komorki
  }
}

void init_mask(void) {
  double sum = 0;
  for (int i = 0; i < SIZE_OF_MASK; i++) {
    for (int j = 0; j < SIZE_OF_MASK; j++) {
      double r = sqrt(pow(i - RADIUS, 2) + pow(j - RADIUS, 2)) / RADIUS;
      mask[i][j] = calcualte_ks(r);
      sum += mask[i][j];
    }
  }
  printf("Suma wag: %f\n", sum);
  // normlaizacja
  if (sum == 0) {
    exit(1);
    // error
  }
  for (int i = 0; i < SIZE_OF_MASK; i++) {
    for (int j = 0; j < SIZE_OF_MASK; j++) {
      mask[i][j] = mask[i][j] / sum;
    }
  }
}

void init_board(void) {
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    for (int j = 0; j < SIZE_OF_BOARD; j++) {
      current_board[i][j] = 0.0;
      new_board[i][j] = 0.0;
    }
  }
  srand(SEED);
  int start = SIZE_OF_BOARD / 2 - SIZE_OF_SQUARE / 2; // 128 - 20 = 108
  for (int i = start; i < start + SIZE_OF_SQUARE; i++) {
    for (int j = start; j < start + SIZE_OF_SQUARE; j++) {
      current_board[i][j] = (double)rand() / RAND_MAX; // [0, 1]
    }
  }
}
