"""The scenarios run.py puts SD2Cloud's debug build through on PCSX2.

Each one: "name"; "what" it tries, in a line; "cards", the cards on the "microSD" when it starts (folder/name -> a
card of fixtures/: "empty", or "game", which has two saves of the game SLUS-21065); "files", other files (path ->
a file of fixtures/, or bytes); "settings", lines to add to sd2cloud.ini; "script", what is pressed
(src/platform/debug.c has the letters: X O T Q = cross, circle, triangle, square; U D < > = the arrows; S = START;
L = SELECT; . = a second's wait; C = stop here; I at the start = started as after IGR; t at the start = the templates
are put into the game cards as after a game, SD2Cloud/active.txt naming the card in the device); "seconds", how long
it may take (90).

A script ends in C on the screen it should have reached: a scenario that goes somewhere else doesn't get to its C in
time, or leaves another log.
"""

import pathlib


def psu_folder(psu, folder):
    """the files of a .psu of fixtures/ as a folder of them: a save copied off a card the way a file manager copies it
    to a USB drive (path -> bytes, for "files"). The saves of fixtures/ have no icon.sys, which tells such a folder"""
    d = (pathlib.Path(__file__).parent / "fixtures" / psu).read_bytes()
    files, at = {}, 512
    for _ in range(int.from_bytes(d[4:8], "little")):
        e, at = d[at:at + 512], at + 512
        if e[0] & 0x20:   # "." and "..": folders
            continue
        size = int.from_bytes(e[4:8], "little")
        files[folder + "/" + e[64:96].split(b"\0")[0].decode()] = d[at:at + size]
        at += (size + 1023) & ~1023
    return files


# the cards most scenarios start from: two numbered cards and a game's card
CARDS = {"Card1/Card1-1": "game", "Card2/Card2-1": "empty", "SLUS-21065/SLUS-21065-1": "game"}
# an OPL VMC on the microSD, and the settings of the game that uses it
VMC_FILES = {"VMC/SLUS-21065_0.bin": "game.mcd", "VMC/Extra_0.bin": "empty.mcd",
             "CFG/SLUS_210.65.cfg": b"$ConfigSource=1\r\n$VMC_0=SLUS-21065_0\r\n$Compatibility=0\r\n"}
# what the app remembers of the empty card: never sent, marked as resolved as it is (its fingerprint, of no saves)
RESOLVED = (b"[Card2/Card2-1]\r\nfingerprint = \r\nfingerprint_version = 0\r\nsha256 = \r\nwhen = \r\n"
            b"resolved = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\r\n")
# the sd2psx's own settings, as its firmware writes them on the microSD
SD2PSX_SETTINGS = (b"[General]\nMode=PS2\nFlippedScreen=OFF\n[PS1]\nAutoboot=ON\nGameID=ON\nEnableControllerCombo=ON\n"
                   b"MaxCardIdx=0\n[PS2]\nAutoboot=OFF\nGameID=ON\nCardSize=8\nVariant=RETAIL\nMaxCardIdx=0\n")
# a template that is there when a scenario starts: "Net", with the save of fixtures/save.psu
TEMPLATE = {"SD2Cloud/templates/Net/save.psu": "save.psu"}

