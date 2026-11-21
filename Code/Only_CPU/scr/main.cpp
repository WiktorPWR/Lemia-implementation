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

#define SIZE_OF_BOARD 512
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

/// CSV file with per-iteration timings, written to ::OUTPUT_FOLDER.
#define PROFILE_FILE "profile.csv"

/// The first WARMUP_ITERATIONS iterations are computed normally, but they are
/// not written to PROFILE_FILE and not included in the statistics.
#define WARMUP_ITERATIONS 10

/// Number of timed sections of one iteration.
#define NUMBER_OF_SECTIONS 4
/// Section indices (also the order of columns in PROFILE_FILE).
#define SECTION_POTENTIAL 0 ///< potential U (convolution)
#define SECTION_GROWTH 1    ///< growth function and board update
#define SECTION_COPY_MASS 2 ///< copy new_board -> current_board and mass
#define SECTION_IO 3        ///< printf and saving the PGM frame
/// Sections 0 .. NUMBER_OF_COMPUTE_SECTIONS - 1 are pure computation (no I/O).
#define NUMBER_OF_COMPUTE_SECTIONS 3

/// Height (peak weight) of each kernel ring, from the centre outwards.
const double height_of_section[NUMBER_OF_RINGS] = {
    1.0}; // to  jest wysokosc pierscienia
/// Board state at the current step; cell values in [0, 1].
double current_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
/// Board state being computed for the next step.
double new_board[SIZE_OF_BOARD][SIZE_OF_BOARD];
/// Potential U of every cell (convolution of current_board with mask).
double potential[SIZE_OF_BOARD][SIZE_OF_BOARD];
/// Normalised convolution kernel (weights sum to 1), centred at
/// [RADIUS][RADIUS].
double mask[SIZE_OF_MASK][SIZE_OF_MASK];

/// Names of the sections, in the order of the SECTION_* indices.
static const char *const SECTION_NAMES[NUMBER_OF_SECTIONS] = {
    "potential", "growth", "copy_mass", "io"};

/// Statistics of one section over all measured iterations (after warm-up).
struct SectionStats {
  double sum_ms;    ///< sum of the section time [ms]
  double min_ms;    ///< shortest time of the section [ms]
  int min_ms_iter;  ///< iteration in which min_ms occurred
  double max_ms;    ///< longest time of the section [ms]
  int max_ms_iter;  ///< iteration in which max_ms occurred
  double pct_sum;   ///< sum of the per-iteration shares [%] (for the mean)
  double pct_min;   ///< smallest per-iteration share [%]
  int pct_min_iter; ///< iteration in which pct_min occurred
  double pct_max;   ///< largest per-iteration share [%]
  int pct_max_iter; ///< iteration in which pct_max occurred
};

/// Statistics of all sections.
struct Profile {
  int count; ///< number of iterations in the statistics (after warm-up)
  SectionStats section[NUMBER_OF_SECTIONS];
  double run_ms[NUMBER_OF_SECTIONS]; ///< sum of the times over ALL iterations,
                                     ///< warm-up included [ms] (for the
                                     ///< [time_share] section)
};

void init_board(void);
void init_mask(void);
void compute_potential(void);
void compute_growth(void);
double copy_board_and_mass(void);
double ms_since(std::chrono::steady_clock::time_point since);
void profile_init(Profile *profile);
int profile_record(Profile *profile, FILE *csv, int iteration,
                   const double *ms);
double profile_compute_sum_ms(const Profile *profile);
void write_profile_section(FILE *f, const Profile *profile);
void write_time_share_section(FILE *f, const Profile *profile, double seconds);
int write_settings(const char *folder);
int write_result(const char *folder, int last_iteration, double final_mass,
                 bool died_out, double seconds, const Profile *profile);

