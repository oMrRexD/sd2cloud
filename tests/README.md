# Tests

SD2Cloud writes to memory cards, so what it does to one is checked by tests that run on their own. There are two
kinds, because most of the program can be tested without a PlayStation 2 and the rest can't.

## The engine, on any PC

The parts of the program that have no screen and need no PS2 hardware are built with the PC's own compiler and run
right there:

```
make -C tests
```

It needs a C compiler, `make` and zlib (on Debian or Ubuntu: `sudo apt install build-essential zlib1g-dev`), and
Python 3 if it is there, for one last check. No PS2 toolchain. GitHub Actions runs the same command on every push
and pull request, and nothing is published when a check fails.

What is covered:

| File | What it checks |
|---|---|
| `test_mcfs.c` | The memory card's file system: a new card; a save imported from a `.psu`, exported to one, copied to another card and deleted; a card with no room; an import given up halfway; a file that isn't a `.psu`; what changes a card's fingerprint and its root folder's signature |
| `test_card_file.c` | A card file installed on the microSD: as a new card and over a smaller, an equal and a bigger one; from memory and, when it doesn't fit there, straight from its file; a file that stops reading (tried again, or given up); a new card cancelled; a `.ps2` with its ECC bytes; a card copied out to a `.zip` and installed back |
| `test_cards.c` | The list of cards: which files are cards and of which kind, what the settings leave out, what the state makes of each |
| `test_small.c` | The JSON reader, the `.ini` files changed in place, the settings and the state |

The cards and the saves are made by the tests themselves: nothing in this folder comes from a game.

How it works: `host/` stands in for what those parts take from the rest of the program and from the PS2 SDK (the
integer types, SHA-256, the log, the texts, a folder's listing). The "microSD" is the folder `sd/` of
`tests/build/work/`, which each test starts by emptying. `malloc` is wrapped, as the program's own is, so that a
test can say that a card doesn't fit in memory.

To see the program's own log while the tests run: `SD2CLOUD_TEST_LOG=1 make -C tests`.

### Adding a test

A test is a function in one of the `test_*.c` files, run from that file's `suite_*()` with `RUN(name)`. It checks
with `CHECK(condition)`, `CHECK_INT(got, want)` and `CHECK_STR(got, want)`, which say where they failed and go on.
`t.h` has the few helpers there are. A new `test_*.c` is picked up by the Makefile; its suite goes in `t.h` and in
the list of `run.c`.

A change to `mcfs.c`, `restore.c`, `stream.c`, `cards.c`, `config.c`, `state.c`, `json.c` or `files.c` comes with
the test that would have caught it going wrong.

## The screens, on PCSX2

What the user does on the screens (open a card, copy a save, install a card file...) is tried on the emulator, with
the debug build: it follows a `script.txt` instead of a controller, and its "microSD" is a folder of the PC.

```
make DEBUG=1
python tests/pcsx2/run.py --pcsx2 <path to pcsx2-qt.exe>
```

Each scenario of `pcsx2/scenarios.py` starts from cards made for it (`pcsx2/fixtures`), runs its script, and has to
leave what it left when it was recorded (`pcsx2/expected`): the program's own log, without what changes from one
run to the next, and the SHA-256 of every file of the "microSD". `--list` names the scenarios, `--only a,b` runs
some, `--show` prints what each one left.

It needs a PCSX2 of your own, which can't be had on GitHub (the emulator needs a PlayStation 2 BIOS): a **portable**
one, set up, with "Enable Host Filesystem" on. `run.py` refuses to start a PCSX2 that isn't portable, so that it
never touches the one you play on.

A change that makes a scenario leave something else shows as a difference. When that is what the change was for (a
new line in the log, say), look at it with `--show` and record it again with `--record`; the new expected result
goes in the same commit.

`pcsx2/fixtures` is made by the program's own code: `make -C tests fixtures` (an empty card, a card with two saves,
and a save as a `.psu` file).

## What neither reaches

The calls to Google Drive, which need an account, and the PS2 itself: the sd2psx, the USB port, the IGR of a game.
Those are tried by hand, on PCSX2 and on a console, and a pull request says how it was.
