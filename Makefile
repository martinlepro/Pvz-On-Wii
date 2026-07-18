#---------------------------------------------------------------------------------
# Plants vs. Zombies Wii — devkitPPC / libogc / GRRLIB Makefile
#---------------------------------------------------------------------------------
TARGET   := pvz_wii
BUILD    := build
SOURCES  := source
INCLUDES := include
DATA     := assets

HB_OUT   := output/homebrew
ISO_OUT  := output/disc
STAGING  := disc/staging

APP_TITLE       := Plants vs Zombies Wii
APP_AUTHOR      := Open Source Contributors
APP_VERSION     := 0.1.0

#---------------------------------------------------------------------------------
# Root dir depends on the recursive-make phase (this Makefile re-invokes itself
# from inside build/ to compile object files -- see the two-phase split below)
#---------------------------------------------------------------------------------
ifeq ($(BUILD),$(notdir $(CURDIR)))
  ROOT_DIR := $(realpath ..)
else
  ROOT_DIR := $(CURDIR)
endif

#---------------------------------------------------------------------------------
# devkitPro / devkitPPC -- project-local, no PATH or environment-variable setup
# required. Lives at $(ROOT_DIR)/tools/devkitpro, right next to tools/wit/.
#
# tools/ is gitignored (it's a big, platform-specific binary toolchain, not
# source). Run scripts/setup_toolchain.bat once after cloning and it downloads
# everything into tools/devkitpro + tools/wit automatically.
#
# Lookup order (first match wins):
#   1. DEVKITPRO already set in the environment (explicit override, optional)
#   2. $(ROOT_DIR)/tools/devkitpro  (fetched by scripts/setup_toolchain.bat)
#---------------------------------------------------------------------------------
DEVKITPRO_DIR := tools/devkitpro

ifeq ($(strip $(DEVKITPRO)),)
  ifneq ($(wildcard $(ROOT_DIR)/$(DEVKITPRO_DIR)/devkitPPC/wii_rules),)
    DEVKITPRO := $(ROOT_DIR)/$(DEVKITPRO_DIR)
    export DEVKITPRO
  else
    $(error devkitPro toolchain not found in $(DEVKITPRO_DIR)/. Run scripts/setup_toolchain.bat once after cloning this repo -- it downloads devkitPPC, GRRLIB and WIT into tools/ automatically, no installer or environment variables needed. (Already have devkitPro installed elsewhere? Set DEVKITPRO yourself for this shell instead.))
  endif
else
  export DEVKITPRO
endif

DEVKITPPC := $(DEVKITPRO)/devkitPPC
export DEVKITPPC

include $(DEVKITPPC)/wii_rules

WIT_DIR  := tools/wit
WIT_PATH ?= $(ROOT_DIR)/$(WIT_DIR)/wit
export WIT_PATH

#---------------------------------------------------------------------------------
# libogc (GRRLIB is installed into here per GRRLIB's own docs) + portlibs
#
# Checked in both the standard devkitPro location (tools/devkitpro/portlibs)
# and at the project root (tools/portlibs was found there in your upload --
# harmless to also search it even if unused).
#---------------------------------------------------------------------------------
LIBOGC_DIR    := $(DEVKITPRO)/libogc
PORTLIBS_DIRS := $(DEVKITPRO)/portlibs/wii $(DEVKITPRO)/portlibs/ppc \
                  $(ROOT_DIR)/portlibs/wii $(ROOT_DIR)/portlibs/ppc

PROJ_INCLUDES := -I$(ROOT_DIR)/include \
                 -I$(ROOT_DIR)/source \
                 -I$(ROOT_DIR)/$(BUILD) \
                 -I$(LIBOGC_DIR)/include \
                 $(foreach dir,$(PORTLIBS_DIRS),-I$(dir)/include)

