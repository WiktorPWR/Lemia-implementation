/**
 * @file main.cpp
 * @brief CPU implementation of Lenia, a continuous cellular automaton.
 *
 * The board is a SIZE_OF_BOARD x SIZE_OF_BOARD grid of cells with values in
 * [0, 1] whose edges wrap around (a torus). Every step convolves the board
 * with a ring-shaped kernel (::mask), passes the result through a Gaussian
 * growth function and adds a small part of it to every cell. Every
 * SAVE_EVERY-th step is saved as a PGM image to ::FRAMES_FOLDER, together with
 * a SETTINGS_FILE describing the parameters and the result of the run.
 */
#include "pgm.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define SIZE_OF_BOARD 256
#define RADIUS 13
#define SEED 42
#define NUMBER_OF_INTERATION 1000
#define NUMBER_OF_RINGS 1
#define SIZE_OF_MASK (RADIUS * 2 + 1)
#define SIZE_OF_SQUARE 40

/// Folder for the settings and result files, relative to the working
/// directory.
const char *OUTPUT_FOLDER = "Results/";
/// Subfolder of ::OUTPUT_FOLDER for the PGM frames (keeps Results/ tidy).
const char *FRAMES_FOLDER = "Results/frames/";

#define SIGMA 0.04565995
#define MU 0.3

#define DLUGOSC_KROKU 0.1

/// Total board mass (sum of all cells) below which life counts as extinct
/// and the simulation stops early.
#define EXTINCTION_MASS 1e-9

/// A frame is saved every SAVE_EVERY iterations.
#define SAVE_EVERY 10

/// Text file with the settings and the result, written to ::OUTPUT_FOLDER.
#define SETTINGS_FILE "settings.txt"

/// Height (peak weight) of each kernel ring, from the centre outwards.
const double height_of_section[NUMBER_OF_RINGS] = {
    1.0}; // to  jest wysokosc pierscienia
/// Board state at the current step; cell values in [0, 1].
double current_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
/// Board state being computed for the next step.
double new_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
/// Normalised convolution kernel (weights sum to 1), centred at
/// [RADIUS][RADIUS].
double mask[SIZE_OF_MASK][SIZE_OF_MASK];

void init_board(void);
void init_mask(void);
int write_settings(const char *folder);
int write_result(const char *folder, int last_iteration, double final_mass,
                 bool died_out, double seconds);

/**
 * @brief Runs the simulation and saves snapshots of the board to disk.
 *
 * For every cell the potential @f$U@f$ is computed as the convolution of
 * ::current_board with ::mask (edges wrap around). The cell is then updated as
 * @f[ A_{t+1} = \mathrm{clip}\big(A_t + \Delta t \cdot G(U),\ 0,\ 1\big),
 *     \qquad G(U) = 2\exp\!\Big(-\frac{(U-\mu)^2}{2\sigma^2}\Big) - 1 @f]
 * where @f$\mu@f$ = MU, @f$\sigma@f$ = SIGMA and @f$\Delta t@f$ =
 * DLUGOSC_KROKU. The new values go to ::new_board first, so every cell is
 * updated from the same old state, and are copied back to ::current_board after
 * the step.
 *
 * After every step the total mass of the board is computed. If it drops below
 * EXTINCTION_MASS, the last (empty) frame is saved and the simulation stops
 * early.
 *
 * The settings are written to SETTINGS_FILE before the first step (so they
 * are kept even if the program is interrupted); the result and the run time
 * are appended after the last step.
 *
 * @return 0 on success (also when life died out early), 1 if the output folder,
 *         the settings file or a frame file could not be written.
 */
