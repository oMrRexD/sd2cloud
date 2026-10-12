# Contributing to SD2Cloud

Thank you for helping. SD2Cloud writes to memory cards, so the main rule is care: a change that touches the cards
has to be tested, on copies of them, before it is sent.

You can write issues and pull requests in English or in Portuguese.

## Reporting a bug

Open an [issue](../../issues/new/choose) with the bug form. What helps the most:

- the SD2Cloud version (in Settings, next to About SD2Cloud);
- the console, the sd2psx device and its firmware version;
- how SD2Cloud was started: which program opened it and where SD2Cloud is stored;
- what you did, step by step, what happened and what you expected instead;
- the exact text of an error message, or a photo of the screen.

If saved data was damaged, say which operation was running (sync, restore, copy, move, delete) and keep a copy of
the affected `.mcd` file.

## Suggesting something

Use the suggestion form in the same place. Say what you would like SD2Cloud to do and in which situation you missed
it.

## Translating

SD2Cloud's texts are in [lang/](lang): a plain text file per language, `en.txt` being the original. Every language
there is built into the program, so a translation needs nothing from the people who use it: they pick it in
Settings > Language, or the program follows the PS2's own language.

Each text is a line `ID = text`, and the line above it says what it is (a button, one line, a message of up to some
lines) and, in a translation, the English text:

```
# One line. English: Searching for memory cards...
T_SEARCHING = Buscando tarjetas de memoria...
```

- Change only what comes after `=`. The IDs, `LANGUAGE_NAME`, `DATE_FORMAT` and `CONFIRM_BUTTON` stay as they are
  (`DATE_FORMAT` says how the language writes a date: `DMY`, `MDY` or `YMD`, or with a separator of its own, like
  `D.M.Y` for 31.12.2026; `CONFIRM_BUTTON` is `CROSS`, or `CIRCLE` where the circle confirms and the cross goes
  back, as in Japan).
- `%s` and `%d` (also `%lld`, `%02d`) are filled in by the program, with a card's name or a number: keep each one,
  in the same order.
- `\n` starts a new line on the screen; a text that starts with a space starts with `\ `. A text in Japanese may
  break between any two of its characters (never before 、。」 and the like), so it needs no spaces.
- The screen is a TV's, and small: each text has to fit its place. Keep the texts about as long as the English ones.
  A row of buttons too long for its line is written in smaller letters, which still fits most of them.
- A text left empty is shown in English.

To improve a translation, edit its file (GitHub's editor in the browser does it: the pencil on the file's page) and
open a pull request. GitHub checks every pull request with `tools/check_messages.py`: that no text is missing or
unknown, that `%s` and `%d` match the English ones, and that each text fits on the screen. When a check fails, its
log says which text and by how much. Once a pull request is merged, the [beta](../../releases/tag/beta) has it within
minutes.

To add a language, copy `en.txt` to a file named by the language's code, in lowercase letters only (`fr.txt`,
`it.txt`), write its name in that language in `LANGUAGE_NAME` (`Français`), and translate. The program picks it up
by itself; `fr`, `de`, `it`, `nl` and `ja` are also chosen when the PS2 is set to that language. The font has the
Latin letters with their accents, the Cyrillic ones, and the kana and common kanji of Japanese (no Chinese or Korean
characters for now: `check_messages.py` says which letters it lacks; a kanji `lang/ja.txt` needs beyond the common
ones goes into the font with `python tools/make_font.py`).

The README has a page in each language too (`README.es.md`, `README.ja.md`...), with the screens of the program in
that language (`docs/screens/*-<code>.png`) and its logo (`docs/logo/<code>/`). A new language's page is welcome, but
not needed for the language to be in the program; the line of languages at the top of every page then gets it too.

## Sending a pull request

1. Fork the repository and create a branch from `main`.
2. Find your way: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) is the map of the code, and each source file says
   what it is for at its top.
3. Build it: the steps are in the [README](README.md#building). GitHub Actions also builds every pull request; that
   build has no Google credentials, so it only shows that the code compiles.
4. Run the tests: `make -C tests` needs no PS2 toolchain, and GitHub Actions runs it too. A change to the parts it
   covers comes with a test of it; a change to a screen is tried with the scenarios on PCSX2, if you have one. Both
   are in [tests/README.md](tests/README.md).
5. Say in the pull request how you tried it: `make DEBUG=1` builds a debug version for PCSX2, and on a real console
   say which sd2psx device and firmware were used.
6. Keep one subject per pull request.

Conventions of the code:

- Code, comments, file names and commit messages are in English.
- Every text shown on screen has an id in `src/ui/messages.def` and its text in `lang/en.txt` and `lang/pt.txt`.
  After adding one, `python tools/check_messages.py --sync` puts it in the other languages' files, to be translated,
  and `python tools/check_messages.py` checks that each text fits where it is drawn.
- Follow the style of the code around your change (4 spaces, lines up to 120 columns: `.editorconfig` has it), and
  keep the build free of warnings.
- A function or a variable that only its own file uses is `static`. What another file uses is declared in that
  module's header (`src/app/app.h` for the screens).

Contributions are accepted under the project's license, the [GNU General Public License v3](LICENSE).
