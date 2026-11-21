# Makefile - projekt Only_CPU_FFT (g++ / MinGW)
# Uzycie:  mingw32-make              - kompilacja (release)
#          mingw32-make run          - kompilacja + uruchomienie
#          mingw32-make show         - kompilacja + czyszczenie wynikow + uruchomienie + okno z wynikami
#          mingw32-make view         - samo okno z wynikami, bez liczenia od nowa
#          mingw32-make gif          - GIF z istniejacych klatek (Results/animation.gif)
#          mingw32-make MODE=debug   - kompilacja w trybie debug
#          mingw32-make clean        - usuwa folder build
#          mingw32-make clean_results - usuwa folder z klatkami (Results)

# ================= Ustawienia (to zmieniasz) =================
TARGET   := Only_CPU_FFT
CXX      := g++
MODE     ?= release

INC_DIRS := inc              # foldery z headerami        (-I)
LIB_DIRS :=                  # foldery z bibliotekami     (-L)
LIBS     := fftw3            # nazwy bibliotek bez "lib"  (-l), np. sfml-graphics
                             # fftw3 = FFTW (double), z MSYS2: mingw-w64-ucrt-x86_64-fftw

CXXFLAGS := -std=c++17 -Wall -Wextra
LDFLAGS  :=
#define THREADS 4

# Wizualizacja: RESULTS_DIR musi byc tym samym folderem co OUTPUT_FOLDER w main.cpp
RESULTS_DIR := Results
VIEWER      := ../Visualization/lenia_viewer.py
GIF_MAKER   := ../Visualization/make_gif.py

ifeq ($(MODE),debug)
  CXXFLAGS += -O0 -g
else
  CXXFLAGS += -O2 -DNDEBUG
endif

# ================= Pliki (zbierane automatycznie) =================
SRC_DIR := scr
BUILD   := build/$(MODE)

# rekurencyjny wildcard: wszystkie .cpp z scr/ i podfolderow
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRCS := $(call rwildcard,$(SRC_DIR),*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD)/obj/%.o)
DEPS := $(OBJS:.o=.d)

# -MMD -MP: g++ zapisuje, z jakich headerow korzysta plik -> zmiana headera = rekompilacja
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CPPFLAGS  := $(INC_FLAGS) -MMD -MP
LDFLAGS  += $(addprefix -L,$(LIB_DIRS))
LDLIBS   := $(addprefix -l,$(LIBS))

# ================= Roznice Windows / Linux =================
ifeq ($(OS),Windows_NT)
  SHELL := cmd.exe
  EXE   := $(BUILD)/$(TARGET).exe
  # "|| ver >nul": przy kompilacji rownoleglej (-j) inny proces mogl juz utworzyc folder
  mkdir  = @if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)" 2>nul || ver >nul
  RMDIR  = @if exist build rmdir /s /q build
  RMRESULTS = @if exist "$(RESULTS_DIR)" rmdir /s /q "$(RESULTS_DIR)"
  RUN    = $(subst /,\,$(EXE))
  PYTHON ?= python
else
  EXE   := $(BUILD)/$(TARGET)
  mkdir  = @mkdir -p $1
  RMDIR  = @rm -rf build
  RMRESULTS = @rm -rf "$(RESULTS_DIR)"
  RUN    = ./$(EXE)
  PYTHON ?= python3
endif

# ================= Reguly =================
.PHONY: all run clean clean_results show view gif

all: $(EXE) compile_commands.json

# compile_commands.json dla clangd (podpowiadanie w VS Code) - te same flagi co przy kompilacji
comma := ,
COMPDB_ENTRY = {"directory": "$(CURDIR)", "file": "$1", "command": "$(CXX) $(INC_FLAGS) $(CXXFLAGS) -c $1"}

compile_commands.json: $(SRCS) Makefile
	$(file >$@,[$(subst } {,}$(comma) {,$(strip $(foreach s,$(SRCS),$(call COMPDB_ENTRY,$s))))])
	@echo Zapisano $@

$(EXE): $(OBJS)
	$(call mkdir,$(@D))
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/obj/%.o: $(SRC_DIR)/%.cpp
	$(call mkdir,$(@D))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: all
	$(RUN)

# show: kompilacja -> usuniecie starych klatek -> symulacja -> GIF -> okno z wynikami.
# Komendy wykonuja sie po kolei; jesli program zwroci blad, make przerywa
# i okno sie nie otworzy. Nie zalezy od "run", bo czyszczenie musi byc PRZED symulacja.
show: all
	$(RMRESULTS)
	$(RUN)
	$(PYTHON) $(GIF_MAKER) $(RESULTS_DIR)
	$(PYTHON) $(VIEWER) $(RESULTS_DIR)

view:
	$(PYTHON) $(VIEWER) $(RESULTS_DIR)

# GIF z istniejacych klatek (zapisuje RESULTS_DIR/animation.gif)
gif:
	$(PYTHON) $(GIF_MAKER) $(RESULTS_DIR)

clean:
	$(RMDIR)

# usuwa folder z klatkami (program sam go utworzy przy nastepnym uruchomieniu)
clean_results:
	$(RMRESULTS)

-include $(DEPS)
