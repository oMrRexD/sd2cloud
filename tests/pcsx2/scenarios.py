"""The scenarios run.py puts SD2Cloud's debug build through on PCSX2.

Each one: "name"; "what" it tries, in a line; "cards", the cards on the "microSD" when it starts (folder/name -> a
card of fixtures/: "empty", or "game", which has two saves of the game SLUS-21065); "files", other files (path ->
a file of fixtures/, or bytes); "settings", lines to add to sd2cloud.ini; "script", what is pressed (src/system.c
has the letters: X O T Q = cross, circle, triangle, square; U D < > = the arrows; S = START; . = a second's wait;
C = stop here; I at the start = started as after IGR); "seconds", how long it may take (90).

A script ends in C on the screen it should have reached: a scenario that goes somewhere else doesn't get to its C in
time, or leaves another log.
"""

# the cards most scenarios start from: two numbered cards and a game's card
CARDS = {"Card1/Card1-1": "game", "Card2/Card2-1": "empty", "SLUS-21065/SLUS-21065-1": "game"}

SCENARIOS = [
    {"name": "main-screen", "what": "the cards are found, read and listed", "cards": dict(CARDS, **{"BOOT/BootCard-1": "empty"}),
     "script": "C"},
    {"name": "save-copy", "what": "a save copied to another card", "cards": CARDS, "script": "XXXXXXC"},
    {"name": "save-move", "what": "a save moved to another card", "cards": CARDS, "script": "XXDXXXXC"},
    {"name": "save-delete", "what": "a save deleted", "cards": CARDS, "script": "XXDDXXXC"},
    {"name": "save-new-card", "what": "a save copied to a new card of its game", "cards": CARDS, "script": "XXX>XXXC"},
    {"name": "psu-import", "what": "a .psu file imported into a card", "cards": CARDS, "files": {"Files/save.psu": "save.psu"},
     "script": ">>XXXXXXXC"},
    {"name": "psu-export", "what": "a save exported as a .psu file", "cards": CARDS, "script": "XXX>>XTXXC"},
    {"name": "card-to-zip", "what": "a whole card copied to a folder, in a .zip", "cards": CARDS, "script": "TDDXXTXXXC"},
    {"name": "card-file-new", "what": "a card file installed as a new card", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXQXXXC"},
    {"name": "card-file-replace", "what": "a card file installed over a card", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXQDDXXXC"},
    {"name": "card-file-look", "what": "a card file opened to see its saves, and one of them", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXX.OOC"},
    {"name": "settings", "what": "settings changed (how many backups are kept, their format, the language) and About",
     "cards": CARDS, "script": "SDDDDDXDXDXDXUUXDXDDDDDDXOOC"},
    {"name": "igr-no-account", "what": "started as after IGR with no Google account: it leaves at once", "cards": CARDS,
     "script": "I"},
]
