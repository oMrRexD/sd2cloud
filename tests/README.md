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

## What these tests don't reach

The screens, the Google Drive calls and the PS2 itself. The screens and the calls to Google are tried on PCSX2 with
the debug build (`make DEBUG=1`), which a `script.txt` in the `SD2Cloud` folder can drive; the sd2psx, the USB port
and the IGR of a game only on a console.
