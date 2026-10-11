## What this changes


## How it was tested

<!-- PCSX2 (make DEBUG=1) or a real console; on a console, which sd2psx device and firmware. -->

## Checklist

- [ ] It builds without warnings (`make`)
- [ ] The tests pass (`make -C tests`)
- [ ] Code, comments and names are in English
- [ ] New or changed on-screen texts are in `lang/en.txt` and `lang/pt.txt` (`python tools/check_messages.py --sync` puts new ones in the other languages), and `python tools/check_messages.py` passes
- [ ] A change that writes to memory cards was tested on copies of the cards
