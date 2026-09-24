# Makefile: `make` compila las tres versiones en bin/ (requiere g++, SDL2 y OpenMP).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

SRC_DIR := src
BIN_DIR := bin
HEADERS := $(SRC_DIR)/screensaver_common.hpp

TARGETS := $(BIN_DIR)/screensaver_sequential \
           $(BIN_DIR)/screensaver_parallel \
           $(BIN_DIR)/screensaver_parallel_v2

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# La version secuencial se compila SIN -fopenmp.
$(BIN_DIR)/screensaver_sequential: $(SRC_DIR)/sequential.cpp $(HEADERS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $< -o $@ $(SDL_LIBS) -lm

$(BIN_DIR)/screensaver_parallel: $(SRC_DIR)/parallel.cpp $(HEADERS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SDL_CFLAGS) $< -o $@ $(SDL_LIBS) -lm

$(BIN_DIR)/screensaver_parallel_v2: $(SRC_DIR)/parallel_v2.cpp $(HEADERS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -fopenmp $(SDL_CFLAGS) $< -o $@ $(SDL_LIBS) -lm

clean:
	rm -f $(TARGETS)

.PHONY: all clean
