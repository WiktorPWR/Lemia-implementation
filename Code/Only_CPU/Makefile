# Makefile - projekt Only_CPU (g++ / MinGW)
# Uzycie:  mingw32-make              - kompilacja (release)
#          mingw32-make run          - kompilacja + uruchomienie
#          mingw32-make MODE=debug   - kompilacja w trybie debug
#          mingw32-make clean        - usuwa folder build

# ================= Ustawienia (to zmieniasz) =================
TARGET   := Only_CPU
CXX      := g++
MODE     ?= release

INC_DIRS := inc              # foldery z headerami        (-I)
LIB_DIRS :=                  # foldery z bibliotekami     (-L)
LIBS     :=                  # nazwy bibliotek bez "lib"  (-l), np. sfml-graphics

CXXFLAGS := -std=c++17 -Wall -Wextra
LDFLAGS  :=

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
  RUN    = $(subst /,\,$(EXE))
else
  EXE   := $(BUILD)/$(TARGET)
  mkdir  = @mkdir -p $1
  RMDIR  = @rm -rf build
  RUN    = ./$(EXE)
endif

# ================= Reguly =================
.PHONY: all run clean

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

clean:
	$(RMDIR)

-include $(DEPS)
