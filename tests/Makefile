# SD2Cloud's regression tests: the parts of the program that need no PS2 (the memory card's file system, a card's
# .zip, a card file's way onto the microSD, the settings, the list of cards) built with the PC's own compiler, on
# stand-ins for the rest (host/), and run right here. See README.md.
#
#   make -C tests          builds and runs them (also: make test, from the project's folder)
#   SD2CLOUD_TEST_LOG=1    shows the program's own log while they run
SRC = ../src
ENGINE = mcfs stream restore cards config state json files
TESTS = $(basename $(wildcard test_*.c))
OUT = build

CC ?= cc
CFLAGS += -O1 -g -Wall -Wno-format-truncation -Ihost -I$(SRC) -I.
LDFLAGS += -Wl,--wrap=malloc
LDLIBS += -lz

OBJS = $(addprefix $(OUT)/, $(addsuffix .o, $(ENGINE) $(TESTS) run host sha256))

.PHONY: test clean
test: $(OUT)/run_tests
	@mkdir -p $(OUT)/work
	cd $(OUT)/work && ../run_tests
	@if command -v python3 > /dev/null; then python3 check_zip.py $(OUT)/work; fi

$(OUT)/run_tests: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OUT)/%.o: $(SRC)/%.c $(wildcard $(SRC)/*.h) $(SRC)/messages.def | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: %.c t.h $(wildcard $(SRC)/*.h) | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: host/%.c $(wildcard $(SRC)/*.h) $(SRC)/messages.def | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT):
	mkdir -p $(OUT)

clean:
	rm -rf $(OUT)
