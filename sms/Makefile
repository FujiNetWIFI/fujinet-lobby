# FujiNet Game Lobby for the Sega Master System.
#
# Standalone, like ../coleco/ and ../intv/ -- not wired into the MekkoGX
# framework under ../clients/. A 32K client built with z88dk's +sms target
# and fujinet-lib-experimental's sms target (the mailbox transport and the
# claim stamp) -- the same rig as fujinet-config sms/. z88dk writes
# zcc_opt.def into its working directory, so it runs in build/.
#
#   make            build/lobby.sms (32K, stamped)
#   make install    copy it into the pico bring-up tree, where its run.sh
#                   can run it by name
#   make run        install + launch it in MAME against a live fujinet-pc
#   make shot       headless MAME capture: screen as text + a PNG
#                   (tools/sms-shot.sh; PRESS="9:Down 10:B1" AT=12 make shot)
#   make clean
#
# Environment overrides:
#   Z88DK      z88dk root                      (default ~/Workspace/z88dk)
#   FNLIB      fujinet-lib-experimental, add-sms (default ~/Workspace/fnlib-sms)
#   PICO_SMS   the cartridge bring-up tree     (default ~/Workspace/fn-sms/pico/sms)
#   LOBBY_URL  endpoint override for testing, e.g.
#              make LOBBY_URL='N:http://127.0.0.1:8080/view'

Z88DK    ?= $(HOME)/Workspace/z88dk
FNLIB    ?= $(HOME)/Workspace/fnlib-sms
PICO_SMS ?= $(HOME)/Workspace/fn-sms/pico/sms

export ZCCCFG = $(Z88DK)/lib/config
export PATH  := $(Z88DK)/bin:$(PATH)

APP  = lobby.c st_name.c st_list.c st_boot.c ink.c fujiraw.c netraw.c
PLAT = fujidisp.c fujiin.c fujisnd.c snd_data.c fujiedit.c fujisplash.c \
       fujisplash_data.c
SRCS = $(APP) $(PLAT)

FNLIB_LIB = $(FNLIB)/r2r/sms/fujinet.sms.lib

# The endpoint override rides a generated header, as in ../coleco: zcc
# strips the quotes from a -D string, and the header is content-compared so
# an unchanged URL does not force a rebuild. A plain `make` reverts the build
# to the production endpoint.
URL_H = build/lobby_url.h
ifdef LOBBY_URL
URL_CONTENT = \#define LOBBY_URL_BASE "$(LOBBY_URL)?bin=1&platform=sms&pagesize="
else
URL_CONTENT = /* production endpoint -- the default in constants.h */
endif

all: build/lobby.sms

.DELETE_ON_ERROR:

$(FNLIB_LIB):
	$(MAKE) -C $(FNLIB) sms/r2r

build:
	mkdir -p build

$(URL_H): FORCE | build
	@printf '%s\n' '$(URL_CONTENT)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

# The tiles (window frame, cursor, font) are ASCII art in font.txt.
build/font_data.c: font.txt tools/mkfont.py | build
	python3 tools/mkfont.py font.txt $@

# Stamped with the "FUJI" claim at $7FDC (without it the cartridge runs the
# image as a plain game, mailbox off), which also re-checksums the header.
build/lobby.sms: $(SRCS) build/font_data.c $(wildcard *.h) $(URL_H) $(FNLIB_LIB)
	cd build && zcc +sms -O2 -I.. -I. -I$(FNLIB)/include -I$(FNLIB)/bus/sms -m \
	    -pragma-define:CRT_ENABLE_STDIO=0 -pragma-define:CLIB_FOPEN_MAX=0 \
	    $(addprefix ../,$(SRCS)) font_data.c -L$(FNLIB)/r2r/sms -lfujinet.sms.lib \
	    -o lobby -create-app
	python3 $(FNLIB)/makefiles/sms-romstamp.py --stamp $@

install: build/lobby.sms
	mkdir -p $(PICO_SMS)/build
	cp build/lobby.sms $(PICO_SMS)/build/lobby.sms

run: install
	cd $(PICO_SMS) && ./run.sh lobby

shot: build/lobby.sms
	tools/sms-shot.sh "$(PRESS)"

clean:
	rm -rf build

.PHONY: all install run shot clean FORCE
