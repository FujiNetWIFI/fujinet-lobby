PRODUCT = lobby
PLATFORMS += adam
PLATFORMS += adam_cpm
PLATFORMS += apple2
PLATFORMS += atari
PLATFORMS += c64
PLATFORMS += coco
PLATFORMS += msxrom

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

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

include mekkogx/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2

AUTOEXEC_COCO = src/coco/autoexec.bas

coco/disk-post::
	decb copy -t $(AUTOEXEC_COCO) $(R2R_DIR)/coco/$(PRODUCT).dsk,AUTOEXEC.BAS

apple2/disk-post::
	ac -d $(R2R_DIR)/apple2/$(PRODUCT).po BASIC.SYSTEM

atari/r2r-post::
	cp $(R2R_DIR)/atari/$(PRODUCT).com $(R2R_DIR)/atari/$(PRODUCT).xex

