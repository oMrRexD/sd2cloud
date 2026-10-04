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

## Sending a pull request

1. Fork the repository and create a branch from `main`.
2. Build it: the steps are in the [README](README.md#building). GitHub Actions also builds every pull request; that
   build has no Google credentials, so it only shows that the code compiles.
3. Test it and say in the pull request how: `make DEBUG=1` builds a debug version for PCSX2, and on a real console
   say which sd2psx device and firmware were used.
4. Keep one subject per pull request.

Conventions of the code:

- Code, comments, file names and commit messages are in English.
- Every text shown on screen exists in English and in Portuguese. The texts are in `src/messages.def`, and
  `python tools/check_messages.py` checks that each one fits where it is drawn.
- Follow the style of the code around your change, and keep the build free of warnings.

Contributions are accepted under the project's license, the [GNU General Public License v3](LICENSE).
