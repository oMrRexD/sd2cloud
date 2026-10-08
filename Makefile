# SD2Cloud (PS2 app). Needs the ps2dev toolchain (ps2sdk, ps2sdk-ports, gsKit) with PS2DEV/PS2SDK/GSKIT set.
# Once, before the first build:
#   - tools/build_ports.sh: wolfSSL + curl with 4096-bit RSA in ports4096/ (without it Google's HTTPS fails with -155);
#   - tools/build_mmceman.sh: the sd2psx driver in third_party/mmceman/ (the SDK's can hang the sd2psx);
#   - src/credentials.h: python tools/make_credentials.py <client_secret.json> src/credentials.h (Google Cloud OAuth
#     client of type "TVs and Limited Input devices").
# make         -> dist/SD2CLOUD.ELF and dist/SD2CLOUD-IGR.ELF (the IGR helper, built in igr/)
# make DEBUG=1  -> dist/SD2CLOUD-DEBUG.ELF: "(debug)" folder on Drive, keeps 3 backups, a script.txt presses the buttons
#                 (without a script it runs as IGR)
# Then python tools/make_release.py packages the release in dist/.
PORTS4096 ?= ports4096
MMCEMAN ?= third_party/mmceman/mmceman.irx
DIST = dist

ifeq ($(DEBUG),1)
OBJ_DIR = obj-debug
EE_BIN = $(OBJ_DIR)/sd2cloud-debug-unpacked.elf
EE_BIN_PACKED = $(DIST)/SD2CLOUD-DEBUG.ELF
EE_CFLAGS += -DDEBUG_BUILD
else
OBJ_DIR = obj
EE_BIN = $(OBJ_DIR)/sd2cloud-unpacked.elf
EE_BIN_PACKED = $(DIST)/SD2CLOUD.ELF
endif
SOURCES = main system ui font image look sound files json i18n config state cards mcfs stream google helper update restore icon
HEADERS = $(addprefix src/, common.h messages.h messages.def ui.h font.h image.h icon.h look.h sound.h)
# the interface's images, sounds and font, embedded in the program (assets/ is made by tools/make_assets.py; the sounds
# by tools/make_sounds.py)
IMAGES = space glow buttons card minicard
SOUNDS = startup exit move confirm back
# and the games' names by their ID (assets/gamenames.txt, made by tools/make_gamenames.py)
ASSET_OBJS = $(addprefix asset_, $(addsuffix _png.o, $(IMAGES)) $(addsuffix _adp.o, $(SOUNDS))) asset_font_ttf.o \
             asset_gamenames_txt.o
# the IOP modules embedded in the program: mmceman (the sd2psx), and the drivers of the other devices a program can be
# opened from after SD2Cloud (loaded only for that, see run_elf)
EMBEDDED_IRX = mmceman mcman mcserv usbd usbmass_bd bdm bdmfs_fatfs mx4sio_bd ata_bd ps2dev9 ps2atad ps2hdd ps2fs
EE_OBJS = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(SOURCES)) qrcodegen.o cacert_pem.o ini_default.o $(ASSET_OBJS) \
          $(addprefix irx_, $(addsuffix .o, $(EMBEDDED_IRX))))
EE_INCS += -Isrc -I. -Ithird_party/qrcodegen -I$(PORTS4096)/include -I$(PS2SDK)/ports/include \
           -I$(PS2SDK)/ports/include/freetype2 -I$(GSKIT)/include
# malloc and memalign go through system.c, which zeroes what the network libraries take while they start
EE_LDFLAGS += -L$(PORTS4096)/lib -L$(PS2SDK)/ports/lib -L$(GSKIT)/lib -Wl,--wrap=malloc -Wl,--wrap=memalign
EE_LIBS = -lcurl -lwolfssl -lfreetype -lpng -lz -lsocket -lps2_drivers -laudsrv -lmc -lelf-loader-nocolour -lpatches -lgskit -ldmakit -ldebug -lcdvd -lpthread -lpthreadglue -lm
EE_CFLAGS += -Os -Wall -Wno-format-truncation $(EXTRA_CFLAGS)
# the commit the program is built from, shown in "About": the first 7 characters of its hash (empty when it isn't
# known). It goes into the program, so two builds only come out the same when they are given the same one: GitHub
# Actions passes its own, and so must a build made to compare hashes with it
APP_COMMIT ?= $(shell git rev-parse HEAD 2>/dev/null | cut -c1-7)
EE_CFLAGS += -DAPP_COMMIT=\"$(APP_COMMIT)\"

