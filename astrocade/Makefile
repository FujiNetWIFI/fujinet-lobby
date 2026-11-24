# The FujiNet Game Lobby for the Bally Astrocade. Standalone zmac assembly,
# like the other Astrocade FujiNet clients: the build is build.sh (zmac +
# the layout checks), this file is the convenience wrapper.
#
#   make                 build/lobby.bin (live, lobby.fujinet.online)
#   make run             MAME against a fujinet-pc BoIP listener
#   make shot            DEMO=1 build, headless MAME, PNGs in build/astrocde/

SRCS := lobby.asm HVGLIB.H $(wildcard *.inc) $(wildcard assets/*.inc) \
        tools/mkdemo.py

all: build/lobby.bin

build/lobby.bin: $(SRCS) build.sh tools/checksize.py
	./build.sh

run: build/lobby.bin
	./run.sh

MAME_DIR ?= $(HOME)/Workspace/mame

# The demo needs no network: FUJINET_TCP points nowhere, so a live
# fujinet-pc (which accepts one client) is left alone.
shot:
	DEMO=1 ./build.sh
	rm -rf build/astrocde
	cd $(MAME_DIR) && FUJINET_TCP=127.0.0.1:1 ./mame astrocde \
	    -rompath roms \
	    -cartslot fujinet -cart $(CURDIR)/build/lobby.bin \
	    -autoboot_script $(CURDIR)/emu/shot.lua -video none -sound none \
	    -nothrottle -seconds_to_run 26 -snapshot_directory $(CURDIR)/build
	@ls build/astrocde/
	@rm -f build/lobby.bin      # never leave a demo image where make run finds it

clean:
	rm -rf build

.PHONY: all run shot clean
