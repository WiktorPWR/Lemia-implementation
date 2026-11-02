# Only_CPU

Szkielet projektu C++ (tylko CPU) budowanego przez **Makefile** i **g++ (MinGW)**.
Przykładowy moduł (`example`) służy tylko do sprawdzenia, że wszystko się kompiluje,
i do podmiany na własny kod.

## Struktura

```
Only_CPU/
├── Makefile            ← opis budowania: kompilator, flagi, headery, biblioteki
├── build_and_run.bat   ← kompilacja + uruchomienie jednym poleceniem
├── inc/                ← headery (.h)
│   └── example.h
├── scr/                ← pliki źródłowe (.cpp)
│   ├── main.cpp
│   └── example.cpp
└── build/              ← tworzy się samo, nie trafia na GitHuba
```

## Wymagania

MinGW z `g++` i `mingw32-make` w PATH (u Ciebie `D:\C++\ming\ucrt64\bin`).

## Kompilacja i uruchomienie

W terminalu, w tym folderze:

```powershell
.\build_and_run.bat           # release: buduje i uruchamia
.\build_and_run.bat debug     # debug: buduje i uruchamia
.\build_and_run.bat build     # release: tylko buduje
.\build_and_run.bat clean     # usuwa folder build
```

Albo bezpośrednio przez make:

```powershell
mingw32-make                  # buduje (release)
mingw32-make run              # buduje i uruchamia
mingw32-make MODE=debug run   # debug
mingw32-make clean
```

Gotowy program: `build\release\Only_CPU.exe` (albo `build\debug\...`).
Make kompiluje tylko pliki, które się zmieniły (również po zmianie headera, którego używają).

---

## Jak dodać nowe pliki do kompilacji

**Wystarczy wrzucić plik `.cpp` do `scr/`**, także do podfolderu (np. `scr/lenia/kernel.cpp`).
Makefile sam zbiera wszystkie `*.cpp`, więc niczego w nim nie zmieniasz.

## Jak dodawać headery

**Header wrzucasz do `inc/`** i dołączasz go w kodzie przez `#include "nazwa.h"`.

- Podfolder `inc/lenia/kernel.h` dołączasz jako `#include "lenia/kernel.h"`.
- Każdy header zaczynaj od `#pragma once`.
- Headery z innego folderu dopisujesz do `INC_DIRS` w Makefile:
  ```make
  INC_DIRS := inc extern/glm
  ```

## Jak dodać bibliotekę

Wszystko ustawiasz w sekcji **Ustawienia** na górze Makefile:

| Zmienna    | Co to jest | Przykład |
|------------|------------|----------|
| `INC_DIRS` | foldery z headerami (`-I`) | `inc D:/libs/SFML/include` |
| `LIB_DIRS` | foldery z plikami `.a` / `.lib` (`-L`) | `D:/libs/SFML/lib` |
| `LIBS`     | nazwy bibliotek **bez** `lib` i rozszerzenia (`-l`) | `sfml-graphics sfml-window sfml-system` |

Przykłady:

- **Header-only** (np. glm, stb): wystarczy dodać folder do `INC_DIRS`.
- **Biblioteka z plikiem `libcos.a`**: `LIB_DIRS := ścieżka/do/lib` oraz `LIBS := cos`.
- **Biblioteka z `.dll`**: jak wyżej, a dodatkowo skopiuj `.dll` obok `.exe` (do `build\release\`),
  inaczej program nie wystartuje.
- **Wątki** (`std::thread`) i **OpenMP**: dopisz `-pthread` lub `-fopenmp` do `CXXFLAGS` i `LDFLAGS`.

> Biblioteka musi być zbudowana pod **MinGW/g++** (pliki `.a`). Pliki `.lib` z Visual Studio
> zwykle nie zadziałają z g++.

## Podpowiadanie w VS Code (clangd)

Makefile przy każdej kompilacji zapisuje `compile_commands.json` z tymi samymi flagami
(`INC_DIRS`, `CXXFLAGS`). Dzięki temu clangd podpowiada kod i widzi Twoje headery i biblioteki.
**Po dodaniu pliku albo zmianie Makefile zbuduj projekt raz.** Gdy podpowiedzi „zgłupieją”:
`Ctrl+Shift+P` → **clangd: Restart language server**.

> Ścieżki w `INC_DIRS` pisz z ukośnikiem `/` (np. `D:/libs/glm`). Odwrotny ukośnik `\` psuje plik JSON.

## Przydatne flagi

- `LDFLAGS := -static` sprawia, że `.exe` działa bez DLL-i MinGW (np. na innym komputerze).
- `-O3 -march=native` w `CXXFLAGS` (release) daje szybszy kod pod Twój procesor.

## Typowe problemy

| Komunikat | Co zrobić |
|-----------|-----------|
| `Nie znaleziono mingw32-make` | Dodaj `D:\C++\ming\ucrt64\bin` do PATH. |
| `undefined reference to ...` | Brakuje pliku `.cpp` z definicją w `scr/` albo biblioteki w `LIBS`. |
| `No such file or directory` przy `#include` | Header nie leży w `inc/` albo jego folder nie jest w `INC_DIRS`. |
| `.exe` nie startuje (brak DLL) | Skopiuj brakujący `.dll` obok `.exe` albo użyj `LDFLAGS := -static`. |
| `missing separator` | W Makefile linie z poleceniami muszą zaczynać się od **tabulatora**, nie spacji. |