/**
 * @brief Runs the simulation, saves snapshots of the board and profiles it.
 *
 * Every step has two passes over the board. Pass A (compute_potential())
 * computes the potential @f$U@f$ of every cell as the convolution of
 * ::current_board with ::mask (edges wrap around) into ::potential. Pass B
 * (compute_growth()) updates every cell as
 * @f[ A_{t+1} = \mathrm{clip}\big(A_t + \Delta t \cdot G(U),\ 0,\ 1\big),
 *     \qquad G(U) = 2\exp\!\Big(-\frac{(U-\mu)^2}{2\sigma^2}\Big) - 1 @f]
 * where @f$\mu@f$ = MU, @f$\sigma@f$ = SIGMA and @f$\Delta t@f$ =
 * DLUGOSC_KROKU. The new values go to ::new_board first, so every cell is
 * updated from the same old state, and are copied back to ::current_board after
 * the step (copy_board_and_mass()).
 *
 * After every step the total mass of the board is computed. If it drops below
 * EXTINCTION_MASS, the last (empty) frame is saved and the simulation stops
 * early.
 *
 * Profiling: every iteration measures four sections (potential, growth, copy +
 * mass, I/O) with one timer per section. Iterations after the first
 * WARMUP_ITERATIONS are written to PROFILE_FILE (one line per iteration) and
 * summed up in a Profile, which goes to the [profile] section of SETTINGS_FILE.
 *
 * The settings are written to SETTINGS_FILE before the first step (so they
 * are kept even if the program is interrupted); the result, the profile and
 * the run time are appended after the last step.
 *
 * @return 0 on success (also when life died out early), 1 if the output folder,
 *         the settings file, the profile file or a frame file could not be
 *         written.
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

  // Plik z pomiarami otwieramy raz, przed petla; zamykamy po niej
  char profile_path[512];
  snprintf(profile_path, sizeof(profile_path), "%s/%s", OUTPUT_FOLDER,
           PROFILE_FILE);
  FILE *profile_file = fopen(profile_path, "w");
  if (profile_file == NULL) {
    printf("Nie moge otworzyc pliku %s\n", profile_path);
    return 1; // błąd przy otwieraniu pliku z profilem
  }
  fprintf(profile_file,
          "iteration,potential_ms,growth_ms,copy_mass_ms,io_ms,total_ms,"
          "potential_pct,growth_pct,copy_mass_pct,io_pct\n");

  Profile profile;
  profile_init(&profile);

  auto start_time = std::chrono::steady_clock::now();
  double final_mass = 0.0; // masa po ostatnim kroku (do pliku z wynikiem)
  int last_iteration = 0;
  bool died_out = false;

  // Idąc po naszych iteracjach
  for (int iteration = 0; iteration <= NUMBER_OF_INTERATION; iteration++) {
    double ms[NUMBER_OF_SECTIONS]; // czasy czterech sekcji tej iteracji [ms]

    // Sekcja 1: potencjal U dla wszystkich komorek
    auto section_start = std::chrono::steady_clock::now();
    compute_potential();
    ms[SECTION_POTENTIAL] = ms_since(section_start);

    // Sekcja 2: wzrost G(U) i aktualizacja do new_board
    section_start = std::chrono::steady_clock::now();
    compute_growth();
    ms[SECTION_GROWTH] = ms_since(section_start);

    // Sekcja 3: przenosimy nowa plansze na stara i liczymy mase
    section_start = std::chrono::steady_clock::now();
    double mass = copy_board_and_mass();
    ms[SECTION_COPY_MASS] = ms_since(section_start);

    final_mass = mass;
    last_iteration = iteration;
    bool extinct = mass < EXTINCTION_MASS; // wszystkie komorki martwe

    // Sekcja 4: zapis I/O. Co SAVE_EVERY iteracji, a przy wymarciu zawsze -
    // zeby ostatnia klatka byla zapisana. W pozostalych iteracjach czas ~0.
    section_start = std::chrono::steady_clock::now();
    if (iteration % SAVE_EVERY == 0 || extinct) {
      printf("iteracja %4d   masa %.1f\n", iteration, mass);
      if (save_board_frame(FRAMES_FOLDER, iteration, &current_board[0][0],
                           SIZE_OF_BOARD) != 0) {
        fclose(profile_file);
        return 1; // błąd przy zapisie pliku
      }
    }
    ms[SECTION_IO] = ms_since(section_start);

    // Czas calego przebiegu (razem z rozgrzewka) - do udzialu w time_seconds
    for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
      profile.run_ms[s] += ms[s];
    }

    // Rozgrzewka nie trafia do statystyk ani do pliku z profilem
    if (iteration >= WARMUP_ITERATIONS &&
        profile_record(&profile, profile_file, iteration, ms) != 0) {
      fclose(profile_file);
      return 1; // błąd przy zapisie pliku z profilem
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
  if (fclose(profile_file) != 0) {
    printf("Blad zapisu pliku %s\n", profile_path);
    return 1; // błąd przy zamykaniu pliku z profilem
  }
  if (write_result(OUTPUT_FOLDER, last_iteration, final_mass, died_out, seconds,
                   &profile) != 0) {
    return 1; // błąd przy dopisywaniu wyniku
  }
  return 0; // takze przy wymarciu - to nie blad, make ma otworzyc wizualizacje
}

/**
 * @brief Pass A: potential U of every cell into ::potential.
 *
 * U is the sum of ::current_board over the mask window around the cell,
 * weighted by ::mask (edges wrap around). The summation order is fixed
 * (i, then j), so the result does not depend on how the work is split into
 * passes.
 */
