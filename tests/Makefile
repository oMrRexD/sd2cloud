# SD2Cloud's regression tests: the parts of the program that need no PS2 (the memory card's file system, a card's
# .zip, a card file's way onto the microSD, the settings, the list of cards) built with the PC's own compiler, on
# stand-ins for the rest (host/), and run right here. See README.md.
#
#   make -C tests            builds and runs them (also: make test, from the project's folder)
#   SD2CLOUD_TEST_LOG=1      shows the program's own log while they run
#   make -C tests fixtures   makes again the cards the PCSX2 scenarios start from (pcsx2/fixtures)
SRC = ../src
CORE = $(SRC)/core
HEADERS = $(wildcard $(SRC)/*.h $(SRC)/*/*.h) $(SRC)/ui/messages.def
ENGINE = mcfs stream restore cards config state json files templates
TESTS = $(basename $(wildcard test_*.c))
OUT = build

CC ?= cc
CFLAGS += -O1 -g -Wall -Wno-format-truncation -Ihost -I$(SRC) -I$(CORE) -I$(SRC)/platform -I$(SRC)/ui -I.
LDFLAGS += -Wl,--wrap=malloc
LDLIBS += -lz

COMMON = $(addprefix $(OUT)/, $(addsuffix .o, $(ENGINE) util host sha256))
OBJS = $(COMMON) $(addprefix $(OUT)/, $(addsuffix .o, $(TESTS) run))
FIXTURES = pcsx2/fixtures

.PHONY: test fixtures clean
test: $(OUT)/run_tests
	@mkdir -p $(OUT)/work
	cd $(OUT)/work && ../run_tests
	@if command -v python3 > /dev/null; then python3 check_zip.py $(OUT)/work; fi

$(OUT)/run_tests: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# (gzip -n: no name and no date inside, so that the same card is the same file)
fixtures: $(OUT)/make_fixtures
	mkdir -p $(FIXTURES)
	cd $(FIXTURES) && ../../$(OUT)/make_fixtures && gzip -9 -n -f empty.mcd game.mcd

$(OUT)/make_fixtures: $(COMMON) $(OUT)/make_fixtures.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OUT)/%.o: $(CORE)/%.c $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: %.c t.h $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: host/%.c $(HEADERS) | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT):
	mkdir -p $(OUT)

clean:
	rm -rf $(OUT)
