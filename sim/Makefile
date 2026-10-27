SIM_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(SIM_DIR)/..)

ifeq ($(OS),Windows_NT)
  BUILD := build.bat
  EXE := kobuki_sim.exe
  RUN := $(SIM_DIR)$(EXE)
else
  BUILD := ./build.sh
  EXE := kobuki_sim
  RUN := $(SIM_DIR)$(EXE)
endif

.PHONY: all build A B check help

all: build

build:
	cd "$(SIM_DIR)" && $(BUILD)

A: build
	cd "$(ROOT)" || exit $$?; "$(RUN)" --layout A --heading 90 --seed 1 -v || { rc=$$?; [ $$rc -eq 1 ] || exit $$rc; }

B: build
	cd "$(ROOT)" || exit $$?; "$(RUN)" --layout B --heading 0 --seed 1 -v || { rc=$$?; [ $$rc -eq 1 ] || exit $$rc; }

check: build
	cd "$(ROOT)" && python3 sim/check_pathfind_recover.py

help:
	@echo "make / make build  build the simulator for this OS"
	@echo "make A             build, run Layout A (heading 90, seed 1, verbose), and write the replay"
	@echo "make B             build, run Layout B (heading 0, seed 1, verbose), and write the replay"
	@echo "make check         build, then run pathfind Reverse recovery check"