void compute_potential(void) {
  for (int x = 0; x < SIZE_OF_BOARD; x++) {
    for (int y = 0; y < SIZE_OF_BOARD; y++) {
      double U = 0.0;
      // Dla kazdje komorki liczymy potencjal na podstawei  maski
      for (int i = 0; i < SIZE_OF_MASK; i++) {
        for (int j = 0; j < SIZE_OF_MASK; j++) {
          int x_index = (x + i - RADIUS + SIZE_OF_BOARD) % SIZE_OF_BOARD;
          int y_index = (y + j - RADIUS + SIZE_OF_BOARD) % SIZE_OF_BOARD;
          U += current_board[x_index][y_index] * mask[i][j];
        }
      }
      potential[x][y] = U;
    }
  }
}

/**
 * @brief Pass B: growth function and board update into ::new_board.
 *
 * Reads ::potential and ::current_board and writes the clipped new value of
 * every cell to ::new_board (the formula is in the description of main()).
 */
void compute_growth(void) {
  for (int x = 0; x < SIZE_OF_BOARD; x++) {
    for (int y = 0; y < SIZE_OF_BOARD; y++) {
      double U = potential[x][y];
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
}

/**
 * @brief Copies ::new_board to ::current_board and sums all cells.
 *
 * @return Total mass of the board (sum of all cells) after the copy.
 */
double copy_board_and_mass(void) {
  double mass = 0.0;
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    for (int j = 0; j < SIZE_OF_BOARD; j++) {
      current_board[i][j] = new_board[i][j];
      mass += current_board[i][j];
    }
  }
  return mass;
}

/**
 * @brief Time elapsed since @p since.
 *
 * @param since Start of the measured section (std::chrono::steady_clock).
 * @return Elapsed time in milliseconds.
 */
double ms_since(std::chrono::steady_clock::time_point since) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - since)
      .count();
}

/**
 * @brief Resets @p profile: no iterations, all statistics zero.
 *
 * @param profile Statistics to reset.
 */
void profile_init(Profile *profile) {
  profile->count = 0;
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    profile->section[s] = SectionStats();
    profile->run_ms[s] = 0.0;
  }
}

/**
 * @brief Adds one iteration to the statistics and to the CSV file.
 *
 * Computes total_ms (sum of the four sections) and the share of every section
 * in it, updates sum / minimum / maximum (with iteration numbers) of the times
 * and of the shares, and writes one line to @p csv. If total_ms is 0 (timer
 * resolution), all shares of that iteration are 0.
 *
 * @param profile   Statistics to update.
 * @param csv       Open profile file (see PROFILE_FILE).
 * @param iteration Number of the iteration (written to the CSV).
 * @param ms        Times of the sections in this iteration [ms], indexed by
 *                  SECTION_*.
 * @return 0 on success, 1 if the line could not be written.
 */
