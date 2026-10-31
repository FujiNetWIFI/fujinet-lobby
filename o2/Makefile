# Makefile -- the FujiNet game lobby for the Odyssey 2 / Videopac:
# build/lobby.bin (build.sh), which CONFIG boots from
# tnfs://ec.tnfs.io/o2/lobby.bin.
#
#   make                  the image (the live lobby)
#   make run              in a MAME window (run.sh), against FUJINET_TCP
#   make test             the scenarios (test/t) on a private rig: its own
#                         fujinet-pc and lobby server, checked against the
#                         screen's text (tools/cfgtest.py)
#   make rows             frame stability of the list, NTSC and PAL
#   make shots            screenshots/, from mock rooms (test/seed/shots.json)

CHECK = O2SIM_FAST=1 python3 tools/cfgtest.py
T = build/t/lobby.bin
SEED = test/seed

all:
	./build.sh

run: all
	./run.sh

# the lobby on the rig's port; 5 Card Stud pointed at a dead server
$(T) build/5card/5card.bin: all
	ENDPOINT=http://127.0.0.1:8089/view OUT=build/t ./build.sh
	OUT=$(CURDIR)/build/5card ENDPOINT=http://127.0.0.1:9/ \
	    $(HOME)/Workspace/fujinet-5cardstud/o2/build.sh

test: $(T) build/5card/5card.bin
	LOBBY_SEED=$(SEED)/rooms.json $(CHECK) test/t/first.scen build/run/t_first 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json $(CHECK) test/t/list.scen build/run/t_list 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json $(CHECK) test/t/keys.scen build/run/t_keys 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json $(CHECK) test/t/join_kc.scen build/run/t_join_kc 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json $(CHECK) test/t/join_5card.scen build/run/t_join_5card 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json LOBBY_CHANGE="2 $(SEED)/move.json POST" \
	    $(CHECK) test/t/move.scen build/run/t_move 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json LOBBY_CHANGE="2 $(SEED)/gone.json DELETE" \
	    $(CHECK) test/t/gone.scen build/run/t_gone 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/bad.json $(CHECK) test/t/errors.scen build/run/t_errors 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/empty.json $(CHECK) test/t/empty.scen build/run/t_empty 60 $(T)
	NAME=PLAYER1 LOBBY_SEED=none $(CHECK) test/t/down.scen build/run/t_down 60 $(T)

rows: $(T)
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json O2SIM_FAST=1 \
	    test/rig.sh test/scen/rows.scen build/run/rows 60 $(T)
	python3 tools/rowcheck.py build/run/rows list
	NAME=PLAYER1 LOBBY_SEED=$(SEED)/rooms.json O2SIM_FAST=1 O2SIM_MACHINE=videopac \
	    test/rig.sh test/scen/rows.scen build/run/rows_pal 60 $(T)
	python3 tools/rowcheck.py build/run/rows_pal list

shots: $(T) build/5card/5card.bin
	rm -rf build/shots
	NAME=ODYSSEY LOBBY_SEED=$(SEED)/shots.json FUJINET_PACE_MS=40 \
	    test/rig.sh test/scen/shots.scen build/shots 120 $(T)
	LOBBY_SEED=$(SEED)/shots.json O2SIM_FAST=1 \
	    test/rig.sh test/scen/shots_name.scen build/shots 60 $(T)
	NAME=ODYSSEY LOBBY_SEED=$(SEED)/empty.json O2SIM_FAST=1 \
	    test/rig.sh test/scen/shots_empty.scen build/shots 60 $(T)
	mkdir -p screenshots
	cp build/shots/*.png screenshots/

clean:
	rm -rf build

.PHONY: all run test rows shots clean
