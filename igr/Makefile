# SD2CLOUD-IGR.ELF (the IGR helper) and SD2CLOUD-OPEN.ELF (the shortcut: the same program built with -DOPEN, which
# opens SD2Cloud the way the user would). Build with: make   (DEBUG=1: the -DEBUG ones, which run SD2CLOUD-DEBUG.ELF)
# mmceman is the one built by tools/build_mmceman.sh (see the main Makefile)
MMCEMAN ?= ../third_party/mmceman/mmceman.irx
ifeq ($(DEBUG),1)
V = -debug
EE_BIN_PACKED = SD2CLOUD-IGR-DEBUG.ELF
OPEN_PACKED = SD2CLOUD-OPEN-DEBUG.ELF
EE_CFLAGS += -DDEBUG_BUILD
else
V =
EE_BIN_PACKED = SD2CLOUD-IGR.ELF
OPEN_PACKED = SD2CLOUD-OPEN.ELF
endif
EE_BIN = igr$(V)-unpacked.elf
OPEN_BIN = open$(V)-unpacked.elf
EE_OBJS = igr$(V).o mmceman_irx.o
OPEN_OBJS = open$(V).o mmceman_irx.o
EE_INCS += -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2SDK)/ports/lib
EE_LIBS = -lps2_drivers -lelf-loader-nocolour -lpatches
EE_CFLAGS += -Os -Wall

all: $(EE_BIN_PACKED) $(OPEN_PACKED)

$(EE_BIN_PACKED): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

$(OPEN_PACKED): $(OPEN_BIN)
	ps2-packer $< $@ > /dev/null

igr$(V).o: igr.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

open$(V).o: igr.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -DOPEN -c $< -o $@

# the shortcut also has the text screen, for when SD2Cloud isn't where it looks
$(OPEN_BIN): $(OPEN_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $(OPEN_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS) -ldebug

mmceman_irx.c: $(MMCEMAN)
	bin2c $< $@ mmceman_irx

clean:
	rm -f *.o *-unpacked.elf SD2CLOUD-IGR.ELF SD2CLOUD-IGR-DEBUG.ELF SD2CLOUD-OPEN.ELF SD2CLOUD-OPEN-DEBUG.ELF mmceman_irx.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
