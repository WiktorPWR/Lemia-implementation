# CPU_and_GPU

Szkielet projektu C++ + CUDA budowanego przez **CMake**. Przykładowy moduł (`example_gpu`)
dodaje dwie tablice na GPU. Służy tylko do sprawdzenia, że wszystko się kompiluje,
i do podmiany na własny kod.

## Struktura

```
CPU_and_GPU/
├── CMakeLists.txt       ← opis projektu: pliki, headery, biblioteki, flagi
├── build_and_run.bat    ← kompilacja + uruchomienie jednym poleceniem
├── inc/                 ← headery (.h, .cuh)
│   ├── example_gpu.h    ← interfejs GPU bez typów CUDA (można go dołączyć w .cpp)
│   └── cuda_check.cuh   ← makro CUDA_CHECK, tylko dla plików .cu
├── scr/                 ← pliki źródłowe (.cpp, .cu)
│   ├── main.cpp
│   └── example_gpu.cu   ← kernel + funkcja, która go uruchamia
└── build/               ← tworzy się samo, nie trafia na GitHuba
```

## Wymagania

- CUDA Toolkit (u Ciebie 12.9 w `D:\CUDA\v12.9`)
- Visual Studio 2022 Build Tools z „Desktop development with C++” (daje `cl.exe`, CMake i Ninja)

Skrypt sam ładuje środowisko Visual Studio, więc nie trzeba niczego dopisywać do PATH.

## Kompilacja i uruchomienie

W terminalu (PowerShell / cmd / terminal w VS Code), w tym folderze:

```powershell
.\build_and_run.bat           # Release: buduje i uruchamia
.\build_and_run.bat debug     # Debug: buduje i uruchamia (wolniejszy, do debugowania)
.\build_and_run.bat build     # Release: tylko buduje
.\build_and_run.bat clean     # usuwa folder build
```

Gotowy program: `build\Release\CPU_and_GPU.exe` (albo `build\Debug\...`).
Kolejne kompilacje są szybkie, bo budują się tylko zmienione pliki.

> Uruchomiony dwuklikiem skrypt zamyka okno zaraz po zakończeniu, więc lepiej odpalać go z terminala.

Gdy pojawią się dziwne błędy po większych zmianach w `CMakeLists.txt`, zrób `.\build_and_run.bat clean`
i zbuduj projekt od nowa.

---

## Jak dodać nowe pliki do kompilacji

**Wystarczy wrzucić plik do `scr/`.** `CMakeLists.txt` sam zbiera wszystkie `*.cpp` i `*.cu`
z `scr/`, również z podfolderów (np. `scr/lenia/kernel.cu`). Przy następnym uruchomieniu skryptu
nowy plik zostanie wykryty i skompilowany. W `CMakeLists.txt` nie trzeba nic zmieniać.

Który typ pliku wybrać:

| Plik   | Kompilator | Co w nim pisać |
|--------|------------|----------------|
| `.cpp` | `cl.exe`   | zwykły C++ (logika, wczytywanie plików, `main`) |
| `.cu`  | `nvcc`     | wszystko z CUDA: `__global__`, `<<<...>>>`, `cudaMalloc`, `cudaMemcpy` |

Zasada z przykładu: **kod GPU siedzi w `.cu`**, a reszta programu woła go przez zwykłe funkcje
zadeklarowane w headerze bez typów CUDA (tak jak `example_gpu.h` → `addGPU`).

Przykład nowego modułu GPU:

1. `inc/blur_gpu.h`
   ```cpp
   #pragma once
   void blurGPU(float* data, int W, int H);
   ```
2. `scr/blur_gpu.cu`
   ```cpp
   #include "blur_gpu.h"
   #include "cuda_check.cuh"

   __global__ void blurKernel(float* d, int W, int H) { /* ... */ }

   void blurGPU(float* data, int W, int H) {
       // cudaMalloc, cudaMemcpy, blurKernel<<<...>>>, cudaMemcpy, cudaFree
   }
   ```
3. W `main.cpp`: `#include "blur_gpu.h"` i wywołanie `blurGPU(...)`.

## Jak dodawać headery

**Header wrzucasz do `inc/`** i dołączasz go w kodzie przez `#include "nazwa.h"`.
Folder `inc/` jest już dodany jako ścieżka do headerów:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)
```

- Podfolder `inc/lenia/kernel.h` dołączasz jako `#include "lenia/kernel.h"`.
- Każdy header zaczynaj od `#pragma once` (chroni przed podwójnym dołączeniem).
- `.h` to header, który może być dołączony w `.cpp` i `.cu`. **Nie może** zawierać rzeczy z CUDA.
- `.cuh` to header tylko dla plików `.cu` (może zawierać `__global__`, `cuda_runtime.h` itd.).
- Funkcja, która ma działać **i na CPU, i na GPU**, może siedzieć w zwykłym `.h` z makrem `HD`:
  ```cpp
  #ifdef __CUDACC__
  #define HD __host__ __device__   // kompiluje nvcc (.cu)
  #else
  #define HD                       // kompiluje cl.exe (.cpp)
  #endif

  HD inline float growth(float u) { return 2.0f * u - 1.0f; }
  ```
  Wtedy CPU i GPU liczą dokładnie tym samym kodem.

