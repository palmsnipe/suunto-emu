CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
BUILD_DIR ?= build
TEST_PROFILE ?= sapporo-2.22.60
DIFFERENTIAL_RUNNER_DIR ?= tests/differential

export TEST_FILTER
export SEMU_FIRMWARE_MANIFEST
export FIRMWARE_ROOT
export TEST_PROFILE
export RENODE

CPPFLAGS ?=
CFLAGS ?= -O2
CFLAGS_EXTRA ?=
WARNINGS = -Wall -Wextra -Werror -pedantic
PROJECT_CPPFLAGS = -Iinclude -Isrc/devices -Isrc/compat -Itests/support
PROJECT_CFLAGS = -std=c99 $(WARNINGS) $(CFLAGS) $(CFLAGS_EXTRA)

LIB_SOURCES = $(sort $(shell find src -type f -name '*.c' ! -path 'src/frontends/*'))
LIB_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(LIB_SOURCES))
HEADLESS_SOURCES = src/frontends/cli.c src/frontends/main_headless.c
HEADLESS_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(HEADLESS_SOURCES))
SDL_SOURCES = src/frontends/cli.c src/frontends/main_sdl.c
SDL_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/obj-sdl/%.o,$(SDL_SOURCES))
SDL_INPUT_TEST_BIN = $(BUILD_DIR)/tests/test_sdl_input

UNIT_TEST_SOURCES = $(sort $(wildcard tests/unit/test_*.c))
DEVICE_TEST_SOURCES = $(sort $(wildcard tests/devices/test_*.c))
INTEGRATION_TEST_SOURCES = $(sort $(wildcard tests/integration/test_*.c))
TEST_SOURCES = $(UNIT_TEST_SOURCES) $(DEVICE_TEST_SOURCES) \
               $(INTEGRATION_TEST_SOURCES)
TEST_BINS = $(patsubst tests/unit/%.c,$(BUILD_DIR)/tests/%,$(UNIT_TEST_SOURCES)) \
            $(patsubst tests/devices/%.c,$(BUILD_DIR)/tests/%,$(DEVICE_TEST_SOURCES)) \
            $(patsubst tests/integration/%.c,$(BUILD_DIR)/tests/%,$(INTEGRATION_TEST_SOURCES))
INTEGRATION_TEST_BINS = $(patsubst tests/integration/%.c,$(BUILD_DIR)/tests/%,$(INTEGRATION_TEST_SOURCES))

.PHONY: all sdl bench check-sdl3-required test check check-lines \
	check-task-contracts check-sdl test-firmware test-differential sanitize clean bench

BENCH_BINS = $(BUILD_DIR)/bench_cpu

bench: $(BENCH_BINS)
	@for b in $(BENCH_BINS); do \
		echo "=== $$b ==="; \
		$$b; \
	done

all: $(BUILD_DIR)/suunto-emu

sdl: check-sdl3-required $(BUILD_DIR)/suunto-emu-sdl

check-sdl3-required:
	@PKG_CONFIG="$(PKG_CONFIG)" sh tools/check_sdl3.sh required

$(BUILD_DIR)/libsemu.a: $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(LIB_OBJECTS)

$(BUILD_DIR)/suunto-emu: $(BUILD_DIR)/libsemu.a $(HEADLESS_OBJECTS)
	$(CC) $(PROJECT_CFLAGS) $(HEADLESS_OBJECTS) $(BUILD_DIR)/libsemu.a -o $@

$(BUILD_DIR)/suunto-emu-sdl: $(BUILD_DIR)/libsemu.a $(SDL_OBJECTS)
	$(CC) $(PROJECT_CFLAGS) $(SDL_OBJECTS) $(BUILD_DIR)/libsemu.a \
		$(shell $(PKG_CONFIG) --libs sdl3 2>/dev/null) -o $@

$(BUILD_DIR)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj-sdl/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) \
		$(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null) -MMD -MP -c $< -o $@

