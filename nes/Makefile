# FujiNet Game Lobby for the NES.
#
# Standalone, like ../coleco/ and ../intv/ -- not wired into the MekkoGX
# framework under ../clients/. A 32K NROM client with 8K CHR-ROM (font.txt
# via tools/mkfont.py) and the cartridge's 8K WRAM at $6000, built with cc65's
# `nes` target and fujinet-lib-experimental's `nes` target (the mailbox
# transport, the FujiNet linker config and the claim stamp) -- the same rig
# as fujinet-config nes/.
#
#   make            build/lobby.nes (stamped, RMW-checked)
#   make install    copy it into the pico bring-up tree, where its run.sh
#                   can boot it by name
#   make run        install + launch it in MAME against a live fujinet-pc
#   make shot       headless MAME capture: screen as text + a PNG
#                   (tools/nes-shot.sh; PRESS="4:Down 5:A" AT=8 make shot)
#   make clean
#
# Environment overrides:
#   FNLIB      fujinet-lib-experimental       (default ~/Workspace/fujinet-lib-experimental)
#   PICO_NES   the cartridge bring-up tree    (default ~/Workspace/fn-nes/pico/nes)
#   LOBBY_URL  endpoint override for testing, e.g.
#              make LOBBY_URL='N:http://127.0.0.1:8080/view'

FNLIB    ?= $(HOME)/Workspace/fujinet-lib-experimental
PICO_NES ?= $(HOME)/Workspace/fn-nes/pico/nes

APP   = lobby.c st_name.c st_list.c st_boot.c fujiraw.c netraw.c
PLAT  = fujidisp.c fujiin.c fujiedit.c sfx.c
SRCS  = $(APP) $(PLAT)
ASRCS = disp_pal.s
OBJS  = $(patsubst %.c,build/%.o,$(SRCS)) $(patsubst %.s,build/%.o,$(ASRCS)) \
        build/font.o

FNLIB_LIB = $(FNLIB)/r2r/nes/fujinet.nes.lib
NES_CFG   = $(FNLIB)/makefiles/nes-fujinet.cfg
ROMSTAMP  = $(FNLIB)/makefiles/nes-romstamp.py
CHECKROM  = $(PICO_NES)/tools/checkrom.py

CFLAGS  = -t nes -O --cpu 6502 -DBUILD_NES -I. -Ibuild \
          -I$(FNLIB)/include -I$(FNLIB)/bus/nes
LDFLAGS = -t nes -C $(NES_CFG) -m build/lobby.map

# The endpoint override rides a generated header, as in ../coleco: one
# mechanism for both ports, and content-compared so an unchanged URL does not
# force a rebuild. A plain `make` reverts the build to the production
# endpoint.
URL_H = build/lobby_url.h
ifdef LOBBY_URL
URL_CONTENT = \#define LOBBY_URL_BASE "$(LOBBY_URL)?bin=1&platform=nes&pagesize="
else
URL_CONTENT = /* production endpoint -- the default in constants.h */
endif

all: build/lobby.nes

.DELETE_ON_ERROR:

$(FNLIB_LIB):
	$(MAKE) -C $(FNLIB) nes

$(URL_H): FORCE | build
	@printf '%s\n' '$(URL_CONTENT)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

build/%.o: %.c $(wildcard *.h) $(URL_H) | build
	cl65 $(CFLAGS) -c -o $@ $<

build/%.o: %.s | build
	cl65 -t nes -c -o $@ $<

# The background pattern table: replaces cc65's neschar.s (see mkfont.py).
build/font.s: font.txt tools/mkfont.py | build
	python3 tools/mkfont.py font.txt $@

build/font.o: build/font.s
	cl65 -t nes -c -o $@ $<

build:
	mkdir -p build

# Stamped with the "FUJI" claim at $FFF0 (without it the cartridge shuts the
# mailbox down the moment the image boots) and checked for read-modify-write
# instructions, which would land on the mailbox as spurious events. The
# stamp's check is a port of the cartridge's own checkrom.py, which runs too
# when the bring-up tree is here to supply it (CI has only the lib).
build/lobby.nes: $(OBJS) $(FNLIB_LIB) | build
	cl65 $(LDFLAGS) -o $@ $(OBJS) $(FNLIB_LIB)
	python3 $(ROMSTAMP) --stamp --map build/lobby.map $@
	if [ -f $(CHECKROM) ]; then \
	    python3 $(CHECKROM) --claim --map build/lobby.map $@; fi

install: build/lobby.nes
	mkdir -p $(PICO_NES)/build
	cp build/lobby.nes $(PICO_NES)/build/lobby.nes

run: install
	cd $(PICO_NES) && ./run.sh lobby

shot: build/lobby.nes
	tools/nes-shot.sh "$(PRESS)"

clean:
	rm -rf build

.PHONY: all install run shot clean FORCE