int profile_record(Profile *profile, FILE *csv, int iteration,
                   const double *ms) {
  double total_ms = 0.0;
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    total_ms += ms[s];
  }

  bool first = (profile->count == 0); // pierwsza mierzona iteracja
  double pct[NUMBER_OF_SECTIONS];
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    pct[s] = total_ms > 0.0 ? ms[s] / total_ms * 100.0 : 0.0;
    SectionStats *st = &profile->section[s];
    st->sum_ms += ms[s];
    st->pct_sum += pct[s];
    if (first || ms[s] < st->min_ms) {
      st->min_ms = ms[s];
      st->min_ms_iter = iteration;
    }
    if (first || ms[s] > st->max_ms) {
      st->max_ms = ms[s];
      st->max_ms_iter = iteration;
    }
    if (first || pct[s] < st->pct_min) {
      st->pct_min = pct[s];
      st->pct_min_iter = iteration;
    }
    if (first || pct[s] > st->pct_max) {
      st->pct_max = pct[s];
      st->pct_max_iter = iteration;
    }
  }
  profile->count++;

  int written = fprintf(
      csv, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%.4f\n", iteration,
      ms[SECTION_POTENTIAL], ms[SECTION_GROWTH], ms[SECTION_COPY_MASS],
      ms[SECTION_IO], total_ms, pct[SECTION_POTENTIAL], pct[SECTION_GROWTH],
      pct[SECTION_COPY_MASS], pct[SECTION_IO]);
  return written < 0 ? 1 : 0;
}

/**
 * @brief Total time of the computing sections (1-3, without I/O).
 *
 * @param profile Statistics of the run.
 * @return Sum of the potential, growth and copy + mass times [ms].
 */
double profile_compute_sum_ms(const Profile *profile) {
  double sum = 0.0;
  for (int s = 0; s < NUMBER_OF_COMPUTE_SECTIONS; s++) {
    sum += profile->section[s].sum_ms;
  }
  return sum;
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

  fprintf(f, "# Lenia (Only_CPU) - nastawy symulacji\n");
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
 * iteration, the final mass and the run time (total and per iteration),
 * followed by the [profile] section (see write_profile_section()) and the
 * [time_share] section (see write_time_share_section()).
 * If this section is missing, the run was interrupted.
 *
 * @param folder         Output folder with the file from write_settings().
 * @param last_iteration Number of the last computed iteration.
 * @param final_mass     Sum of all cells after the last iteration.
 * @param died_out       true if the simulation stopped because life died out.
 * @param seconds        Run time of the simulation loop (including saving
 *                       the frames).
 * @param profile        Timing statistics of the run.
 * @return 0 on success, 1 if the file could not be opened.
 */
int write_result(const char *folder, int last_iteration, double final_mass,
                 bool died_out, double seconds, const Profile *profile) {
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
  write_profile_section(f, profile);
  write_time_share_section(f, profile, seconds);
  fclose(f);
  return 0;
}

/// Width of the key column in the [profile] section (longest key + 1).
#define PROFILE_KEY_WIDTH 25

/**
 * @brief Prints one line "key = " of the [profile] section, key padded to
 *        PROFILE_KEY_WIDTH, without the value and the newline.
 *
 * @param f      Open settings file.
 * @param prefix Section name (see ::SECTION_NAMES), or "" for no prefix.
 * @param suffix Rest of the key, e.g. "_mean_ms".
 */
static void print_profile_key(FILE *f, const char *prefix, const char *suffix) {
  char key[64];
  snprintf(key, sizeof(key), "%s%s", prefix, suffix);
  fprintf(f, "%-*s= ", PROFILE_KEY_WIDTH, key);
}

/**
 * @brief Appends the [profile] section to the settings file.
 *
 * Contains the number of iterations in the statistics (after warm-up) and,
 * for every section: mean / minimum / maximum time [ms] (with iteration
 * numbers), share of its total time in the total time of all sections [%],
 * and mean / minimum / maximum of the per-iteration shares from the CSV file
 * (with iteration numbers). For sections 1-3 also the share without I/O, and
 * finally the throughput in cells per second, computed from the time of
 * sections 1-3. If no iteration was measured (life died out during warm-up),
 * only the counts and a note are written, with no division by zero.
 *
 * @param f       Open settings file (appended to).
 * @param profile Timing statistics of the run.
 */
void write_profile_section(FILE *f, const Profile *profile) {
  fprintf(f, "\n[profile]\n");
  print_profile_key(f, "", "warmup_iterations");
  fprintf(f, "%d\n", WARMUP_ITERATIONS);
  print_profile_key(f, "", "iterations_in_stats");
  fprintf(f, "%d\n", profile->count);
  if (profile->count == 0) {
    print_profile_key(f, "", "note");
    fprintf(f, "no iterations after warm-up - no statistics\n");
    return;
  }

  double total_sum_ms = 0.0; // czas wszystkich czterech sekcji
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    total_sum_ms += profile->section[s].sum_ms;
  }
  double compute_sum_ms = profile_compute_sum_ms(profile); // sekcje 1-3

  // Srednie, minima i maksima czasow
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_mean_ms");
    fprintf(f, "%.6f\n", profile->section[s].sum_ms / profile->count);
  }
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_min_ms");
    fprintf(f, "%.6f (iteration %d)\n", profile->section[s].min_ms,
            profile->section[s].min_ms_iter);
  }
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_max_ms");
    fprintf(f, "%.6f (iteration %d)\n", profile->section[s].max_ms,
            profile->section[s].max_ms_iter);
  }

  // Udzial sekcji w calym czasie (suma czasu sekcji / suma wszystkich sekcji)
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_share_pct");
    fprintf(f, "%.4f\n",
            total_sum_ms > 0.0
                ? profile->section[s].sum_ms / total_sum_ms * 100.0
                : 0.0);
  }

  // Udzial sekcji 1-3 bez I/O (czas samych obliczen)
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_COMPUTE_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_share_calc_pct");
    fprintf(f, "%.4f\n",
            compute_sum_ms > 0.0
                ? profile->section[s].sum_ms / compute_sum_ms * 100.0
                : 0.0);
  }

  // Udzialy procentowe tak jak w pliku CSV: srednia, minimum, maksimum
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_csv_mean_pct");
    fprintf(f, "%.4f\n", profile->section[s].pct_sum / profile->count);
  }
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_csv_min_pct");
    fprintf(f, "%.4f (iteration %d)\n", profile->section[s].pct_min,
            profile->section[s].pct_min_iter);
  }
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_csv_max_pct");
    fprintf(f, "%.4f (iteration %d)\n", profile->section[s].pct_max,
            profile->section[s].pct_max_iter);
  }

  // Przepustowosc: komorki na sekunde, z czasu sekcji 1-3 (bez I/O)
  fprintf(f, "\n");
  print_profile_key(f, "", "cells_per_second");
  if (compute_sum_ms > 0.0) {
    fprintf(f, "%.0f\n",
            (double)SIZE_OF_BOARD * SIZE_OF_BOARD * profile->count /
                (compute_sum_ms / 1000.0));
  } else {
    fprintf(f, "n/a\n");
  }
}

