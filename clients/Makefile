PRODUCT = lobby
PLATFORMS += adam
PLATFORMS += adam_cpm
PLATFORMS += apple2
PLATFORMS += atari
PLATFORMS += c64
PLATFORMS += coco
PLATFORMS += msxrom
# msdos must be built separately via the make-exp bash script:
#PLATFORMS += msdos

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%
INCLUDE_DIRS = include include/%PLATFORM%

CFLAGS = -DBUILD_$(PLATFORM_UC)

# FUJINET_LIB can be
# - a version number such as 4.7.6
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no fujinet-lib will be used
FUJINET_LIB = 4.10.0

# HIRESTXT_LIB can be
# - a version number such as 0.5.0.2
# - a directory which contains the built library
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no hirestxt-mod will be used
# Only used for coco/dragon builds.
HIRESTXT_LIB =

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

# Empty suppresses the framework re-adding BASIC.SYSTEM to the apple2 disk.
DISK_BASIC_SYSTEM =

include mekkogx/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2

# hirestxt puts its PMODE 4 buffer at $0A00..$21FF, so code must start above it.
LDFLAGS_EXTRA_COCO = --org=2200 --limit=7C00
CFLAGS_EXTRA_COCO += -Wno-const

AUTOEXEC_COCO = src/coco/autoexec.bas
AUTOEXEC_MSDOS = src/msdos/AUTOEXEC.BAT

# LOBBY.BIN loads over BASIC's program area, so a small loader placed low
# does the RUNM from machine code instead of BASIC doing the LOADM.
LOADER_COCO = src/coco/loader/loader.c
LOADER_BIN = $(R2R_DIR)/coco/loader.bin
DISK_EXTRA_DEPS_COCO := $(AUTOEXEC_COCO) $(LOADER_BIN)

$(LOADER_BIN):: $(LOADER_COCO)
	mkdir -p $(R2R_DIR)/coco
	cmoc -o $@ $<

coco/disk-post:: $(LOADER_BIN)
	decb copy -t -0 $(AUTOEXEC_COCO) $(R2R_DIR)/coco/$(PRODUCT).dsk,AUTOEXEC.BAS
	decb copy -b -2 $(LOADER_BIN) $(R2R_DIR)/coco/$(PRODUCT).dsk,LOADER.BIN

msdos/disk-post::
	mcopy -t -i $(R2R_DIR)/msdos/$(PRODUCT).img $(AUTOEXEC_MSDOS) "::AUTOEXEC.BAT"

atari/r2r-post::
	cp $(R2R_DIR)/atari/$(PRODUCT).com $(R2R_DIR)/atari/$(PRODUCT).xex