# The actual fix: these -L flags were missing entirely before, which is why
# the linker couldn't find -lgrrlib/-lfreetype/etc. even though the headers
# resolved fine via PROJ_INCLUDES above.
LIBPATHS := -L$(LIBOGC_DIR)/lib/wii \
            $(foreach dir,$(PORTLIBS_DIRS),-L$(dir)/lib)

CFLAGS   := -g -O2 -Wall $(MACHDEP) $(PROJ_INCLUDES)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
LDFLAGS  := -g $(MACHDEP)

LD := $(CXX)

# freetype's own transitive dependency chain (bz2/png/z, plus harfbuzz and/or
# brotli depending on how this particular portlib build was compiled) is
# queried from its own freetype-config script rather than hardcoded, since
# guessing it by hand breaks every time the devkitPro freetype build changes
# (this is also what devkitPro's own wiki recommends for config-script-style
# portlibs). Falls back to a best-effort guess if the script isn't found.
FREETYPE_CONFIG := $(DEVKITPRO)/portlibs/ppc/bin/freetype-config
FREETYPE_LIBS    := $(shell $(FREETYPE_CONFIG) --libs 2>/dev/null)
ifeq ($(strip $(FREETYPE_LIBS)),)
  FREETYPE_LIBS := -lfreetype -lbz2 -lpng -lz
endif

# -lpngu: GRRLIB's own bundled PNG-loading helper (built alongside
# libgrrlib.a in vendor/grrlib-src/GRRLIB/lib/pngu) -- GRRLIB's PNG functions
# call into it directly, so it must be linked explicitly, it isn't folded
# into libgrrlib.a itself.
# -lbrotlidec / -lbrotlicommon: this freetype build is compiled with WOFF2
# support (--with-brotli=yes per devkitPro's own ppc/freetype PKGBUILD), so
# it unconditionally needs Brotli's decoder linked in -- freetype-config's
# own output doesn't reflect this dependency, so it's added explicitly
# rather than relying on FREETYPE_LIBS to catch it.
LIBS := -lgrrlib -lpngu $(FREETYPE_LIBS) -lbrotlidec -lbrotlicommon -ljpeg -lpng -lz -lfat -lwiiuse -lbte -lmad -lasnd -logc -lm

ifneq (,$(wildcard $(ROOT_DIR)/icon.png))
	ICON := icon.png
endif

ISO_SCRIPT_SH  := scripts/build_iso.sh
ISO_SCRIPT_BAT := scripts/build_iso.bat