/**
 * @brief Appends the [time_share] section: share of every section in the
 *        whole run time (time_seconds) [%].
 *
 * Computed once, after the last iteration. Unlike _share_pct in [profile]
 * (only iterations after warm-up, divided by the sum of the sections), it uses
 * the times of ALL iterations, warm-up included, divided by the wall time of
 * the whole simulation loop (time_seconds). untimed_run_pct is the part of
 * time_seconds not covered by any section (writing PROFILE_FILE, the loop
 * itself, the timers).
 *
 * @param f       Open settings file (appended to).
 * @param profile Timing statistics of the run (run_ms).
 * @param seconds Run time of the simulation loop, as time_seconds in [result].
 */
void write_time_share_section(FILE *f, const Profile *profile, double seconds) {
  double run_total_ms = seconds * 1000.0;
  fprintf(f, "\n[time_share]\n");
  print_profile_key(f, "", "run_total_ms");
  fprintf(f, "%.3f\n", run_total_ms);
  if (run_total_ms <= 0.0) {
    print_profile_key(f, "", "note");
    fprintf(f, "run time is 0 - no shares\n");
    return;
  }

  double sections_ms = 0.0; // suma sekcji 1-4 (podsekcje sa juz w sekcji 1)
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    sections_ms += profile->run_ms[s];
  }

  // Laczny czas kazdej sekcji w calym przebiegu
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_run_ms");
    fprintf(f, "%.3f\n", profile->run_ms[s]);
  }

  // Udzial w calym czasie: czas sekcji / time_seconds
  fprintf(f, "\n");
  for (int s = 0; s < NUMBER_OF_SECTIONS; s++) {
    print_profile_key(f, SECTION_NAMES[s], "_run_pct");
    fprintf(f, "%.4f\n", profile->run_ms[s] / run_total_ms * 100.0);
  }
  print_profile_key(f, "", "untimed_run_pct");
  fprintf(f, "%.4f\n", (run_total_ms - sections_ms) / run_total_ms * 100.0);
}
