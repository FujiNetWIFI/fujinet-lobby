# FujiNet Game Lobby for the Emerson Arcadia 2001.
#
# Self-contained: the image tools (mkimage, checkrom, checkdepth,
# checksize) are vendored under tools/ (copies of fujinet-config/arcadia's
# -- keep them in step). Only Macroassembler AS comes from outside: asl on
# PATH, else ~/asl.
#
#   make            build/lobby.bin (8K, claim stamped, every check clean)
#   make run        launch in MAME against a live fujinet-pc (see run.sh)
#   make drive      a scripted headless session: STEPS='t:LOBBY;d;k:down;d'
#                   (emu/drive.lua lists the step language)
#
# ENDPOINT= and PLATFORM= pass through to build.sh (a test lobby, or
# another platform's rooms); after using them, run make clean so the
# next plain make is the production build.

MAME_DIR    ?= $(HOME)/Workspace/mame
FUJINET_TCP ?= 127.0.0.1:9995
STEPS       ?= t:PAGE;d;q

SOURCES = lobby.asm $(wildcard *.inc) build.sh

MAMEHL = FUJINET_TCP=$(FUJINET_TCP) $(MAME_DIR)/mame arcadia \
	  -cartslot fujinet -cart $(CURDIR)/build/lobby.bin \
	  -video none -sound none -nothrottle -snapshot_directory build/snap \
	  -cfg_directory build/cfg -nvram_directory build/nvram

.PHONY: all run drive clean
all: build/lobby.bin

build/lobby.bin: $(SOURCES)
	./build.sh

run: build/lobby.bin
	./run.sh

drive: build/lobby.bin
	DRIVE_STEPS='$(STEPS)' $(MAMEHL) -autoboot_script emu/drive.lua \
	  -seconds_to_run 120

clean:
	rm -rf build