.PHONY: all igr clean
all: $(PORTS4096)/lib/libwolfssl.a $(MMCEMAN) src/credentials.h $(EE_BIN_PACKED)
ifneq ($(DEBUG),1)
all: igr
endif

$(PORTS4096)/lib/libwolfssl.a:
	@echo "wolfSSL with 4096-bit RSA is missing: run tools/build_ports.sh first (or set PORTS4096)." && false

$(MMCEMAN):
	@echo "mmceman is missing: run tools/build_mmceman.sh first (or set MMCEMAN)." && false

src/credentials.h:
	@echo "src/credentials.h is missing: python tools/make_credentials.py <client_secret.json> src/credentials.h" && false

$(EE_BIN_PACKED): $(EE_BIN)
	@mkdir -p $(DIST)
	ps2-packer $< $@ > /dev/null

# the IGR helper: its own small program, in igr/ (its debug variant is built there with make DEBUG=1, after make clean)
igr: $(MMCEMAN)
	$(MAKE) -C igr MMCEMAN=$(abspath $(MMCEMAN))
	@mkdir -p $(DIST)
	cp igr/SD2CLOUD-IGR.ELF $(DIST)/

$(OBJ_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(OBJ_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(OBJ_DIR)/qrcodegen.o: third_party/qrcodegen/qrcodegen.c
	@mkdir -p $(OBJ_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# embedded files: bin2c turns each one into a C array
$(OBJ_DIR)/asset_%_png.c: assets/%.png
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ asset_$*_png

$(OBJ_DIR)/asset_%_adp.c: assets/sounds/%.adp
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ asset_$*_adp

$(OBJ_DIR)/asset_gamenames_txt.c: assets/gamenames.txt
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ asset_gamenames_txt

$(OBJ_DIR)/asset_font_ttf.c: third_party/varelaround/VarelaRound-Regular.ttf
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ asset_font_ttf

$(OBJ_DIR)/irx_%.c: $(PS2SDK)/iop/irx/%.irx
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ $*_irx

# mmceman is the one built by tools/build_mmceman.sh, not the SDK's: that one is older than the sio2man built into
# ps2_drivers, and the pair can hang the sd2psx in a long transfer
$(OBJ_DIR)/irx_mmceman.c: $(MMCEMAN)
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ mmceman_irx

# the HDD driver variant that also handles the PS2's own (OSD) partitions
$(OBJ_DIR)/irx_ps2hdd.c: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ ps2hdd_irx

# Mozilla's root certificates (https://curl.se/ca/cacert.pem)
$(OBJ_DIR)/cacert_pem.c: src/cacert.pem
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ cacert_pem

# the sd2cloud.ini the app creates when there is none: the same file that goes in the zip
$(OBJ_DIR)/ini_default.c: package/sd2cloud.ini
	@mkdir -p $(OBJ_DIR)
	bin2c $< $@ ini_default

$(OBJ_DIR)/ini_default.o: $(OBJ_DIR)/ini_default.c
	$(EE_CC) -c $< -o $@

$(OBJ_DIR)/asset_%.o: $(OBJ_DIR)/asset_%.c
	$(EE_CC) -c $< -o $@

$(OBJ_DIR)/irx_%.o: $(OBJ_DIR)/irx_%.c
	$(EE_CC) -c $< -o $@

$(OBJ_DIR)/cacert_pem.o: $(OBJ_DIR)/cacert_pem.c
	$(EE_CC) -c $< -o $@

clean:
	rm -rf obj obj-debug $(DIST)/SD2CLOUD.ELF $(DIST)/SD2CLOUD-DEBUG.ELF
	$(MAKE) -C igr clean

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