int main() {
  init_board();
  init_mask();
  // Tworzymy folder na klatki (razem z jego rodzicem OUTPUT_FOLDER)
  if (init_output(FRAMES_FOLDER) != 0) {
    return 1; // błąd przy tworzeniu folderu
  }
  // Nastawy zapisujemy przed symulacja - zostana nawet po przerwaniu programu
  if (write_settings(OUTPUT_FOLDER) != 0) {
    return 1; // błąd przy zapisie pliku z nastawami
  }

  auto start_time = std::chrono::steady_clock::now();
  double final_mass = 0.0; // masa po ostatnim kroku (do pliku z wynikiem)
  int last_iteration = 0;
  bool died_out = false;

  // Idąc po naszych iteracjach
  for (int iteration = 0; iteration <= NUMBER_OF_INTERATION; iteration++) {
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

    // Przenosimy nowa plansze na stara i przy okazji liczymy mase (sume
    // komorek)
    double mass = 0.0;
    for (int i = 0; i < SIZE_OF_BOARD; i++) {
      for (int j = 0; j < SIZE_OF_BOARD; j++) {
        current_board[i][j] = new_board[i][j];
        mass += current_board[i][j];
      }
    }

    final_mass = mass;
    last_iteration = iteration;
    bool extinct = mass < EXTINCTION_MASS; // wszystkie komorki martwe

    // Co SAVE_EVERY iteracji, a przy wymarciu zawsze - zeby ostatnia klatka
    // byla zapisana
    if (iteration % SAVE_EVERY == 0 || extinct) {
      printf("iteracja %4d   masa %.1f\n", iteration, mass);
      if (save_board_frame(FRAMES_FOLDER, iteration, &current_board[0][0],
                           SIZE_OF_BOARD) != 0) {
        return 1; // błąd przy zapisie pliku
      }
    }

    if (extinct) {
      printf("Zycie wymarlo w iteracji %d - koniec symulacji.\n", iteration);
      died_out = true;
      break;
    }
  }

  double seconds = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();
  printf("Czas symulacji: %.2f s\n", seconds);
  if (write_result(OUTPUT_FOLDER, last_iteration, final_mass, died_out,
                   seconds) != 0) {
    return 1; // błąd przy dopisywaniu wyniku
  }
  return 0; // takze przy wymarciu - to nie blad, make ma otworzyc wizualizacje
}

/**
 * @brief Kernel core: a smooth bump on the interval (0, 1).
 *
 * Exponential kernel core from Lenia:
 * @f[ K_C(r) = \exp\!\Big(4 - \frac{1}{r(1-r)}\Big) @f]
 * It is 0 at both ends, rises smoothly and reaches its maximum of 1 at r = 0.5.
 *
 * @param r Position inside one ring, expected in [0, 1].
 * @return Height of the bump in [0, 1]; 0 for r <= 0 or r >= 1.
 */
inline double calculate_kc(double r) {
  if (r >= 1 || r <= 0) {
    return 0;
  } else {
    return exp(4 - 1 / (r * (1 - r)));
  }
}

/**
 * @brief Kernel shell: kernel weight at a given normalised distance.
 *
 * The kernel radius is divided into NUMBER_OF_RINGS rings of equal width.
 * The distance @p r is mapped to a ring index and a position inside that ring.
 * The result is the height of that ring (::height_of_section) multiplied by
 * calculate_kc() at that position.
 *
 * @param r Distance from the kernel centre divided by RADIUS
 *          (0 = centre, 1 = edge of the kernel).
 * @return Kernel weight before normalisation; 0 for r >= 1.
 */
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

/**
 * @brief Builds the convolution kernel ::mask and normalises it.
 *
 * For every cell of the SIZE_OF_MASK x SIZE_OF_MASK mask, the distance to the
 * centre is divided by RADIUS and turned into a weight by calcualte_ks().
 * All weights are then divided by their sum so that they add up to 1, which
 * keeps the potential U in [0, 1].
 *
 * Prints the sum of the weights before normalisation. Ends the program with
 * exit code 1 if that sum is 0 (the kernel would be empty).
 */
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

/**
 * @brief Clears both boards and fills a random square in the centre.
 *
 * Sets every cell of ::current_board and ::new_board to 0, then fills a
 * SIZE_OF_SQUARE x SIZE_OF_SQUARE square in the middle of ::current_board with
 * random values in [0, 1]. The random generator is seeded with SEED, so every
 * run starts from the same pattern.
 */
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

