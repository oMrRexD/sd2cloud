# SD2CLOUD-IGR.ELF (the IGR helper). Build with: make   (TEST=1: runs SD2CLOUD-TEST.ELF instead)
ifeq ($(TEST),1)
EE_BIN = igr-test-unpacked.elf
EE_BIN_PACKED = SD2CLOUD-IGR-TEST.ELF
EE_CFLAGS += -DTEST
else
EE_BIN = igr-unpacked.elf
EE_BIN_PACKED = SD2CLOUD-IGR.ELF
endif
EE_OBJS = igr.o mmceman_irx.o
EE_INCS += -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2SDK)/ports/lib
EE_LIBS = -lps2_drivers -lelf-loader-nocolour -lpatches
EE_CFLAGS += -Os -Wall

all: $(EE_BIN_PACKED)

$(EE_BIN_PACKED): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

igr.o: igr.c

mmceman_irx.c: $(PS2SDK)/iop/irx/mmceman.irx
	bin2c $< $@ mmceman_irx

clean:
	rm -f *.o *-unpacked.elf SD2CLOUD-IGR.ELF SD2CLOUD-IGR-TEST.ELF mmceman_irx.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
