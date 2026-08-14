CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
BUILD_DIR ?= build

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

UNIT_TEST_SOURCES = $(sort $(wildcard tests/unit/test_*.c))
DEVICE_TEST_SOURCES = $(sort $(wildcard tests/devices/test_*.c))
TEST_SOURCES = $(UNIT_TEST_SOURCES) $(DEVICE_TEST_SOURCES)
TEST_BINS = $(patsubst tests/unit/%.c,$(BUILD_DIR)/tests/%,$(UNIT_TEST_SOURCES)) \
            $(patsubst tests/devices/%.c,$(BUILD_DIR)/tests/%,$(DEVICE_TEST_SOURCES))

.PHONY: all sdl test check check-sdl test-firmware sanitize clean

all: $(BUILD_DIR)/suunto-emu

sdl: $(BUILD_DIR)/suunto-emu-sdl

$(BUILD_DIR)/libsemu.a: $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(LIB_OBJECTS)

$(BUILD_DIR)/suunto-emu: $(BUILD_DIR)/libsemu.a $(HEADLESS_OBJECTS)
	$(CC) $(PROJECT_CFLAGS) $(HEADLESS_OBJECTS) $(BUILD_DIR)/libsemu.a -o $@

$(BUILD_DIR)/suunto-emu-sdl: $(BUILD_DIR)/libsemu.a $(SDL_OBJECTS)
	$(CC) $(PROJECT_CFLAGS) $(SDL_OBJECTS) $(BUILD_DIR)/libsemu.a \
		$(shell $(PKG_CONFIG) --libs sdl3) -o $@

$(BUILD_DIR)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj-sdl/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) \
		$(shell $(PKG_CONFIG) --cflags sdl3) -MMD -MP -c $< -o $@

$(BUILD_DIR)/tests/%: tests/unit/%.c tests/support/test.c $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) $< tests/support/test.c \
		$(BUILD_DIR)/libsemu.a -o $@

$(BUILD_DIR)/tests/%: tests/devices/%.c tests/support/test.c $(BUILD_DIR)/libsemu.a
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(PROJECT_CFLAGS) $< tests/support/test.c \
		$(BUILD_DIR)/libsemu.a -o $@

test: $(TEST_BINS)
	@set -e; for test_binary in $(TEST_BINS); do \
		echo "TEST $$test_binary"; "$$test_binary"; \
	done

check: all test
	@sh tools/check_source_size.sh
	@$(BUILD_DIR)/suunto-emu list >/dev/null
	@$(BUILD_DIR)/suunto-emu show-profile sapporo-2.22.60 >/dev/null
	@$(BUILD_DIR)/suunto-emu list-layers --profile sapporo-2.22.60 >/dev/null

check-sdl: sdl
	@SDL_VIDEODRIVER=dummy $(BUILD_DIR)/suunto-emu-sdl list >/dev/null

test-firmware: all
	@test -n "$(SEMU_FIRMWARE_MANIFEST)" || \
		{ echo "SEMU_FIRMWARE_MANIFEST is required" >&2; exit 2; }
	@$(BUILD_DIR)/suunto-emu validate --profile sapporo-2.22.60 \
		--firmware "$(SEMU_FIRMWARE_MANIFEST)"

sanitize:
	@$(MAKE) BUILD_DIR=$(BUILD_DIR)/sanitize \
		CFLAGS='-O1 -g' CFLAGS_EXTRA='-fsanitize=address,undefined -fno-omit-frame-pointer' \
		test

clean:
	rm -rf $(BUILD_DIR)

-include $(LIB_OBJECTS:.o=.d) $(HEADLESS_OBJECTS:.o=.d) $(SDL_OBJECTS:.o=.d)