/**
 * @brief Opens `<folder>/SETTINGS_FILE` and prints a message on failure.
 *
 * @param folder Output folder (must already exist).
 * @param mode   fopen() mode: "w" to create the file, "a" to append to it.
 * @return The opened file, or NULL on error.
 */
static FILE *open_settings_file(const char *folder, const char *mode) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", folder, SETTINGS_FILE);
  FILE *f = fopen(path, mode);
  if (f == NULL) {
    printf("Nie moge otworzyc pliku %s\n", path);
  }
  return f;
}

/**
 * @brief Writes all simulation settings to `<folder>/SETTINGS_FILE`.
 *
 * Creates (or overwrites) the file with the date and every parameter that
 * affects the run, grouped into INI-like sections, so a saved run can always
 * be reproduced. Call it before the simulation starts.
 *
 * @param folder Output folder (must already exist, see init_output()).
 * @return 0 on success, 1 if the file could not be created.
 */
int write_settings(const char *folder) {
  FILE *f = open_settings_file(folder, "w");
  if (f == NULL) {
    return 1;
  }

  char date[32];
  time_t now = time(NULL);
  strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));

  fprintf(f, "# Lenia (CPU_and_GPU_FFT) - nastawy symulacji\n");
  fprintf(f, "date                 = %s\n", date);

  fprintf(f, "\n[world]\n");
  fprintf(f, "SIZE_OF_BOARD        = %d\n", SIZE_OF_BOARD);
  fprintf(f, "NUMBER_OF_INTERATION = %d\n", NUMBER_OF_INTERATION);
  fprintf(f, "SAVE_EVERY           = %d\n", SAVE_EVERY);
  fprintf(f, "EXTINCTION_MASS      = %.10g\n", EXTINCTION_MASS);

  fprintf(f, "\n[kernel]\n");
  fprintf(f, "RADIUS               = %d\n", RADIUS);
  fprintf(f, "NUMBER_OF_RINGS      = %d\n", NUMBER_OF_RINGS);
  fprintf(f, "height_of_section    = {");
  for (int k = 0; k < NUMBER_OF_RINGS; k++) {
    fprintf(f, k == 0 ? "%.10g" : ", %.10g", height_of_section[k]);
  }
  fprintf(f, "}\n");

  fprintf(f, "\n[growth]\n");
  fprintf(f, "MU                   = %.10g\n", MU);
  fprintf(f, "SIGMA                = %.10g\n", SIGMA);
  fprintf(f, "DLUGOSC_KROKU        = %.10g\n", DLUGOSC_KROKU);

  fprintf(f, "\n[start]\n");
  fprintf(f, "SEED                 = %d\n", SEED);
  fprintf(f, "SIZE_OF_SQUARE       = %d\n", SIZE_OF_SQUARE);

  fclose(f);
  return 0;
}

/**
 * @brief Appends the result of the run to `<folder>/SETTINGS_FILE`.
 *
 * Adds a [result] section: whether life survived, the last computed
 * iteration, the final mass and the run time (total and per iteration).
 * If this section is missing, the run was interrupted.
 *
 * @param folder         Output folder with the file from write_settings().
 * @param last_iteration Number of the last computed iteration.
 * @param final_mass     Sum of all cells after the last iteration.
 * @param died_out       true if the simulation stopped because life died out.
 * @param seconds        Run time of the simulation loop (including saving
 *                       the frames).
 * @return 0 on success, 1 if the file could not be opened.
 */
int write_result(const char *folder, int last_iteration, double final_mass,
                 bool died_out, double seconds) {
  FILE *f = open_settings_file(folder, "a");
  if (f == NULL) {
    return 1;
  }
  fprintf(f, "\n[result]\n");
  fprintf(f, "status               = %s\n", died_out ? "died_out" : "survived");
  fprintf(f, "last_iteration       = %d\n", last_iteration);
  fprintf(f, "final_mass           = %.3f\n", final_mass);
  fprintf(f, "time_seconds         = %.2f\n", seconds);
  fprintf(f, "time_per_iter_ms     = %.2f\n",
          1000.0 * seconds / (last_iteration + 1));
  fclose(f);
  return 0;
}