$(SDL_OBJECTS) $(BUILD_DIR)/suunto-emu-sdl: | check-sdl3-required

$(SDL_INPUT_TEST_BIN): tests/sdl/test_sdl_input.c tests/support/test.c \
		$(BUILD_DIR)/libsemu.a | check-sdl3-required
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) \
		$(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null) $< \
		tests/support/test.c $(BUILD_DIR)/libsemu.a \
		$(shell $(PKG_CONFIG) --libs sdl3 2>/dev/null) -o $@

$(BUILD_DIR)/bench_cpu: tools/bench_cpu.c $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) tools/bench_cpu.c \
		$(BUILD_DIR)/libsemu.a -o $@

$(BUILD_DIR)/tests/%: tests/unit/%.c tests/support/test.c $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) $< tests/support/test.c \
		$(BUILD_DIR)/libsemu.a -o $@

$(BUILD_DIR)/tests/%: tests/devices/%.c tests/support/test.c $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) $< tests/support/test.c \
		$(BUILD_DIR)/libsemu.a -o $@

$(INTEGRATION_TEST_BINS): $(BUILD_DIR)/tests/%: tests/integration/%.c \
		tests/support/cpu_guest.c tests/support/test.c \
		fixtures/synthetic/rtos/guest_image.h $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) $< \
		tests/support/cpu_guest.c tests/support/test.c $(BUILD_DIR)/libsemu.a -o $@

test:
	@set -e; \
	selected=`sh tools/select_tests.sh $(TEST_SOURCES)`; \
	for test_source in $$selected; do \
		test_name=`basename "$$test_source" .c`; \
		test_binary="$(BUILD_DIR)/tests/$$test_name"; \
		$(MAKE) "$$test_binary"; \
		echo "TEST $$test_binary"; \
		"$$test_binary"; \
	done

check-lines:
	@sh tools/check_source_size.sh

check-task-contracts:
	@sh tools/check_task_contracts.sh

check: all test check-lines check-task-contracts
	@$(BUILD_DIR)/suunto-emu list >/dev/null
	@$(BUILD_DIR)/suunto-emu show-profile sapporo-2.22.60 >/dev/null
	@$(BUILD_DIR)/suunto-emu list-layers --profile sapporo-2.22.60 >/dev/null

check-sdl:
	@set -e; \
	if PKG_CONFIG="$(PKG_CONFIG)" sh tools/check_sdl3.sh probe; then \
		$(MAKE) sdl; \
		$(MAKE) $(SDL_INPUT_TEST_BIN); \
		echo "TEST $(SDL_INPUT_TEST_BIN)"; \
		$(SDL_INPUT_TEST_BIN); \
		SDL_VIDEODRIVER=dummy $(BUILD_DIR)/suunto-emu-sdl list >/dev/null; \
		SEMU_SDL_EMULATOR="$(BUILD_DIR)/suunto-emu-sdl" \
			sh tools/test_sdl_live_input.sh; \
	fi

test-firmware: all
	@SEMU_EMULATOR="$(BUILD_DIR)/suunto-emu" \
		sh tools/run_firmware_tests.sh

test-differential: all
	@SEMU_EMULATOR="$(BUILD_DIR)/suunto-emu" \
		DIFFERENTIAL_RUNNER_DIR="$(DIFFERENTIAL_RUNNER_DIR)" \
		sh tools/run_differential_tests.sh

sanitize:
	@$(MAKE) BUILD_DIR=$(BUILD_DIR)/sanitize \
		CFLAGS='-O1 -g' CFLAGS_EXTRA='-fsanitize=address,undefined -fno-omit-frame-pointer' \
		test

clean:
	rm -rf $(BUILD_DIR)

-include $(LIB_OBJECTS:.o=.d) $(HEADLESS_OBJECTS:.o=.d) $(SDL_OBJECTS:.o=.d)
