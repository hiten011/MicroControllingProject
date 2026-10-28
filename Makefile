# NUCLEO-G474RE firmware: cmake build + ST-LINK upload
#
#   make          — configure + build Debug
#   make upload   — build, then flash build/Debug/main.elf
#   make PRESET=Release upload

PRESET ?= Debug
BUILD_DIR := build/$(PRESET)
ELF := $(BUILD_DIR)/main.elf

CUBE_ROOT ?= $(HOME)/.local/share/stm32cube/bundles
NINJA_BIN := $(firstword $(wildcard $(CUBE_ROOT)/ninja/*/bin))
GCC_BIN := $(firstword $(wildcard $(CUBE_ROOT)/gnu-tools-for-stm32/*/bin))
export PATH := $(GCC_BIN):$(NINJA_BIN):$(PATH)

STM32_PROGRAMMER ?= $(firstword \
	$(wildcard $(CUBE_ROOT)/programmer/*/bin/STM32_Programmer_CLI) \
	$(shell command -v STM32_Programmer_CLI 2>/dev/null))

.PHONY: all build configure upload clean ensure-programmer fix-stlink-usb

all: build

configure:
	cmake --preset $(PRESET)

build: $(ELF)

$(ELF): configure
	cmake --build --preset $(PRESET)

ensure-programmer:
	@test -n "$(STM32_PROGRAMMER)" -a -x "$(STM32_PROGRAMMER)" || { \
		echo "STM32_Programmer_CLI not found. Install STM32CubeProgrammer or set STM32_PROGRAMMER="; \
		exit 1; \
	}

# ST-LINK needs write access to the USB device node (often root-only until udev is set).
fix-stlink-usb:
	@usb=$$(lsusb -d 0483: 2>/dev/null | grep -Ei 'ST-?LINK|STLINK' | head -1 | \
		sed -n 's/.*Bus \([0-9]*\) Device \([0-9]*\):.*/\1 \2/p'); \
	if [ -z "$$usb" ]; then \
		echo "No ST-LINK on USB — plug in the NUCLEO."; \
		exit 1; \
	fi; \
	set -- $$usb; bus=$$1; dev=$$2; node=/dev/bus/usb/$$(printf '%03d' $$bus)/$$(printf '%03d' $$dev); \
	if [ ! -w "$$node" ]; then \
		echo "Fixing write access on $$node …"; \
		if command -v docker >/dev/null 2>&1; then \
			docker run --rm --privileged -v /dev/bus/usb:/dev/bus/usb alpine \
				chmod 666 "$$node" >/dev/null; \
		elif command -v sudo >/dev/null 2>&1; then \
			sudo chmod 666 "$$node"; \
		else \
			echo "Cannot write $$node (need docker or sudo / udev rule)."; \
			exit 1; \
		fi; \
	fi

upload: $(ELF) ensure-programmer fix-stlink-usb
	$(STM32_PROGRAMMER) -c port=SWD mode=UR -w $(ELF) -v -rst

clean:
	rm -rf build/$(PRESET)