#---------------------------------------------------------------------------------
# Top-level recipes (outer Makefile - Phase 1)
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(ROOT_DIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(ROOT_DIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(ROOT_DIR)/$(dir))
export DEPSDIR := $(ROOT_DIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
BINFILES := $(filter-out %.wav %.ttf,$(BINFILES))

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES     := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o) $(OFILES_BIN)
export HFILES     := $(addsuffix .h,$(subst .,_,$(BINFILES)))

.PHONY: all iso wbfs homebrew opening-bnr check-wit clean clean-iso run help

all: $(BUILD) $(TARGET).dol homebrew
	@if [ -x "$(WIT_PATH)" ] || [ -x "$(WIT_PATH).exe" ]; then \
		echo "==> Building ISO + WBFS"; \
		TARGET=$(TARGET) WIT_PATH="$(WIT_PATH)" "$(ROOT_DIR)/$(ISO_SCRIPT_SH)" "$(ROOT_DIR)/$(TARGET).dol"; \
	else \
		echo "==> ISO/WBFS: skipped (install WIT to enable: make iso WIT_PATH=/path/to/wit)"; \
	fi

iso: check-wit
	@echo "==> Packing Wii disc images"
	@chmod +x $(ROOT_DIR)/$(ISO_SCRIPT_SH) 2>/dev/null || true
	@TARGET=$(TARGET) WIT_PATH="$(WIT_PATH)" "$(ROOT_DIR)/$(ISO_SCRIPT_SH)" "$(ROOT_DIR)/$(TARGET).dol"

wbfs: iso

homebrew: $(TARGET).dol
	@mkdir -p $(ROOT_DIR)/$(HB_OUT)
	@cp $(TARGET).dol $(ROOT_DIR)/$(HB_OUT)/boot.dol
	@cp meta.xml $(ROOT_DIR)/$(HB_OUT)/meta.xml
ifneq (,$(wildcard icon.png))
	@cp icon.png $(ROOT_DIR)/$(HB_OUT)/icon.png
endif
ifneq (,$(wildcard assets))
	@mkdir -p $(ROOT_DIR)/$(HB_OUT)/assets
	@cp -a assets/. $(ROOT_DIR)/$(HB_OUT)/assets/ 2>/dev/null || true
endif
	@echo "Homebrew bundle -> $(HB_OUT)/"

opening-bnr:
	@python3 $(ROOT_DIR)/scripts/generate_opening_bnr.py || python $(ROOT_DIR)/scripts/generate_opening_bnr.py

check-wit:
	@if [ -x "$(WIT_PATH)" ]; then \
		exit 0; \
	elif [ -x "$(WIT_PATH).exe" ]; then \
		exit 0; \
	else \
		echo "ERROR: 'wit' executable not found (or not executable) at:"; \
		echo "         $(WIT_PATH)"; \
		echo ""; \
		echo "       Run scripts/setup_toolchain.bat once to fetch it automatically,"; \
		echo "       or point WIT_PATH at your existing install instead, e.g.:"; \
		echo "         make iso WIT_PATH=/path/to/wit"; \
		echo ""; \
		echo "       Download WIT: https://wit.wiimm.de/"; \
		exit 1; \
	fi

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(ROOT_DIR)/Makefile $(OUTPUT).dol

run: all
	@echo "Homebrew: copy $(HB_OUT)/* to /apps/pvz_wii/ on SD card"
	@echo "Disc:     open $(ISO_OUT)/$(TARGET).iso in Dolphin"

help:
	@echo "Targets:"
	@echo "  make            Build $(TARGET).dol + output/homebrew/"
	@echo "  make iso        Build .dol + output/disc/$(TARGET).iso and .wbfs"
	@echo "  make homebrew   Refresh output/homebrew/ only"
	@echo "  make opening-bnr Generate disc/assets/opening.bnr"
	@echo "  make clean      Remove build + output artifacts"
	@echo ""
	@echo "WIT_PATH is currently: $(WIT_PATH)"
	@echo "  Override with: make iso WIT_PATH=/path/to/wit"

clean: clean-iso
	@echo clean ...
	@rm -fr $(ROOT_DIR)/$(BUILD) $(ROOT_DIR)/$(TARGET).elf $(ROOT_DIR)/$(TARGET).dol $(ROOT_DIR)/$(TARGET).map

clean-iso:
	@rm -fr $(ROOT_DIR)/$(STAGING)
	@rm -fr $(ROOT_DIR)/$(HB_OUT)/boot.dol $(ROOT_DIR)/$(HB_OUT)/meta.xml $(ROOT_DIR)/$(HB_OUT)/icon.png $(ROOT_DIR)/$(HB_OUT)/assets
	@rm -fr $(ROOT_DIR)/$(ISO_OUT)/$(TARGET).iso $(ROOT_DIR)/$(ISO_OUT)/$(TARGET).wbfs

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
# Inner Makefile (object compilation - Phase 2)
#---------------------------------------------------------------------------------
else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).elf: $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $(OFILES) $(LIBPATHS) $(LIBS) -Wl,-Map,$(notdir $@).map

$(OUTPUT).dol: $(OUTPUT).elf
	elf2dol $< $@

%.o: %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $(DEPSDIR)/$@

%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $(DEPSDIR)/$@

%.o: %.s
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $(DEPSDIR)/$@

%.o: %.bin
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

endif