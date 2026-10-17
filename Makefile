# Cube Menu - libogc2 (devkitPPC). Run through ./build.sh (docker) or with DEVKITPRO set up.
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment, or use ./build.sh")
endif
include $(DEVKITPRO)/libogc2/gamecube_rules

TARGET   := cubemenu
BUILD    := build
SOURCES  := source
DATA     := data
export VERSION ?= $(shell cat $(CURDIR)/VERSION 2>/dev/null || echo dev)

CFLAGS   = -O2 -Wall -Wno-unused-function -Wno-format-truncation -std=gnu11 $(MACHDEP) $(INCLUDE) -DCUBEMENU_VERSION=\"$(VERSION)\"
LDFLAGS  = $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS     := -lfat -liso9660 -laesnd -lmad -logc -lm
LIBDIRS  := $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/dist/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))
export LD := $(CC)
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES := $(OFILES_BIN) $(CFILES:.c=.o) $(SFILES:.S=.o)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD) -I$(CURDIR)/source -I$(LIBOGC_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBOGC_LIB)
.PHONY: $(BUILD) clean
$(BUILD):
	@mkdir -p $@ dist
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) dist/$(TARGET).elf dist/$(TARGET).dol
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SOURCES) : $(HFILES)
$(CFILES:.c=.o): $(HFILES)
%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)
-include $(DEPENDS)
endif