Headery z innego folderu (np. `extern/`) dopisujesz jako kolejną linię w `target_include_directories`:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/stb"
)
```

## Jak dodać bibliotekę

Wszystkie biblioteki podpina się w `CMakeLists.txt` w `target_link_libraries`.
Poniżej najczęstsze przypadki, od najprostszego.

### 1. Biblioteki z CUDA Toolkit (cuFFT, cuBLAS, cuRAND…)

Są już zainstalowane razem z CUDA, więc wystarczy je odkomentować lub dopisać:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    CUDA::cudart
    CUDA::cufft      # FFT, np. szybki splot w Lenii
    CUDA::cublas
    CUDA::curand     # liczby losowe na GPU
)
```

W kodzie: `#include <cufft.h>`, `#include <cublas_v2.h>`, `#include <curand.h>`.

### 2. Biblioteka header-only (np. stb_image, glm)

Takie biblioteki składają się z samych headerów.

1. Skopiuj je np. do `extern/glm/`.
2. Dodaj ścieżkę w `target_include_directories` (jak wyżej).
3. Dołącz: `#include <glm/glm.hpp>`.

### 3. Biblioteka prosto z GitHuba (FetchContent)

CMake sam pobierze i zbuduje bibliotekę przy pierwszej konfiguracji (wymaga internetu i gita).
Dopisz **przed** `add_executable`:

```cmake
include(FetchContent)
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)
```

a potem w `target_link_libraries` dopisz `fmt::fmt`. W kodzie: `#include <fmt/core.h>`.
Pobrane pliki lądują w `build/`, więc nie trafiają na GitHuba.

### 4. Biblioteka zainstalowana w systemie (find_package)

Jeśli biblioteka ma wsparcie CMake (np. SFML, OpenCV zainstalowane przez instalator lub vcpkg):

```cmake
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE sfml-graphics sfml-window sfml-system)
```

Jeśli CMake jej nie znajduje, wskaż folder instalacji przy konfiguracji, np. dopisując do linii
`cmake -S . -B ...` w `build_and_run.bat`: `-DCMAKE_PREFIX_PATH="D:/libs/SFML-2.6.2"`.

### 5. Gotowe pliki .lib / .dll bez CMake

Masz tylko `inc/`, `lib/moja.lib` i `bin/moja.dll` (np. w `extern/moja/`):

```cmake
add_library(moja SHARED IMPORTED)
set_target_properties(moja PROPERTIES
    IMPORTED_IMPLIB   "${CMAKE_CURRENT_SOURCE_DIR}/extern/moja/lib/moja.lib"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/extern/moja/bin/moja.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/extern/moja/inc"
)
target_link_libraries(${PROJECT_NAME} PRIVATE moja)

# skopiuj .dll obok .exe po kompilacji, inaczej program nie wystartuje
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
)
```

Biblioteka tylko z `.lib` (statyczna) to `add_library(moja STATIC IMPORTED)` z `IMPORTED_LOCATION`
ustawionym na plik `.lib`. Kopiowanie DLL nie jest wtedy potrzebne.

> Uwaga: `.gitignore` pomija pliki `*.lib` i `*.dll`. Gotowe binarki bibliotek w `extern/`
> trzeba albo dodać do repo wymuszając (`git add -f extern/...`), albo (lepiej) pobierać je
> sposobem 3 lub 4.

---

## Inna karta graficzna

Architektura GPU jest ustawiona na początku `CMakeLists.txt`: `CMAKE_CUDA_ARCHITECTURES 61`
(GTX 1050, Pascal). Dla innej karty zmień liczbę, np. `75` (RTX 20xx), `86` (RTX 30xx), `89` (RTX 40xx),
albo podaj kilka naraz: `61;86`. Po zmianie zrób `.\build_and_run.bat clean`.

## Typowe problemy

| Komunikat | Co zrobić |
|-----------|-----------|
| `Nie znaleziono Visual Studio z kompilatorem C++ i CMake` | Doinstaluj w Visual Studio Installer „Desktop development with C++” (z komponentem CMake). |
| `No CMAKE_CUDA_COMPILER could be found` | Sprawdź, czy `nvcc --version` działa w terminalu (CUDA w PATH). |
| `undefined reference` / `LNK2019 unresolved external symbol` | Brakuje pliku `.cpp`/`.cu` z definicją w `scr/` albo biblioteki w `target_link_libraries`. |
| `cannot open include file` | Header nie leży w `inc/` albo jego folder nie jest w `target_include_directories`. |
| Kod `__global__` / `<<<>>>` nie kompiluje się | Ten kod musi być w pliku `.cu`, nie `.cpp`. |
