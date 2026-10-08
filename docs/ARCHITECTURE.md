# How SD2Cloud is built

SD2Cloud is a PlayStation 2 program, written in C on the [ps2dev](https://github.com/ps2dev) toolchain. It runs
from the microSD of an sd2psx (or another MMCE device), lists the memory card files that device keeps there, and
backs them up to Google Drive. It also manages those cards: saves copied between them, cards installed from files,
backups restored.

This is the map of the code: where each thing is, how the main things happen, and the rules of the device that the
code is written around. Each source file says what it is for at its top.

## The folders

| Folder | What is in it |
|---|---|
| `src/main.c` | Where the program starts, and the state its screens share |
| `src/app/` | The screens and what they do, one file for each screen or flow |
| `src/core/` | The rules, with no screen and no PS2 hardware: memory cards, backups, settings, Google Drive |
| `src/platform/` | The PS2 itself: IOP modules, controller, clock, network, the MMCE device, other programs |
| `src/ui/` | Drawing: the scenes, the font, images, icons, sound, and the texts in both languages |
| `igr/` | The two small programs that live on the memory card: the IGR helper and the shortcut |
| `tests/` | The regression tests: `core` on a PC, the screens on PCSX2 ([tests/README.md](../tests/README.md)) |
| `tools/` | Scripts of the build: the ports, mmceman, the art, the sounds, the release, the texts' check |
| `package/` | What goes in the release's zip beside the programs, and the SAS package's icon and title |
| `assets/`, `third_party/` | The art and sounds the program embeds; the font and the QR code generator |

A file of `src/core` never draws and never touches the hardware, which is what lets `tests/` build that folder with
a PC's compiler. `src/app` is the only place that knows the screens; it calls down into the other three.

### `src/app`: the screens

| File | What it is |
|---|---|
| `menu.c` | The main screen: the cards in their groups, the selected one big on the right |
| `tabs.c` | The groups themselves (numbered cards, games, boot cards, files) and the big card's picture |
| `saves.c`, `save.c` | One card's saves, as the PS2 browser shows them; one save's page and what is done with it |
| `dest.c` | Where a save goes: a card picked among the others, or a new card made on the spot |
| `browse.c` | The Files group: folders of the microSD and of a USB drive, `.psu` files, exporting |
| `install.c` | A memory card file looked into, and installed as a card |
| `backup.c`, `autosync.c` | The sync to Google Drive; the one that runs by itself after a game |
| `history.c` | A card's backups on Drive, and restoring one |
| `active.c` | The card the device is using right now: finding it, switching it, noticing it change |
| `settings.c`, `account.c` | The settings; the Google account |
| `tools.c`, `keyboard.c` | The tools (SELECT): the templates, made by marking saves on the cards and put into cards; the keyboard on the screen their names are typed on |
| `leave.c` | Which program is opened when SD2Cloud closes |
| `dialog.c`, `work.c`, `format.c` | The dialog box; the screen of something being done; how sizes and dates are said |

`app.h` declares what these files use of one another. What a file keeps to itself is `static`.

### `src/core`: the rules

| File | What it is |
|---|---|
| `cards.c` | Finds the card files on the microSD and tells their kind |
| `mcfs.c` | The memory card's file system, read and written inside a card's file: saves, icons, the fingerprint |
| `stream.c` | A card read and turned into a `.zip`, piece by piece, without ever holding it whole |
| `google.c` | Google Drive: the sign-in with a code, folders, the resumable upload, downloads |
| `restore.c` | A card written back from a backup, or installed from a file |
| `templates.c` | The templates: sets of saves kept on the microSD as `.psu` files, and put into cards |
| `config.c`, `state.c` | The settings (`sd2cloud.ini`); what is remembered between runs (`state.ini`, `token.dat`) |
| `update.c` | Updating the program from its GitHub releases |
| `files.c`, `json.c` | Whole files and `.ini` files; the little of JSON the answers need |

Each has its header (`mcfs.h`, `cards.h`...). `src/common.h` includes them all.

## How it starts

`main()` calls `system_init()` (`platform/boot.c`), which resets the IOP, loads the drivers, finds the microSD and
the device, and reads the settings. Then one of two things happens:

- opened by the user: `manual()` (`app/menu.c`) lists the cards and shows the main screen;
- opened by the IGR helper, with `-igr`, after a game: `igr()` (`app/autosync.c`) syncs what changed and leaves.

## What a backup does

1. `cards_scan()` lists the card files; `cards_check()` computes each one's **fingerprint**: a SHA-256 of the
   card's file system index (`mcfs_fingerprint`), which reads a few hundred KB of the card instead of all of it.
2. The fingerprint is compared with the one in `state.ini`: a card is new, changed, or up to date.
3. For each card to send, `stream_zip()` reads the card in blocks, hashes it, deflates it into a `.zip`, and hands
   the `.zip` out in 1 MiB pieces. `google_upload()` sends each piece as it comes (a resumable upload whose total
   size isn't known at the start). A PS2 has 32 MB of memory and a card can be 128 MB: nothing holds a card whole.
4. The backup's SHA-256 and fingerprint are written to the Drive file's properties and to `state.ini`; backups past
   the limit kept per card are deleted.

Restoring (`restore_card`) downloads the `.zip` to memory, inflates it once only to check it against the SHA-256
recorded with it, then inflates it over the card and reads the card back.

## The screens

`ui/ui.c` draws on a thread of its own: the current **scene** (a `scene_*` function) every frame. The main thread
does the work and changes what the scene shows, under `ui_lock()`. So a long operation keeps the screen alive
without drawing anything itself: it updates a few variables and calls a progress function.

The main thread waits for the controller in `wait_button()` and `wait_nav()` (`platform/input.c`). While it waits,
`idleHook` runs: that is how `device_watch()` (`app/active.c`) notices the card changed on the device itself, or the
device taken out.

Every text on screen is in `ui/messages.def`, in English and in Portuguese; `tools/check_messages.py` checks that
each one fits where it is drawn.

## The rules of the device

The code is shaped by what an sd2psx can and can't do. Breaking one of these corrupts a card or hangs the console.

- **One file open at a time.** The device can't handle two operations at once. A copy between two of its files
  opens one, reads a piece, closes it, opens the other. Only a file on another device (a USB drive) stays open.
- **No renaming.** `file_replace()` writes a `.new` file, reads it back, and writes the real one again.
- **The card in use is not the file.** The device keeps the card it is emulating in its own memory and writes back
  only what the PS2 changes. Writing that card's file under it would mix the two, so before a card's file is
  changed, `card_free()` moves the device to another card, or the user is told.
- **Which card is in use** is asked of the device (`mmce_active_card`), and checked against the slot: the root
  folder the PS2 sees there has to be the one inside the card's file (`mc_root_signature`, `mcfs_root_signature`).
- **Writing is slow, and a growing file slower.** The device syncs a file after every 4 KB it is given. A card that
  is replaced by one of its size is written over in place.
- **What is written is read back.** A card, a save, a `.psu`, a settings file: each is read again and compared
  before the program says it is done.

## What it writes, and where

On the microSD: `SD2Cloud/sd2cloud.ini` (the settings, the user's file), `SD2Cloud/state.ini` (what was backed up),
`SD2Cloud/token.dat` (the access to Google), `SD2Cloud/templates/` (a folder for each template, a `.psu` for each
of its saves, and `templates.ini`: which one is the main template and which cards are settled with it), the cards
themselves under `MemoryCards/PS2/`, and whatever the user exports. On the memory card in use, only when the automatic sync is turned on: the `APP_SD2CLOUD` save folder (the
SAS package: the IGR helper and a shortcut), written by `platform/helper.c` through the PS2's own memory card driver.

## The debug build

`make DEBUG=1` builds `SD2CLOUD-DEBUG.ELF`: the same program, which keeps a log, uses a state file and a Drive
folder of its own, and can be driven by a `script.txt` in the `SD2Cloud` folder instead of a controller
(`platform/debug.c` has the letters). On PCSX2 the microSD is the `host:` device, a folder of the PC. That is what
the scenarios of `tests/pcsx2` are made of.

## Building and releasing

The `Makefile` builds the program; the art, the sounds, the font, the IOP modules and the two programs of `igr/`
are embedded in it as C arrays. GitHub Actions (`.github/workflows/build.yml`) builds every push and pull request
in a pinned image of the toolchain and runs the tests; a push to `main` publishes the `beta` pre-release, and a
`v*` tag publishes a release. Nothing is published when a test fails.
