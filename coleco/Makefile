# FujiNet Game Lobby for the ColecoVision.
#
# Standalone, like ../intv/ -- not wired into the MekkoGX framework under
# ../clients/. Builds with z88dk's +coleco target, os7lib (the OS7 BIOS
# bindings) and fujinet-lib-experimental's coleco target (the mailbox
# transport), the same rig as fujinet-config/coleco/.
#
#   make            build/lobby.rom (stamped, exactly 32768 bytes)
#   make install    copy it into the pico bring-up tree as build/lobby.bin,
#                   where its run.sh can boot it by name
#   make clean
#
# Environment overrides:
#   Z88DK        z88dk root                     (default ~/Workspace/z88dk)
#   OS7LIB       os7lib checkout                (default ~/Workspace/os7lib)
#   FNLIB        fujinet-lib-experimental       (default ~/Workspace/fujinet-lib-experimental)
#   PICO_COLECO  the cartridge bring-up tree    (default ~/Workspace/fujinet-firmware/pico/coleco)
#   LOBBY_URL    endpoint override for testing, e.g.
#                make LOBBY_URL='N:http://127.0.0.1:8080/view'

Z88DK       ?= $(HOME)/Workspace/z88dk
OS7LIB      ?= $(HOME)/Workspace/os7lib
FNLIB       ?= $(HOME)/Workspace/fujinet-lib-experimental
PICO_COLECO ?= $(HOME)/Workspace/fujinet-firmware/pico/coleco

export ZCCCFG = $(Z88DK)/lib/config
export PATH  := $(Z88DK)/bin:$(PATH)

APP  = lobby.c st_name.c st_list.c st_boot.c fujiraw.c netraw.c
PLAT = fujidisp.c fujiin.c fujisnd.c fujiedit.c fujisplash.c fujisplash_data.c
SRCS = $(APP) $(PLAT)

FNLIB_LIB = $(FNLIB)/r2r/coleco/fujinet.coleco.lib

CFLAGS = +coleco -O2 -I. -Ibuild -I$(OS7LIB)/src -I$(FNLIB)/include -I$(FNLIB)/bus/coleco

# The endpoint override rides a generated header, not a -D: zcc strips the
# quotes out of a -D string on its way to sccz80. Content-compared so an
# unchanged URL does not force a rebuild; a plain `make` reverts the build to
# the production endpoint.
URL_H = build/lobby_url.h
ifdef LOBBY_URL
URL_CONTENT = \#define LOBBY_URL_BASE "$(LOBBY_URL)?bin=1&platform=coleco&pagesize="
else
URL_CONTENT = /* production endpoint -- the default in constants.h */
endif

# The link line is fujinet-config/coleco's, which is the pico bring-up's
# build.sh -- a shipped game's plus the 32K/fence contract; see that script
# for the why of each pragma.
LDFLAGS = +coleco -m -s \
  -pragma-define:CRT_ORG_BSS=0x702C \
  -pragma-define:CRT_COLECO_SPRITE_NAME_SIZE=128 \
  -pragma-define:CRT_COLECO_SPRITE_ORDER_SIZE=32 \
  -pragma-define:CRT_COLECO_BIOS_BUFFER_SIZE=32 \
  -pragma-define:CRT_COLECO_BIOS_CONTROLLER_SIZE=12 \
  -pragma-define:CRT_ENABLE_STDIO=0 \
  -pragma-define:REGISTER_SP=0x73B8 \
  -pragma-define:CLIB_FOPEN_MAX=0 \
  -pragma-define:fputc_cons=0 \
  -pragma-define:CLIB_DEFAULT_SCREEN_MODE=-1 \
  -Cz--romsize=32768 -Cz--rombase=32768 \
  -Cz--code-fence=0xF800 -Cz--data-fence=0xF800 \
  -L$(OS7LIB) -los7 \
  -L$(FNLIB)/r2r/coleco -lfujinet.coleco.lib

all: build/lobby.rom

# A lobby.rom that failed the stamp or the RAM guard must not survive with a
# fresh mtime, or it would never be re-checked.
.DELETE_ON_ERROR:

$(FNLIB_LIB):
	$(MAKE) -C $(FNLIB) coleco/r2r

$(URL_H): FORCE
	@mkdir -p build
	@echo '$(URL_CONTENT)' > $@.tmp; \
	if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

FORCE:

build/lobby.rom: $(SRCS) $(wildcard *.h) $(FNLIB_LIB) $(URL_H)
	mkdir -p build
	zcc $(CFLAGS) $(LDFLAGS) $(SRCS) -o build/lobby -create-app
	@if [ -f $@ ]; then :; else mv build/lobby $@; fi
	@rm -f *.sym   # zcc -s writes per-source symbol files next to the sources
	python3 $(FNLIB)/makefiles/coleco-romstamp.py --stamp $@
	./bsscheck.py build/lobby.map

install: build/lobby.rom
	cp build/lobby.rom $(PICO_COLECO)/build/lobby.bin

clean:
	rm -rf build

.PHONY: all install clean FORCE