SCENARIOS = [
    {"name": "main-screen", "what": "the cards are found, read and listed", "cards": dict(CARDS, **{"BOOT/BootCard-1": "empty"}),
     "script": "C"},
    # "Copy" on a save's page marks it. Others can be marked, there and on the other cards, which are listed when
    # that card's saves are left (O). START (S) pastes them into the card it is pressed on: the one under the cursor
    # in that list, or the one whose saves are open. On the card they are all on, it goes to that list too. In the
    # Games group of the list, their game's folder comes first, and "New card" first inside it (X D: its card)
    {"name": "save-copy", "what": "a save copied to another card: START on its own card goes to the list of cards, START "
     "there pastes it", "cards": CARDS, "script": "XXXSDSXXC"},
    {"name": "save-copy-many", "what": "saves marked on two cards, copied together to a third", "cards": CARDS,
     "script": "XXXO>XDX>XO<DSXXC"},
    {"name": "save-copy-list", "what": "a save marked on a card, pasted on another card of the list", "cards": CARDS,
     "script": "XXXODSXXC"},
    {"name": "save-paste-inside", "what": "a save marked on a card, pasted with another card's saves open", "cards": CARDS,
     "script": "XXXODXSXXC"},
    {"name": "save-export-many", "what": "two saves marked on a card, exported together as .psu files", "cards": CARDS,
     "script": "XXX>XS>>XSXXC"},
    {"name": "save-move", "what": "a save moved to another card", "cards": CARDS, "script": "XXDXXXXC"},
    {"name": "save-delete", "what": "a save deleted", "cards": CARDS, "script": "XXDDXXXC"},
    # (after "New card", its size: the cursor starts on the one recommended for what goes into it, and X takes it)
    {"name": "save-new-card", "what": "a save copied to a new card of its game: \"New card\" in its game's folder, in the list "
     "of cards", "cards": CARDS, "script": "XXXS>XXXXXC"},
    {"name": "save-new-card-size", "what": "a save copied to a new card of its game, of a size other than the one "
     "recommended (2 MB)", "cards": CARDS, "script": "XXXS>XXDXXXC"},
    # (no card of the game yet: "New card" among the folders, first)
    {"name": "save-new-game", "what": "a save copied to a new card of a game that has no card yet", "cards":
     {"Card1/Card1-1": "game", "Card2/Card2-1": "empty", "SLUS-20001/SLUS-20001-1": "empty"}, "script": "XXXS>SXXXC"},
    # (the start's warning answered with square: that card is left alone)
    {"name": "save-new-card-template", "what": "a save copied to a new card of its game: the main template goes into it too",
     "cards": CARDS, "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n"}),
     "script": "QXXXS>XXXXXC"},
    {"name": "psu-import", "what": "a .psu file imported into a card", "cards": CARDS, "files": {"Files/save.psu": "save.psu"},
     "script": ">>XXXXXXXC"},
    # (circle on the question of where it goes: the cards as they were left, not from the start)
    {"name": "psu-import-back", "what": "a .psu's card picked, its question answered with circle: back on that card, "
     "picked again and imported", "cards": CARDS, "files": {"Files/save.psu": "save.psu"}, "script": ">>XXXXDXOXXXC"},
    {"name": "save-move-back", "what": "a save's move: a game card picked in its folder, the question answered with "
     "circle, back on that card in its folder, moved", "cards": CARDS, "script": "XXDX>XDXOXXXC"},
    {"name": "save-copy-back", "what": "a copy's paste on a card of the list given up at its question: the list as it "
     "was, pasted there", "cards": CARDS, "script": "XXXSDSOSXXC"},
    {"name": "psu-folder-import", "what": "a save copied as a folder (its files, as on the card): square on it, its page, installed into a card",
     "cards": CARDS, "files": dict(psu_folder("save.psu", "Files/BASLUS-21065GAMMA"), **{"Files/BASLUS-21065GAMMA/icon.sys": b"PS2D"}),
     "script": ">>XXQXXXXC"},
    {"name": "psu-folder-enter", "what": "X on a folder with an icon.sys goes into it, as into any folder; square there "
     "installs the folder", "cards": CARDS,
     "files": dict(psu_folder("save.psu", "Files/BASLUS-21065GAMMA"), **{"Files/BASLUS-21065GAMMA/icon.sys": b"PS2D"}),
     "script": ">>XXXQXXXXC"},
    {"name": "psu-folder-not-save", "what": "square on a folder of something else that has an icon.sys: refused, no card touched",
     "cards": CARDS, "files": {"Files/DVD/icon.sys": b"PS2D", "Files/DVD/Game/a.iso": b"x"}, "script": ">>XXQXC"},
    {"name": "psu-export", "what": "a save exported as a .psu file", "cards": CARDS, "script": "XXXS>>XSXXC"},
    {"name": "card-to-zip", "what": "a whole card copied to a folder, in a .zip", "cards": CARDS, "script": "TDDDXXTXXXC"},
    {"name": "card-file-new", "what": "a card file installed as a new card", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXQXXXC"},
    {"name": "card-file-replace", "what": "a card file installed over a card", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXQDDXXXC"},
    {"name": "card-file-look", "what": "a card file opened to see its saves, and one of them", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXX.OOC"},
    {"name": "card-file-copy", "what": "two saves of a card file marked there, and copied to a card", "cards": CARDS,
     "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXXX>XSOODSXXC"},
    # (the folders are browsed over the ones the card file was opened from, which come back as they were)
    # (circle goes back to the folder with the saves marked, the card file kept open: START there pastes them)
    {"name": "card-file-held", "what": "a save of a card file marked, circle back to its folder, pasted there as a .psu",
     "cards": CARDS, "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXXXOSXXC"},
    # (circle up out of the MMCE with them marked: the list of cards, where START pastes them into one)
    {"name": "card-file-held-out", "what": "a save of a card file marked, out of the MMCE to the list of cards, pasted into "
     "an empty card", "cards": CARDS, "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXXXOOODSXXC"},
    # (a card's save marked, its copy picked into the MMCE: X opens a card file there, START pastes into it)
    {"name": "card-file-paste-in", "what": "a save of a card copied into a card file of the MMCE, opened with X during the "
     "copy", "cards": CARDS, "files": {"Files/card.mcd": "empty.mcd"}, "script": "XXXS>>XXXSXXC"},
    # (a save of one card file marked, another card file opened: no question; START there pastes it into that one)
    {"name": "card-file-two", "what": "a save of a card file marked, a second card file of the folder opened, the save "
     "pasted into it", "cards": CARDS, "files": {"Files/a.mcd": "game.mcd", "Files/b.mcd": "empty.mcd"},
     "script": ">>XXXXXODXSXXC"},
    {"name": "card-file-export", "what": "a save of a card file opened in Files, copied to a folder of the MMCE as a .psu",
     "cards": CARDS, "files": {"Files/card.mcd": "game.mcd"}, "script": ">>XXXXXSOSXXC"},
    # The Memory Card: a PS2 memory card in the console's other slot, in the Files group (run.py puts
    # fixtures/memcard.ps2.gz in PCSX2's slot 2)
    {"name": "memcard-open", "what": "the Memory Card in the Files group, opened: its saves", "cards": CARDS,
     "memcard": "memcard.ps2", "script": ">>DDDXC"},
    {"name": "memcard-copy-out", "what": "a save of the Memory Card copied to a card", "cards": CARDS,
     "memcard": "memcard.ps2", "script": ">>DDDXXXS<<SXXC"},
    {"name": "memcard-copy-in", "what": "a save of a card copied to the Memory Card, picked in the Files group",
     "cards": CARDS, "memcard": "memcard.ps2", "script": "XXXS>>DDDSXXC"},
    {"name": "memcard-move-out", "what": "a save of the Memory Card moved to a card", "cards": CARDS,
     "memcard": "memcard.ps2", "script": ">>DDDXXDXXXXC"},
    {"name": "memcard-delete", "what": "a save of the Memory Card deleted", "cards": CARDS, "memcard": "memcard.ps2",
     "script": ">>DDDXXDDXXXC"},
    {"name": "memcard-psu-in", "what": "a .psu file imported into the Memory Card", "cards": CARDS,
     "memcard": "memcard.ps2", "files": {"Files/save.psu": "save.psu"}, "script": ">>XXXX>>XXXC"},
    # (its options, "Upload to Drive": read into a card file, sent as a dry sync sends, "B": nothing goes)
    # (the Memory Card put back from a backup: "Y" makes the backup the data folder's restore-test.mcd, the "game"
    # card, two saves of SLUS-21065; they go into the Memory Card, its other saves stay)
    {"name": "memcard-restore", "what": "the Memory Card restored from a backup: the backup's saves copied into it",
     "cards": CARDS, "memcard": "memcard.ps2", "files": {"SD2Cloud/restore-test.mcd": "game.mcd"},
     "script": ">>DDDXTDXYXXC"},
    {"name": "memcard-upload", "what": "the whole Memory Card uploaded to Drive (dry: PCSX2 has no Google account)",
     "cards": CARDS, "memcard": "memcard.ps2", "script": ">>DDDXTXXBwC"},
    {"name": "memcard-paste-back", "what": "a paste on the Memory Card given up at its question: back on the Memory Card, "
     "pasted there", "cards": CARDS, "memcard": "memcard.ps2", "script": "XXXO>>DDDSOSXXC"},
    {"name": "memcard-paste-list", "what": "a save marked on a card, pasted with START on the Memory Card in the list of "
     "cards", "cards": CARDS, "memcard": "memcard.ps2", "script": "XXXO>>DDDSXXC"},
    {"name": "settings", "what": "settings changed (how many backups are kept, their format, the language) and About",
     "cards": CARDS, "script": "SDDDDDDXDXDXDXXDXDXUUUXUXDDDDDDDXOOC"},
    # (a sync with nothing sent, "B": SELECT shows its cards; X on one still to go: resolved; on the next, its sync off)
    {"name": "sync-list", "what": "the list of a sync's cards: one card marked as resolved, another's sync turned off",
     "cards": CARDS, "script": "BLDXDXDXDDXwC"},
    # (square on the next card: skipped; again: taken back; again: skipped)
    {"name": "sync-list-skip", "what": "the list of a sync's cards: square skips a card at once, and takes it back",
     "cards": CARDS, "script": "BLDQQQwC"},
    # (a card marked as resolved as it is, the empty one: the automatic sync leaves it; a sync of every card sends it)
    # (the backup by save on: a sync of every card sends, after each card, its saves one by one; then the Memory
    # Card's, in the other slot. Each save's signature is kept in the state, and a save as it was sent isn't sent again)
    {"name": "save-backup", "what": "the backup by save: each card's saves, and the Memory Card's, sent one by one",
     "cards": CARDS, "memcard": "memcard.ps2", "settings": "[cards]\r\nsaves = yes\r\n", "script": "BwC"},
    # (no sd2psx: standalone.txt makes host: a drive the program was started from, with no device in the slots. Its
    # VMCs are its cards, the Memory Cards of both slots are in the Files group, and nothing is taken as in use)
    {"name": "standalone", "what": "no sd2psx: the drive's VMCs as cards, the Memory Cards of both slots in Files",
     "files": {"SD2Cloud/standalone.txt": b"x", "VMC/generic_0.bin": "game.mcd"}, "memcard": "memcard.ps2",
     "script": ">C"},
    {"name": "standalone-memcard", "what": "no sd2psx: the Memory Card of slot 2 opened from its row in Files",
     "files": {"SD2Cloud/standalone.txt": b"x"}, "memcard": "memcard.ps2", "script": ">DDDXC"},
    {"name": "standalone-sas", "what": "no sd2psx, the program on a drive the IGR can't start it from: the SAS package's screen says so",
     "files": {"SD2Cloud/standalone.txt": b"x"}, "script": "SDDXC"},
    {"name": "sync-resolved", "what": "a card marked as resolved is left out of the automatic sync, until it is sent",
     "cards": CARDS, "files": {"SD2Cloud/state-debug.ini": RESOLVED}, "script": "BwC"},
    {"name": "card-sync-toggle", "what": "a card's sync turned off in its options, and on again", "cards": CARDS,
     "script": "TDXTDXC"},
    # (the device has another folder for that game now, by its Game2Folder.ini: the card of the game's own folder was
    # left behind. Its two saves go to the card of the folder the device opens, which is made for them)
    {"name": "card-move-saves", "what": "the saves of a card the device no longer opens for its game, moved to the one it does",
     "cards": {"Card1/Card1-1": "game", "SLUS-21065/SLUS-21065-1": "game"},
     "files": {".sd2psx/Game2Folder.ini": b"[PS2]\nSLUS-21065=MCCG-10045\nSLUS-21267=MCCG-10045\n"},
     "script": ">DXTDDDDXXXC"},
    {"name": "save-export-list", "what": "a save marked on a card, exported from the Files group of the list of cards",
     "cards": CARDS, "script": "XXXO>>XSXXC"},
    # (a microSD that was in both devices has MemoryCards/PS2 and the MemCard PRO2's own PS2: which device it is in is
    # asked once. Told it is the MemCard PRO2, its card is the one listed; told in the settings it is the sd2psx, the three)
    {"name": "device-both", "what": "a microSD with both devices' cards: which device it is in is asked, and changed in the settings",
     "cards": CARDS, "files": {"PS2/MemoryCard1/MemoryCard1-1.mc2": "game.mcd"}, "script": "DXXSDDDDXUXC"},
    # (on that microSD, with the sd2psx in the slot, R2 lists the MemCard PRO2's cards, and back: a save of the sd2psx's
    # first card copied into the MemCard PRO2's, from the list of cards; and one of the MemCard PRO2's card, marked
    # with its cards on the screen, pasted into the sd2psx's second card after R2 in that list. Nothing of them goes
    # into the state)
    {"name": "device-both-to-other", "what": "a microSD with both devices' cards: a save copied into the other device's card",
     "cards": CARDS, "files": {"PS2/MemoryCard1/MemoryCard1-1.mc2": "empty.mcd"}, "settings": "device = sd2psx\r\n",
     "script": "XXXO2SXXC"},
    {"name": "device-both-from-other", "what": "a microSD with both devices' cards: a save of the other device's card copied into one of the slot's",
     "cards": CARDS, "files": {"PS2/MemoryCard1/MemoryCard1-1.mc2": "game.mcd"}, "settings": "device = sd2psx\r\n",
     "script": "2XXXO2DSXXC"},
    # (the OPL's VMCs of the microSD, VMC/*.bin, are cards of a group of their own, with the games whose OPL settings
    # name them: one is opened, and one is renamed, the settings of its game with it)
    {"name": "vmc-group", "what": "the OPL's VMCs as a group of cards, with the games that use them",
     "cards": CARDS, "files": VMC_FILES, "script": ">>.XC"},
    {"name": "vmc-rename", "what": "a VMC renamed, and the OPL settings of the game that uses it",
     "cards": CARDS, "files": VMC_FILES, "script": ">>DTDDDDXXSXXC"},
    {"name": "vmc-new", "what": "a VMC made in the microSD's VMC folder, of the size recommended, named as offered (generic_0)",
     "cards": CARDS, "files": VMC_FILES, "script": ">>DDXXSXXC"},
    # (two game cards with the same two saves: each is on the screen once, until the options say to show them all;
    # then by name)
    {"name": "all-saves-repeated", "what": "every game card's saves on one screen: the same save on two cards once, then each time, then by name",
     "cards": {"Card1/Card1-1": "empty", "SLUS-21065/SLUS-21065-1": "game", "SLUS-20001/SLUS-20001-1": "game"},
     "script": ">XTDXOTXXOC"},
    # (a save of the same name as one a game card has, saved at another moment, is imported into another game's card:
    # they are two saves, and both are on the screen)
    {"name": "all-saves-progress", "what": "two saves of the same name saved at different moments are both shown",
     "cards": {"Card1/Card1-1": "empty", "SLUS-21065/SLUS-21065-1": "game", "SLUS-20001/SLUS-20001-1": "empty"},
     "files": {"Files/alpha.psu": "alpha-other.psu"}, "script": ">>XXXX>DXXXXOOO<XC"},
    # △ on a card: "Delete card", eighth of its options (on PCSX2 there is no "Insert"), after "Rename", "Change
    # size" and "Split saves by game"; the last card of its folder takes the folder with it. With more than one card in its folder, "Delete
    # whole folder" comes after it
    # "Check card", eighth of a card's options: read through, nothing written
    {"name": "card-check", "what": "a card's file system checked, read only: no problems on a whole card",
     "cards": CARDS, "script": "TDDDDDDDXC"},
    # (a card deleted leaves a safety copy, in SD2Cloud/Safety/: put back from Tools > "Safety copies", it is a card
    # again, in its place)
    {"name": "safety-restore", "what": "a deleted card put back from its safety copy",
     "cards": CARDS, "script": "DTDDDDDDDDXXXLDXXXXXC"},
    {"name": "card-delete", "what": "a card deleted from its options: its file and its folder go", "cards": CARDS,
     "script": "TDDDDDDDDXXXC"},
    {"name": "folder-delete", "what": "a game's folder of two cards deleted whole: triangle on its row, the folder's options",
     "cards": dict(CARDS, **{"SLUS-21065/SLUS-21065-2": "empty"}), "script": ">DTXXXC"},
    # "Change size", sixth of its options: the cursor starts on the size recommended for what it has (1 MB)
    {"name": "card-resize", "what": "a card made again of another size (8 MB to 1 MB): its saves as they were", "cards": CARDS,
     "script": "TDDDDDXXXXC"},
    # "Rename", fifth of a card's options: a numbered card keeps its file, and its name on the sd2psx is typed (X types
    # the key under the cursor, A; START takes it); a folder of the user's own is called something else, asked first
    {"name": "card-rename", "what": "a numbered card's name on the sd2psx typed: its folder's .ini", "cards": CARDS,
     "script": "TDDDDXXSC"},
    {"name": "folder-rename", "what": "a folder of the user's own renamed: its cards and files under the new name",
     "cards": {"Card2/Card2-1": "empty", "Racing/Racing-1": "game"}, "files": {"MemoryCards/PS2/Racing/Racing.ini":
     b"[ChannelName]\n1=Laps\n"}, "script": "DTDDDDXXSXXC"},
    # △ on a file of the Files group: renamed (its extension kept), or deleted
    {"name": "file-rename", "what": "a .psu of a folder of the microSD renamed from its options", "cards": CARDS,
     "files": {"Files/save.psu": "save.psu"}, "script": ">>XXTXXSC"},
    {"name": "file-delete", "what": "a .psu of a folder of the microSD deleted from its options", "cards": CARDS,
     "files": {"Files/save.psu": "save.psu"}, "script": ">>XXTDXXXC"},
    # "New card", the last row of the Cards group (up from the first card): where it goes (a numbered card, a new
    # group typed on the keyboard, or a file of a folder, made with triangle there: its format, size and name), and
    # its size (8 MB, with nothing in it)
    {"name": "card-create", "what": "an empty numbered card made from the main screen", "cards": CARDS,
     "script": "UXXXXXC"},
    {"name": "group-create", "what": "a card made in a new group of the user's own (a folder with a name)", "cards": CARDS,
     "script": "UXDXXSXXXC"},
    {"name": "card-create-file", "what": "an empty card made as an .mcd file of a folder of the microSD", "cards": CARDS,
     "files": {"Files/save.psu": "save.psu"}, "script": "UXDDXXTXXSXXC"},
    {"name": "card-create-ps2", "what": "an empty card made as a PCSX2 .ps2 file (with its ECC) of a folder of the microSD",
     "cards": CARDS, "files": {"Files/save.psu": "save.psu"}, "script": "UXDDXXTDDXXSXXC"},
    # a save marked for a copy goes into a card made for it: "New card" at the end of the Cards group of the list
    {"name": "marked-new-card", "what": "a marked save copied into a new numbered card, of the size recommended for it",
     "cards": CARDS, "script": "XXXOUXXXXXC"},
    # "Export all to a card", the last of the options of "All saves": every save shown into a new card. A folder on
    # two cards, saved differently on each, is asked about: the newest of each
    {"name": "all-saves-export", "what": "every save of the game cards exported to a new card",
     "cards": {"Card1/Card1-1": "empty", "SLUS-21065/SLUS-21065-1": "game", "SLUS-20001/SLUS-20001-1": "game"},
     "script": ">XTDDXXXXXC"},
    {"name": "all-saves-export-repeated", "what": "every save exported to a new card, the newest of a folder saved twice",
     "cards": {"Card1/Card1-1": "empty", "SLUS-21065/SLUS-21065-1": "game", "SLUS-20001/SLUS-20001-1": "empty"},
     "files": {"Files/alpha.psu": "alpha-other.psu"}, "script": ">>XXXX>DXXXXOOO<XTDDXXXXXXC"},
    # marking for a copy a save whose folder is marked on another card: asked first (with each one's card and date);
    # X marks this one in the other's place
    {"name": "mark-same-folder", "what": "a save of a folder marked on another card, marked in its place once asked",
     "cards": {"Card1/Card1-1": "empty", "SLUS-21065/SLUS-21065-1": "game", "SLUS-20001/SLUS-20001-1": "empty"},
     "files": {"Files/alpha.psu": "alpha-other.psu"}, "script": ">>XXXX>DXXXXOOO<XXX>XXC"},
    # "Split saves by game", seventh of a card's options: each game's saves to its card. The game has none: a new one, of
    # the size recommended (1 MB), and the card they come from stays as it was (circle on the question of deleting).
    # The game's card has them already, the same: nothing is copied, and they are deleted from where they were
    {"name": "split-new-card", "what": "a card's saves split by game: a new card for their game, the card left as it was",
     "cards": {"Card1/Card1-1": "game", "Card2/Card2-1": "empty"}, "script": "TDDDDDDXSXXOC"},
    {"name": "split-there-delete", "what": "a card's saves split by game, on the game's card already: deleted from it after",
     "cards": CARDS, "script": "TDDDDDDXSXXXXC"},
    # Settings (START), the fifth row: the size the sd2psx makes a game's first card of, written to its settings.ini
    {"name": "settings-card-size", "what": "the size of the sd2psx's new cards set to 32 MB in its settings.ini",
     "cards": CARDS, "files": {".sd2psx/settings.ini": SD2PSX_SETTINGS}, "script": "SDDDDXDDXXC"},
    {"name": "saves-by-name", "what": "a card's saves the oldest first, by name, and the newest first again: each time the options are closed", "cards": CARDS, "script": "XTDXOTDXOTDXOC"},
    {"name": "igr-no-account", "what": "started as after IGR with no Google account: it leaves at once", "cards": CARDS,
     "script": "I"},
    # the tools (SELECT) and the templates. The keyboard starts on A: DD>> is W. Triangle has a template's options, on
    # the list (LXDT) or with the template open (LXDXT)
    {"name": "template-create", "what": "a template made of two saves marked on a card, under the name it is offered; "
     "the first there is, it is made the default one", "cards": CARDS, "script": "LXXSXX>XSXXXC"},
    {"name": "template-apply", "what": "a template put into a card that isn't a game's, marked in the list of cards", "cards": CARDS, "files": TEMPLATE,
     "script": "LXDTDDXDXDDXSXXC"},
    {"name": "template-edit", "what": "a template renamed (a letter erased, another typed), given a save and rid of one",
     "cards": CARDS, "files": TEMPLATE, "script": "LXDXTDDDDXQDD>>XSTDXXXSXXXXXC"},
    {"name": "template-delete", "what": "a template deleted", "cards": CARDS, "files": TEMPLATE, "script": "LXDXTDDDDDXXC"},
    {"name": "template-main", "what": "a template made the main one, and put into the game card that lacks it",
     "cards": CARDS, "files": TEMPLATE, "script": "LXDXTXXXXC"},
    {"name": "template-main-list", "what": "a template made the default one with square on the list of templates",
     "cards": CARDS, "files": TEMPLATE, "script": "LXDQXXXC"},
    {"name": "template-warning", "what": "the program opens with a main template a game card lacks: said, and put into it",
     "cards": CARDS, "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n"}),
     "script": "XXC"},
    {"name": "template-no-warning", "what": "the same, and the user asks not to be told about that card",
     "cards": CARDS, "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n"}),
     "script": "QC"},
    {"name": "template-warn-again", "what": "told not to warn about that card, then \"Re-enable warnings\" in the template's options: "
     "said again at once, and put into it", "cards": CARDS,
     "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n"}), "script": "QLXDTDDDXXXC"},
    {"name": "template-games", "what": "a template made a game's, and put into that game's card",
     "cards": CARDS, "files": TEMPLATE, "script": "LXDXTDDDXXOXXC"},
    # (the template's save is another version of one the game card has; Card1-1 has it too, and is no game card)
    {"name": "template-update", "what": "a save of a template that a game card has in another version, updated there",
     "cards": CARDS, "files": {"SD2Cloud/templates/Net/alpha.psu": "alpha-other.psu"}, "script": "LXDTDDXDDXXXC"},
    {"name": "template-pick-cards", "what": "a template put into the game cards marked one by one",
     "cards": dict(CARDS, **{"SLUS-20001/SLUS-20001-1": "empty"}), "files": TEMPLATE, "script": "LXDTDDXDXXSXXC"},
    {"name": "template-auto-apply-off", "what": "applying the templates after a game turned off",
     "cards": CARDS, "files": TEMPLATE, "script": "LXDDXC"},
    # after a game the device is on the BootCard. Both cards have nothing in them, as the BootCard: the second one
    # can't be told from the card in the device
    {"name": "template-auto-apply", "what": "after a game, a game card about to be sent gets its templates; one that may be "
     "the card in the device is left for the warning",
     "cards": {"BOOT/BootCard-1": "empty", "Card1/Card1-1": "game", "SLUS-21065/SLUS-21065-1": "game",
               "SLUS-20001/SLUS-20001-1": "empty"},
     "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n",
                                "SD2Cloud/active.txt": b"BOOT/BootCard-1"}),
     "script": "tOC"},
    # the same after a game with no sync to do (PCSX2 has no Google account): the templates are still put in
    {"name": "template-auto-apply-no-sync", "what": "after a game with no sync to do, a game card still gets its templates; one "
     "that may be the card in the device is left for the warning",
     "cards": {"BOOT/BootCard-1": "empty", "Card1/Card1-1": "game", "SLUS-21065/SLUS-21065-1": "game",
               "SLUS-20001/SLUS-20001-1": "empty"},
     "files": dict(TEMPLATE, **{"SD2Cloud/templates/templates.ini": b"[templates]\nmain = Net\n",
                                "SD2Cloud/active.txt": b"BOOT/BootCard-1"}),
     "script": "I"},
]
