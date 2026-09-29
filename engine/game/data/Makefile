# Native ROM data readers and their oracle check.
#
#   make                       build/datacheck
#   make tables                regenerate oracles_tables_*.c from the disasm symbol files
#   make check DISASM=... ROM_AGES=... ROM_SEASONS=...
#                              dump both games and compare with the disasm assets

CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
DISASM ?= $(abspath ../../../../oracles-disasm)
ROM_AGES ?=
ROM_SEASONS ?=

SRC = oracles_rom.c layout.c tileset.c gfx.c oracles_tables_ages.c oracles_tables_seasons.c
BUILD = build

all: $(BUILD)/datacheck

$(BUILD)/datacheck: $(SRC) datacheck.c oracles_rom.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRC) datacheck.c

tables:
	python3 gen_tables.py --sym $(DISASM)/ages.sym --game ages
	python3 gen_tables.py --sym $(DISASM)/seasons.sym --game seasons

check: $(BUILD)/datacheck
	@test -n "$(ROM_AGES)" || (echo "ROM_AGES is required" && exit 2)
	@test -n "$(ROM_SEASONS)" || (echo "ROM_SEASONS is required" && exit 2)
	$(BUILD)/datacheck --rom "$(ROM_AGES)" --game ages --out $(BUILD)/dump-ages
	python3 check_against_disasm.py --dump $(BUILD)/dump-ages --disasm $(DISASM) --game ages --rom "$(ROM_AGES)"
	$(BUILD)/datacheck --rom "$(ROM_SEASONS)" --game seasons --out $(BUILD)/dump-seasons
	python3 check_against_disasm.py --dump $(BUILD)/dump-seasons --disasm $(DISASM) --game seasons --rom "$(ROM_SEASONS)"

clean:
	rm -rf $(BUILD)

.PHONY: all tables check clean
