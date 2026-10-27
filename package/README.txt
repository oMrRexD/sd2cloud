SD2Cloud 2.0
============

Manage and back up your PS2 memory cards, right on the console.

Before you use it: SD2Cloud writes to your memory cards whenever it copies, moves, deletes, imports or restores
saves, and whenever it creates, resizes or deletes cards. Make a backup first: sync your cards to Google Drive, or
copy the MemoryCards folder from the microSD to a PC.

SD2Cloud is an app that runs on the PS2 itself, with no PC needed. It backs up your memory cards to Google Drive
and lets you manage your saves right on the console. It works with:
- sd2psx-family devices (sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero) running the sd2psXtd firmware.
  On the PS2, these devices (and their microSD) are known as MMCE, the name this text uses for them.
- MemCard PRO2.
- Regular Memory Cards: the physical cards in the console's slots.
- OPL's virtual memory cards (VMCs) and other memory card files kept on a USB drive, an MX4SIO or the internal
  HDD.

WHAT IT DOES
Backup to Google Drive
- Syncs your memory cards with your Google Drive whenever you want, or automatically every time you quit a game
  with IGR (In-Game Reset, OPL's button combination for leaving a game). Only the cards that changed are uploaded.
- Keeps a history of backups for each card, and you can restore any of them. The card is checked before and
  after it is written.
- Backup by save (optional): also uploads each save that changed as a file of its own, with its older versions.
- On Drive, the backups are sorted into folders by device, game and card. You can browse them from the Files tab.
Memory card manager
- Shows each card's saves the way the PS2 browser does, with the animated 3D icons.
- Copies, moves and deletes saves between cards, several at once. It can also upload a single save to Google
  Drive as a .psu file (the usual file format for one PS2 save).
- The Library keeps saves outside the cards, sorted by game, with no size limit.
- Creates, resizes, renames, checks and deletes cards, and converts a card to Game ID (each game's saves go to
  that game's own card). Before any change that could lose data, it keeps a safety copy of the card.
- Sorts the cards into tabs: numbered cards and your own folders, game cards (Game ID) in folders named after
  each game, boot cards, OPL's VMCs and the Library.
- Imports .psu files (and saves in folder format) into any card, exports any save as a .psu, and opens memory
  card files and installs them as cards (see FILES).
- Manages the regular Memory Cards in the console's slots: their saves, and the whole card uploaded to Drive and
  restored from there.
- Keeps templates: sets of saves to put into cards (see TEMPLATES).
- Starts other programs: an ELF in a device's folders, or an app kept on a memory card in the Save Application
  System format ("Start application" on the app's page).

REQUIREMENTS
- One of these:
  - an sd2psx-family device with the sd2psXtd firmware (MMCE support);
  - a MemCard PRO2 (its cards are the .mc2 files in the PS2 folder of its microSD). On the PRO2, "Insert this
    card" is still a preview feature, and SD2Cloud never switches cards on its own: to change the card that is
    in use, switch to another one first;
  - neither: SD2Cloud on a USB drive, an MX4SIO or the internal HDD, with regular Memory Cards in the slots (see
    WITHOUT AN SD2PSX).
  A microSD that was used in both devices has the cards of both. SD2Cloud asks once which device it is in
  (Settings > Cards > "Device" changes the answer), and L2 and R2 switch between the two devices' cards.
- A way to open SD2Cloud: OPL's Apps tab, wLaunchELF or any other program that runs ELF files. Use an OPL with
  MMCE support if you can, such as RiptOPL (https://github.com/NathanNeurotic/Open-PS2-Loader). An OPL without it
  does not list the apps on the MMCE, and has no "IGR Bootcard Slot(s)" option. So, with Game ID cards, quitting a
  game with IGR leaves the sd2psx on the game's card, the PS2 goes back to its browser instead of opening
  SD2Cloud, and the automatic sync does not happen.
- For the automatic sync after a game: OPL, which opens SD2Cloud through IGR.
- For the cloud features: a Google account, and a PS2 with a network adapter (built into Slim models) connected
  to your router by cable.

INSTALLATION
1. Extract this .zip file to the root of the MMCE. It adds:
   - APPS/SD2Cloud: the application;
   - ART: the cover and the logo OPL shows for SD2Cloud in its Apps tab;
   - Extras: the same art with its line in Portuguese (to copy over the two files in ART), the SAS package (see
     below) and sd2cloud.example.ini, which explains every setting.
   The first time it runs, SD2Cloud creates its settings file, SD2Cloud/sd2cloud.ini. This .zip doesn't have that
   file, so extracting a new version over an old one keeps your settings. To set them on a PC before the first
   run, copy Extras/sd2cloud.example.ini into a folder named SD2Cloud at the root of the MMCE, rename it to
   sd2cloud.ini and edit it.
2. Open SD2Cloud from OPL's Apps tab, or run APPS/SD2Cloud/SD2CLOUD.ELF from any program that runs ELF files
   (wLaunchELF, for example). The folder doesn't have to stay in APPS.
3. If no Google account is connected, SD2Cloud asks whether to connect one now:
   - "Not now" goes straight to the main screen;
   - "Don't ask again" (TRIANGLE) does the same and stops asking for good (ask_connect = no in sd2cloud.ini).
   You can connect the account later in Settings > Google Drive. To connect, go to google.com/device on your
   phone or computer (or scan the QR code) and enter the code shown on the TV. SD2Cloud can only see the files it
   creates in your Drive.
4. Right after the account is connected, SD2Cloud asks whether to turn on automatic sync (see AUTOMATIC SYNC). If
   you say yes, it installs the SAS package and shows the path to set in OPL. If you say no, you can turn it on
   later in Settings > Automatic sync.
5. SD2Cloud then offers to sync all your memory cards. After that, only the cards that changed are uploaded, and
   each sync makes a new backup of the card on Drive.
   Even if you don't sync, SD2Cloud notes the state of each card the first time it sees it, so the automatic
   sync after IGR only uploads the cards that change from then on.

The app on another device, with an sd2psx
SD2Cloud can also live on a USB drive, an MX4SIO or the internal HDD (exFAT or APA). Put the APPS/SD2Cloud folder
there and open SD2Cloud from there once. It then records where it is, keeps on the MMCE (in SD2Cloud/drivers) the
drivers the SAS package needs to start it from that device, and updates itself in that place. Its settings and
data stay on the MMCE.

The SAS package, on the memory card
When you turn on automatic sync, SD2Cloud puts its SAS (Save Application System) package on the memory card in
use. It is the APP_SD2CLOUD folder, about 240 KB, with:
- the program OPL runs on IGR;
- a shortcut that opens SD2Cloud wherever it is, with its own 3D icon in the PS2 browser;
- without an sd2psx, also the drivers of the device SD2Cloud is on (about 90 KB more).
SD2Cloud itself stays where it is. This .zip also has the package as Extras/APP_SD2CLOUD.psu, to import into
another card by hand (SD2Cloud's own Files tab does it). After that, open SD2Cloud once from where it is, and the
shortcut finds it.

WITHOUT AN SD2PSX
Extract the .zip to a USB drive (FAT32 or exFAT), an MX4SIO card or the internal HDD, and open
APPS/SD2Cloud/SD2CLOUD.ELF from there. SD2Cloud keeps its settings and data on that device.
- Its cards are OPL's VMCs on that device (the VMC folder).
- Besides the devices, the Files tab has the regular Memory Cards of both slots ("Memory Card 1" and "Memory
  Card 2"). Their saves are copied, moved, deleted, uploaded to Drive and restored like any card's.
- What SD2Cloud backs up are the Memory Cards, save by save (Backup by save is on by default without an sd2psx),
  and the first sync already offers them.
- Folders of an sd2psx or a MemCard PRO2 on that device are just files to SD2Cloud: open them in the Files tab.
- For the automatic sync, install the SAS package (Settings > Automatic sync > "SAS package"). It goes to the
  Memory Card, with the drivers of the device SD2Cloud is on, and OPL's IGR opens SD2Cloud through it.
You can take the sd2psx out and put it back while SD2Cloud is open: the screens follow.

MAIN SCREEN
The memory cards are listed on the left, each with a status light: green means synced; yellow means not synced
yet, or never synced. The card under the cursor is shown on the right, with the date of its last backup.
- Up and Down choose a card. The left stick moves the cursor too, on every screen.
- Left and Right, or L1 and R1, change tabs.
- X opens the card: its saves, shown as in the PS2 browser, newest first.
- TRIANGLE opens the card's options (see below).
- START opens the settings (see SETTINGS), and SELECT the tools (see TEMPLATES and SAFETY COPIES).
- O exits the app. You choose where to go next (the programs in the APPS folder, "Run an ELF..." or the PS2
  browser), and the last choice is remembered.
On the screens where X marks and unmarks (the saves of a copy, a template's saves, games and cards, "Export saves
to a card", "Convert to Game ID"), you can hold X down and move the cursor: everything it passes over is marked
or unmarked the same way. A quick press of X while the cursor happens to move doesn't count.

Tabs
The cards are grouped into tabs, shown above the list. Tabs without cards are hidden.
- Cards: the numbered cards (Card1, Card2...), folders with names of your own, and "New card" as the last row.
- Games: the Game ID cards, in folders named after each game. X opens a folder, O goes back.
- Boot: the BootCard.
- OPL VMCs: see OPL VMCS.
- Library: see THE LIBRARY. Shown once it has saves.
- Files: see FILES.
On a microSD with the cards of both devices, the two are shown at the top right: L2 lists the cards of the one on
the left, R2 those of the one on the right, so you can copy saves between them.
A folder that the sd2psx's .sd2psx/Game2Folder.ini gives to several games (a series that shares one card) is
shown under those games' names: what they have in common ("Need for Speed"), or the names themselves.
A game card that this file left behind (the sd2psx now opens another folder for that game) has a grey arrow, and
its options offer to move its saves to the card the sd2psx opens.

Card options (TRIANGLE)
- Sync now.
- Disable sync: the card is left out of every sync, the automatic one too, until you pick "Enable sync" there.
  Its status is then "Excluded".
- Restore a backup: see RESTORING.
- Copy to a device: the whole card goes, as a file, to a folder on a device. Pick the device, go to the folder
  and press TRIANGLE. SD2Cloud asks whether the card goes as a .mcd (sd2psx) or a .ps2 (PCSX2), and writes it
  inside a .zip, like the backups on Drive. Then it reads the .zip back and compares it.
- Insert into sd2psx: makes the sd2psx switch to that card, as its own buttons would. The sd2psx takes a BootCard
  only with Autoboot turned on in its settings, and a game's card only with Game ID. A folder with a name of its
  own can only be picked on the sd2psx itself. On the list, a small card icon marks the card in use.
- Rename, Change size, Convert to Game ID, Check card and Delete card: see CARDS.
On a folder of the Games tab, TRIANGLE has Rename and Delete folder.

All saves
The Games tab starts with "All saves": the saves of every game card on one screen, newest first, as if they were
on a single card. Each save still belongs to its own card, whose name is at the top of the save's page, and
whatever you do to a save is done on that card. A save that more than one card has (the same folder with the same
files, byte for byte, like a template put into every card) is shown only once.

Inside a card
- X on a save opens its page, with Copy, Move, Delete, Upload to Drive and Versions on Drive (see BACKUP BY
  SAVE).
- TRIANGLE opens the options:
  - "Sync now", for that card;
  - "Sort by": by date (newest or oldest first) or by the name of the game, for every card;
  - "Convert to Game ID";
  - in "All saves": "Duplicates", hidden or shown, and "Export saves to a card";
  - on the Memory Card: "Upload to Drive" and "Restore a backup" (see FILES).
- "Export saves to a card" shows all the saves selected. X leaves one out; SQUARE unselects them all (with none
  selected, it selects them all again); START goes on to choose where the new card goes.

Copying and moving
- Copy marks the save (a green tick over its icon) and takes you back to the card's saves, where X now marks and
  unmarks others. O leaves the card for the list of cards, so you can mark saves on other cards too. The marks
  stay as you go from card to card, and their count is on screen. TRIANGLE (Cancel) drops the selection, asking
  first when more than one save is marked.
- START (Paste) pastes them all into the card where you press it: the one that is open, or the one under the
  cursor in the list of cards. A card without enough space for them is left as it is. Pressed where they can't go
  (the card they all came from, or "All saves"), START takes you to the list of cards.
- In the list's Games tab, when the saves are all from one game, that game's folder comes first, in green, with
  "New card" in it. If the game has no folder yet, "New card" comes first among the folders.
- The Library tab of the list keeps the saves outside the cards (see THE LIBRARY). The Files tab of the list
  leads to the folders of a device (see FILES) and to the Memory Card.
- Move takes the save to another card, chosen the same way.
- Upload to Drive sends only that save, as a .psu, to the card's Saves folder on Drive.
- To change the card the sd2psx is using, SD2Cloud asks first, then switches the sd2psx to another card, makes
  the change and switches back. Only for a folder with a name of its own do you have to switch the card on the
  sd2psx yourself first.

Confirmations and syncing
Everything that uploads, writes or deletes data asks for confirmation first.
During a sync:
- O cancels the upload. Cards already uploaded stay on Drive.
- While several cards are being sent, SQUARE asks about the one on its way: SQUARE again skips it this time;
  TRIANGLE skips it and turns its sync off.
- SELECT shows the list of the cards in this sync and how each one went, and SELECT again goes back. On that
  list, SQUARE skips a card at once (press again to undo), and X offers to skip it, mark it as resolved (the
  automatic sync leaves it alone until it changes again) or turn its sync off.

CARDS
- New card (the last row of the Cards tab, and of any list where saves are pasted) can make:
  - a numbered card;
  - a card in a folder of your own (an existing folder or a new one);
  - in the Files tab, a card file on a device (.mcd, .mc2, .ps2 or OPL's .bin): pick the folder and press
    TRIANGLE, "Create here".
  Then you pick its size: 1 to 64 MB on an sd2psx, 8 to 128 MB on a MemCard PRO2. The size recommended for the
  saves going in comes selected.
  In the Games tab, a game's "New card" adds a channel to that game's folder. Settings > Cards > "New card size"
  sets the size the sd2psx itself uses for a game's first card (the sd2psx reads it when it starts).
- Rename: a numbered card, a game's card or a BootCard keeps its file name, because the sd2psx finds it by that
  name. What changes is the name the sd2psx shows for it, in its folder's .ini. A folder of your own is renamed
  along with its cards and files. Backups already on Drive keep the old name.
- Change size: the saves are copied to a new card of the size you pick, which replaces the current one after it
  is verified. If anything fails, the current card stays as it was.
- Convert to Game ID: splits a card into one card per game (Game ID cards are the cards the sd2psx switches to by
  itself when a game starts). It also works on the Memory Card and on card files.
  - The card's saves are listed by game, each with the card it goes to: the game's card, or "New card" with its
    size (TRIANGLE changes it).
  - X leaves a game out. SQUARE unselects them all, so you can pick just a few (with none selected, it selects
    them all again).
  - START copies them all, on one screen with the total time left. O cancels it, after asking: the saves already
    copied stay on their games' cards.
  - If a game's card already has a save in another version, SD2Cloud asks once: keep the card's, replace it, or
    keep the newest.
  - New cards get the game's templates.
  - At the end, R2 removes the copied saves from the source card, after asking. If you don't, it stays as it was.
  - Only with an sd2psx or a MemCard PRO2, since game cards belong to them.
- Check card: looks through the card's file system (its chains of clusters, its entries) and tells what is wrong,
  if anything. Nothing is written.
- Delete card: asks first. If the device is using that card, it moves to another one first. When it was the last
  card in its folder, the folder is deleted too.
- Delete folder (TRIANGLE on a folder of the Games tab): deletes all of its cards, after asking.

SAFETY COPIES
Before a card is deleted or resized, before saves are moved out of it, and before a conversion to Game ID removes
the copied saves, SD2Cloud keeps a copy of the whole card in SD2Cloud/Safety on the MMCE. The 10 newest copies are
kept. Tools (SELECT) > Safety copies lists them: X restores one (over the card, or as the card again if it was
deleted) or deletes it.

OPL VMCS
OPL's virtual memory cards (VMC/*.bin on the MMCE) are the cards of the OPL VMCs tab. Each one shows the games
that use it ("Linked games", taken from OPL's settings for each game, in the CFG folder).
- They are opened, synced, restored, resized and deleted like any card.
- "New VMC", the last row, makes one: 8 to 64 MB, named generic_0, generic_1... unless you type another name.
- Rename renames the file and updates OPL's settings for the games that use it.
- Settings > Cards > "OPL VMC folders" adds the VMC folders of the internal HDD (exFAT or APA), an MX4SIO or a USB
  drive. Each place becomes a folder in the tab.

THE LIBRARY
The Library keeps saves outside the cards: useful when a Memory Card is full, or for a save you want off a card
without deleting it.
- To put a save there, Move or Copy it (or paste marked saves, or import a .psu) and pick the Library tab. Its row
  shows the game the save goes under ("Several games" for marked saves of more than one game).
- Each save is kept as a file of its own, sorted by game, with no size limit, in SD2Cloud/Library on the MMCE.
- The Library tab of the main screen appears once it has saves and lists its games. X opens one, and its saves
  can be moved, copied or deleted like any other, back to any card.
- Settings > Google Drive > "Library on Drive" also sends them to Google Drive, each one by itself, into the
  Library's folder there.

FILES
Files, the last tab of the main screen, lists the devices whose folders you can browse:
- the MMCE;
- a USB drive (FAT32 or exFAT);
- the internal HDD (exFAT, FAT32 or APA). On a disk with both, as the PSBBN installer leaves it, SD2Cloud asks
  which side to open: "exFAT partition" or "APA partitions";
- an MX4SIO (listed when slot 2 has no PS2 memory card in it, because its driver would take over the slot).
A device's drivers are loaded only when it is opened. X opens a device, a folder, a .psu file or a memory card
file; O goes back one folder; Left and Right move a page at a time. Every folder and file is listed, but only
those files can be opened: the buttons at the bottom of the screen are the ones that do something with the
selected entry.

Importing
- X on a .psu file opens the page of the save it holds (name, icon, date and size). "Import to card" then asks
  for the card, chosen from the same tabs as the main screen. SQUARE on the file, in the list, goes straight to
  the card, without opening the page.
- The file is checked as a whole before the card is touched, and what is written is read back and compared.
- A card that already has a save with the same name, or doesn't have enough space, is left as it is: delete or
  move that save first.
- Saves in folder format are installed like a .psu. Select the folder, or go into it, and press SQUARE ("Install
  save"): the save's page opens, and "Import to card" puts it on a card. X still opens the folder as usual. Files
  a computer adds to a folder (Thumbs.db, desktop.ini, .DS_Store) are ignored.

Exporting
- On a save's page, Copy also lists the Files tab among the destinations. Pick the device, go to the folder where
  the files should go and press START.
- Each file is named after its save's folder (BASLUS-21065SAVE.psu, for example), then read back and compared.
- If a file with that name is already there, SD2Cloud asks: X replaces it, SQUARE keeps both (the new one gets a
  number: BASLUS-21065SAVE (2).psu).

Memory card files
- X on a .mcd, a .mc2 (MemCard PRO2), a .ps2 (PCSX2), a .bin (OPL's VMC) or a .zip made by "Copy to a device"
  opens the card inside it, shown like the cards of the MMCE. X on a save opens its page, from where it can be
  copied anywhere, alone or with other saves of that file.
- A .mcd, .mc2 or .bin card outside a .zip can also receive saves: during a copy, X on the file opens it and
  START there pastes them into it, as does START on the file in the list. A .ps2 (its pages carry ECC bytes) and a
  card inside a .zip can only be read.

Installing a memory card file
- SQUARE, on the file in the list or on the screen of its card, asks where the card goes, in the same tabs as the
  main screen.
- "New card" makes a new card: the lowest free number in Cards, the next channel of the BootCard in Boot, and in
  Games, the next channel of the game's folder (created if the game has none). The game is recognized by the
  file's name (SLUS-21065-1.mcd, or SLUS_210.65_0.bin as OPL names them) or by the saves inside.
- X on an existing card replaces it with the file's card, after you confirm.
- The file is read in full before anything is written, and the card is read back afterwards.
- The Files tab of that same screen writes the card as a file on another device: pick the device, go to the
  folder, press TRIANGLE ("Save here") and choose the format and the file's name. The formats are the sd2psx's
  .mcd, the MemCard PRO2's .mc2, PCSX2's .ps2 (with its ECC), OPL's .bin, or the .zip as it is, when the card
  came in one.

The Memory Card
A regular PS2 memory card in the console's other slot (both slots, without an sd2psx) is a row of the Files tab.
- X shows its saves, which are copied, moved, deleted and uploaded to Drive like a card's. Saves can go into it
  from the cards or from .psu files.
- TRIANGLE, on its saves, has:
  - "Upload to Drive": the whole card is read into a card file and sent as a backup of its own, with its
    history;
  - "Restore a backup": the backup's saves are copied into it. Saves in the same folder are replaced; the others
    stay.

Google Drive
With a Google account connected, Google Drive is the last row. It opens the main folder of the backups on Drive,
browsed like a device's folders (SD2Cloud only sees what it made there).
- X on a save's .psu opens its page, with "Import to card", "Download" and "Delete from Drive".
- X on a card's backup (.zip) opens the card inside it, as a memory card file opens: you can copy its saves out
  from their pages, and SQUARE installs the card, on the microSD or as a file on a device.
- SQUARE, on a card's backup in the list, goes straight to installing it. On any other file, SQUARE downloads it
  as it is into a folder of a device (pick the device, go to the folder and press TRIANGLE, "Save here").
- TRIANGLE deletes the file. A deleted file goes to Drive's trash, where it can be restored for 30 days.

Changing the card the sd2psx is using, by importing into it or replacing it, works like any other change to it:
SD2Cloud asks, switches the sd2psx to another card and back.
Names with accented letters may be shown shortened, or may not open at all (a limit of the PS2 SDK's FAT
driver): prefer plain names for the folders you use here.

TEMPLATES (SELECT)
A template is a set of saves you pick, kept on the MMCE, to be put into cards. The sd2psx makes a new game's card
empty: a template gives it what every card of yours should have, such as the network settings an online game
asks for. SELECT, on the main screen, opens the tools; Templates is the first.
- New template: type its name on the on-screen keyboard (X types, SQUARE erases, START finishes). The cards are
  then shown as on the main screen: X opens one, and inside it X marks and unmarks a save. The marks stay as you
  go from card to card, and their count is at the top. START ends the marking and copies the saves into the
  template; TRIANGLE gives up.
- X on a template opens it, shown like a card. X on a save removes it from the template.
- TRIANGLE, on a template in the list or with it open, has the rest: Set as default, Add saves, Apply template,
  Template games, Rename and Delete template. Add saves marks them the same way; a save the template already has
  can be replaced by the one you mark.
- Apply template puts into cards the saves they don't have yet. A save a card already has is never touched, and
  a card without enough space is left as it is.
  - All game cards: every game card missing saves of the template gets them, one after the other, and SD2Cloud
    says how many did.
  - Select cards: the same, for the cards you mark, game cards or not (X marks and unmarks, SQUARE marks or
    unmarks all, START applies).
  - Update existing saves is the one that replaces: on the game cards, the template's saves that a card has in
    another version are replaced by the template's. Nothing else on the card changes, missing saves are not
    added, and it asks first.
  Putting a template's saves into a card doesn't count as a change for the sync: a card that was synced stays
  synced.
- Set as default: the default template is the one every game card should have (a yellow light marks it in the
  list). When SD2Cloud opens and a game card doesn't have it, like a new game's card, SD2Cloud says which cards
  and waits for an answer: X applies it, O leaves it for later, SQUARE stops the warning for those cards. Each
  card is asked about once, and again only when its templates get other saves.
- Template games: a template that isn't the default can be tied to some games. X marks and unmarks each game (a
  green tick), SQUARE marks or unmarks all. The cards of those games are then treated the way the default
  template treats every game card.
- Auto-apply (the last row of the list, On or Off): with automatic sync, when you quit a game with IGR, the game
  cards about to be sent get the saves they are missing from their templates, without asking, before they are
  sent. A card is only changed then when it is surely not the one in the sd2psx (by then OPL has switched back to
  the BootCard); any other card is left for the warning the next time SD2Cloud opens.
- Applied by hand to the card the sd2psx is using, a template goes in through the memory card slot, the same way
  a game would write it.
A template is a folder in SD2Cloud/templates on the MMCE, with a .psu file for each save: a .psu put there with a
PC becomes part of the template too.

SETTINGS (START)
The first page has Sync all cards, Language and About, and four groups, each on a page of its own. X opens a
page, O goes back, and START leaves the settings from any page.
- Sync all cards: uploads the cards that are not synced. If all of them are, SD2Cloud says so and lets you sync
  them all again.
- Google Drive:
  - Google account: disconnects the account (the backups on Drive are kept) or connects it again.
  - Backups kept per card (3, 5, 10, 20, 50 or no limit): how many backups of each card stay on Drive. Past that,
    the oldest is removed.
  - Backup format: .mcd (the sd2psx's own) or .ps2 (the format PCSX2 uses: take the file out of the backup's zip
    and put it in PCSX2's memory card folder). Either one can be restored, whatever is set.
  - Backup by save: see BACKUP BY SAVE.
  - Library on Drive: see THE LIBRARY.
  - Drive organization: shown while the backups on Drive still use the old organization (see GOOGLE DRIVE).
- Automatic sync:
  - Automatic sync: On or Off. When Off, quitting a game with IGR goes straight to the program set in "IGR path",
    without showing SD2Cloud. It also stays off while no Google account is connected.
  - SAS package: installs SD2Cloud's folder on the memory card in use (what OPL runs on IGR, and a shortcut for
    the PS2 browser; see AUTOMATIC SYNC). Once it is installed, the same option reinstalls it (or updates it) or
    uninstalls it.
  - IGR path: the program started after the IGR sync. The choices are Automatic (the OPL found on the MMCE), the
    programs in the APPS folder and the PS2 browser.
- Cards: Device (on a microSD with the cards of both devices), New card size (see CARDS) and OPL VMC folders (see
  OPL VMCS).
- Updates: Check for updates (looks for a new version right away and, if there is one, offers to update) and
  Update channel: Stable (the released versions) or Beta (see UPDATING).
- Language.
- About SD2Cloud: version, credits and licenses.
Changes are saved to sd2cloud.ini, which you can also edit on a PC.

BACKUP BY SAVE
Settings > Google Drive > "Backup by save" is Off by default with an sd2psx, and On without one (then the Memory
Cards are what there is to sync).
- After each card is sent, each of its saves that changed is also sent by itself, as a .psu, to the card's Saves
  folder on Drive. Each save keeps as many versions as the card keeps backups.
- The sync also covers the Memory Card in the other slot (without an sd2psx, the ones in both slots), save by
  save.
- "Versions on Drive", on a save's page, lists the versions of that save. X on one puts it back in place of the
  save (the current save is kept until the version is fully written).

GOOGLE DRIVE
The backups are kept in SD2Cloud, the main folder on your Drive. Inside it:
- a folder for the device (sd2psx or MemCard PRO2), with a folder for each group of cards (a game's group is
  named after the game, such as "Need for Speed - Underground 2 [SLUS-21065]");
- in each group, a folder for each card, with its backups (.zip) and its Saves folder;
- next to them, OPL's VMCs, the Memory Card in a slot and the Library, each in a folder of its own.
The folders are named in the language of the screens when they are created. SD2Cloud finds them by a tag of its
own, so you can rename or move them on Drive.
If your Drive has backups from a version before 2.0 (all groups of cards side by side in "PS2 Memory Card
Backups"), SD2Cloud offers the new organization once, at startup:
- "Organize" moves every file into it, without uploading anything again (with many backups, it can take a few
  minutes). The old folders, now empty, go to Drive's trash, and the main folder becomes SD2Cloud. If the move
  stops halfway, it carries on the next time SD2Cloud opens.
- "Not now" keeps the old organization. Settings > Google Drive > "Drive organization" offers it again.

AUTOMATIC SYNC AFTER A GAME (IGR)
1. In SD2Cloud, open Settings (START) > Automatic sync and select "SAS package". SD2Cloud shows how much space it
   takes on the memory card in use (about 240 KB) and how much is free, and asks before installing. It writes the
   APP_SD2CLOUD folder (mc0:/APP_SD2CLOUD). If your sd2psx uses Autoboot, install it while the BootCard is in use.
2. In OPL's settings, set "IGR Path" to: mc0:/APP_SD2CLOUD/IGR.ELF
   (mc1: if the sd2psx is in slot 2; SD2Cloud shows the right path when it installs the package)
   If version 1.5 or earlier left its helper on the card (mc0:/BOOT/SD2CLOUD-IGR.ELF), that path still works:
   installing the package also updates that file.
3. In OPL, also turn on "IGR Bootcard Slot(s)" (MMCE page), set to the sd2psx's slot or to BOTH. When you quit a
   game, OPL then switches back to the BootCard, where the package is.
From then on, quitting a game with IGR opens SD2Cloud: it uploads the cards that changed, shows "Upload complete"
and goes back to OPL. In this mode, O cancels from the very first screen, also while the cards are checked and
while it connects, and cancelling also goes back to OPL.
SD2Cloud can be in any folder, on the MMCE or on another device. Each time it opens, it records where it is (in
sd2cloud.ini on the MMCE, "app_path"; without an sd2psx, in the package itself, along with that device's
drivers), and the package starts it from there. The package even finds a folder of APPS on the MMCE that was
renamed or moved, before SD2Cloud is opened from it.
To uninstall the package, select "SAS package" again and choose "Uninstall"; then change "IGR Path" in OPL.
To pause the sync without uninstalling anything, set "Automatic sync" to Off in Settings. IGR then skips SD2Cloud
and goes straight to the program that would open after it. The same happens while no Google account is
connected.

Without installing anything (USB drive)
OPL's IGR can also start a program from a USB drive. If the APPS/SD2Cloud folder is on a USB drive formatted as
FAT32, skip step 1 and set "IGR Path" to:
mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF
Nothing is written to the memory card. For this, OPL loads the USB drivers USBD.IRX and USBHDFSD.IRX from
mc?:/SYS-CONF, where FMCB installs them. With the SAS package on the memory card you need none of this, wherever
SD2Cloud is: the package starts it from the MMCE, a USB drive, an MX4SIO or the HDD.

The program opened after IGR
It is chosen in Settings > Automatic sync > "IGR path", among the programs in the APPS folder of the MMCE. If your
OPL is on another device, put its path in sd2cloud.ini, in the [igr] section, starting with the device: mc?:/
(memory card), mass:/ (USB), mx4sio:/, ata:/ (internal HDD, exFAT) or hdd0:PARTITION:pfs:/ (internal HDD, APA).
For example: hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF. When going back, SD2Cloud loads only the drivers of that
device. If the file is not found, it opens the PS2 browser. The "name" line, under "return", gives that program
the name shown on screen.

FILE LOCATIONS
- APPS/SD2Cloud/   the application (updated by SD2Cloud itself; see UPDATING).
- SD2Cloud/        on the MMCE root (without an sd2psx, on the device SD2Cloud is opened from):
                   sd2cloud.ini (the settings, explained in Extras/sd2cloud.example.ini; if deleted,
                   SD2Cloud recreates it),
                   sync-error.txt (what happened in the last automatic sync that failed, if any: worth
                   sending along when reporting a problem),
                   state.ini (a record of what has been uploaded) and token.dat (access to your Google
                   account; do not share this file),
                   Library/ (the Library's saves), Safety/ (the safety copies) and templates/.
- Google Drive:    SD2Cloud/<device>/<group>/<card>/<card> YYYY-MM-DD HHhMM.zip, and the card's saves sent by
                   themselves in <card>/Saves (see GOOGLE DRIVE). 10 backups per card are kept by default; the
                   oldest is removed only after the new backup has been verified.

UPDATING
SD2Cloud doesn't look for updates by itself. In the settings, Updates > "Check for updates" asks GitHub whether
there is a newer version and, if there is, offers to update. The app downloads the new files, checks them against
the SHA-256 published on GitHub, replaces the old files and starts the new version. Your settings are kept.
The SAS package on the memory card in use is updated by the new version when it opens. A package on a card that
isn't in use shows as "Outdated" in the settings the next time: update it there.
To update by hand, extract the new .zip over the old installation: it doesn't have sd2cloud.ini, so your settings
are kept.
The update channel: Updates > "Update channel" chooses where updates come from.
- Stable: the released versions.
- Beta: a build made automatically from every change to the project, before the tests a release goes through. It
  gets new things sooner, and it may have bugs.
Choosing a channel looks for its update right away. Going from Beta back to Stable offers the stable version in
place of the installed beta. "About SD2Cloud" shows, after the version, the change the program was built from.

RESTORING
On the PS2: select the card, press TRIANGLE and choose "Restore a backup"; pick the backup and press X.
- If the card changed since its last backup, SD2Cloud syncs it first, so no data is lost.
- The backup is then downloaded, verified, written over the card on the MMCE and verified again.
- The card the sd2psx is using cannot be restored, because the sd2psx keeps a copy of it in its own memory:
  switch to another card on the sd2psx first.
A single save: "Versions on Drive", on its page (see BACKUP BY SAVE), or its .psu from the Files tab > Google
Drive.
On a PC: download the .zip from Drive and extract the .mcd file. Copy it to the microSD with the PS2 turned off,
or open it in mymc+ to export a single game's saves.

PRIVACY
Your data goes from the PS2 straight to your own Google Drive. No other server is involved, and nothing is
collected. You can revoke access at any time at myaccount.google.com/permissions.
Besides Google, SD2Cloud only contacts GitHub, and only when you select "Check for updates": to see whether there
is a newer version, and to download it if you choose to update.

CREDITS
The design is inspired by the PlayStation BB Navigator and the PS2 memory card browser; all images in SD2Cloud
are drawn by the app itself. Fonts: Varela Round, Nunito and M PLUS Rounded 1c (SIL Open Font License).
Developed by MrRexD.

Third-party software: see THIRD-PARTY-NOTICES.txt.
