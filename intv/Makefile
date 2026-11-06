INTYBASIC ?= intybasic
AS1600    ?= as1600
LIBDIR    ?= $(HOME)/Workspace/IntyBASIC/intybasic/

PRODUCT = lobby
SOURCES = lobby.bas constants.bas fujinet.bas screen.bas scroll.bas \
          input.bas st_name.bas st_list.bas st_qr.bas st_boot.bas

.PHONY: all clean run
all: $(PRODUCT).bin $(PRODUCT).rom

$(PRODUCT).asm: $(SOURCES)
	$(INTYBASIC) lobby.bas $(PRODUCT).asm $(LIBDIR)

$(PRODUCT).bin: $(PRODUCT).asm       # .bin + .cfg, for PiRTO II / SD-flashed boot
	$(AS1600) -o $(PRODUCT).bin -l $(PRODUCT).lst $(PRODUCT).asm

$(PRODUCT).rom: $(PRODUCT).asm       # Intellicart format, for jzIntv
	$(AS1600) -o $(PRODUCT).rom $(PRODUCT).asm

run: $(PRODUCT).rom
	./run.sh

clean:
	rm -f $(PRODUCT).asm $(PRODUCT).bin $(PRODUCT).cfg $(PRODUCT).rom $(PRODUCT).lst
