SD2Cloud 2.0
============

Manage and back up your PS2 memory cards, right on the console.

Before you use it: SD2Cloud changes your memory cards when it copies, moves, deletes, imports or restores saved
data, and when it creates, resizes or deletes cards. Keep a backup before you do: sync your cards to Google Drive,
or copy the MemoryCards folder of the microSD to a PC.

SD2Cloud runs on the PS2 itself. It works with the memory cards stored on the MMCE (the microSD of an
sd2psx-family device with the sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero, as the
PS2 names it) and on a MemCard PRO2, and with regular Memory Cards, the OPL's virtual memory cards (VMC) and
memory card files on a USB drive, an MX4SIO or the internal HDD. No PC is required.

WHAT IT DOES
Cloud sync
- Syncs your memory cards with your Google Drive, on demand or automatically every time you exit a game with
  IGR. Only the cards that changed are uploaded.
- Keeps a history of backups for each card and restores any of them, verified before and after writing.
- Optionally sends each save that changed by itself, with its versions (Backup by save).
- Keeps the backups in folders by device, game and card, and browses them from the Files tab.
Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between cards (several items at once), and uploads a single item to
  Google Drive as a .psu file.
- Keeps saves off the cards in the Library, by game, with no size limit.
- Creates, resizes, renames, checks and deletes cards, and converts a card to Game ID (each game's saves on its
  own card). A safety copy is kept before any change that could lose data.
- Organizes the cards in tabs: numbered cards and your own folders, game (Game ID) cards in folders named after
  each game, boot cards, the OPL's VMCs and the Library.
- Imports .psu files (and saves in folder format) into any card, exports any item as a .psu, opens memory card
  files and installs them as cards (see FILES).
- Manages the regular Memory Cards in the console's slots: their saves, a whole card uploaded to Drive and
  restored from it.
- Keeps templates: sets of saves to put into cards (see TEMPLATES).
- Starts other programs: an ELF in the folders of a device, or an application kept on a memory card the Save
  Application System's way ("Start application" on its page).

REQUIREMENTS
- One of these:
  - an sd2psx-family device with the sd2psXtd firmware (MMCE support);
  - a MemCard PRO2 (its cards are the .mc2 files in the PS2 folder of its microSD). On it, "Insert this card"
    is a preview, and SD2Cloud never switches cards by itself: to change the card in use, switch to another
    one first;
  - no such device: SD2Cloud on a USB drive, an MX4SIO or the internal HDD, with regular Memory Cards in the
    slots (see WITHOUT AN SD2PSX).
  A microSD that was used in both devices has the cards of both: SD2Cloud asks, once, which device it is in
  (Settings > Cards > "Device" changes the answer), and L2 and R2 switch between the two devices' cards.
- A way to start SD2Cloud: the Apps tab of OPL, wLaunchELF or any other launcher that can run an ELF. An OPL
  with MMCE support is recommended, such as RiptOPL (https://github.com/NathanNeurotic/Open-PS2-Loader).
  Without it, OPL does not list the apps on the MMCE and has no "IGR Bootcard Slot(s)" option: with Game ID
  cards, the sd2psx stays on the game's card when you exit with IGR, the PS2 goes back to the browser instead
  of opening SD2Cloud, and the automatic sync does not happen.
- For the automatic sync after a game: OPL, which starts SD2Cloud through its IGR.
- For the cloud features: a PS2 with a network adapter (built into Slim models), connected to your router
  with a cable, and a Google account.

INSTALLATION
1. Extract this .zip file to the root of the MMCE. It adds APPS/SD2Cloud (the application) and
   SD2Cloud/sd2cloud.ini (the settings; it can be edited on a PC before the first run). The file only has
   the options: each one is explained in sd2cloud.example.ini, next to it. The cover and the logo OPL shows
   for SD2Cloud in its Apps tab go to the ART folder. The same art with its line in Portuguese is in
   Extras/ART pt-BR, to copy over those two files.
2. Open SD2Cloud: from the Apps tab of OPL, or by running APPS/SD2Cloud/SD2CLOUD.ELF with any launcher
   (wLaunchELF, for example). The folder does not have to stay in APPS.
   With an sd2psx, SD2Cloud can also live on a USB drive, an MX4SIO card or the internal HDD (exFAT or APA):
   put the APPS/SD2Cloud folder there and open SD2Cloud from there once. It records where it is, keeps on the
   MMCE (SD2Cloud/drivers) the drivers the SAS package needs to start it from that device, and updates itself
   there. Its settings and its data stay on the MMCE.
   On the memory card, the SAS package: when automatic sync is turned on, SD2Cloud puts its SAS (Save
   Application System) package on the memory card in use: the APP_SD2CLOUD folder, with what OPL runs on IGR
   and a shortcut that opens SD2Cloud wherever it is, with its own 3D icon for the PS2 browser (about
   240 KB; without an sd2psx, also the drivers of the device SD2Cloud is on, about 90 KB more). The program
   itself stays where it is. This .zip also has the package as Extras/APP_SD2CLOUD.psu, to import into
   another card by hand (SD2Cloud's own Files tab does that): open SD2Cloud once from where it is, and the
   shortcut finds it.
3. Without a connected Google account, SD2Cloud asks whether to connect one now. Choosing "Not now" opens the
   main screen directly; "Don't ask again" (TRIANGLE) does the same and stops the question for good
   (ask_connect = no in sd2cloud.ini). The account can be connected later in Settings > Google Drive. To
   connect, visit google.com/device on your phone or computer (or use the QR code) and enter the code shown on
   the TV. SD2Cloud can only access the files it creates in your Drive.
4. Right after the account is connected, SD2Cloud asks whether to turn on automatic sync (see AUTOMATIC
   SYNC). If you accept, it installs the SAS package and shows the path to set in OPL; if you decline, it can
   be turned on later in Settings > Automatic sync.
5. SD2Cloud then offers to sync all memory cards. After that, only changed cards are uploaded. Each sync
   creates a backup of the card on Drive.
   Even without syncing, SD2Cloud records the state of each card the first time it sees it: the automatic sync
   after IGR then uploads only the cards that change from that point on.

WITHOUT AN SD2PSX
Extract the .zip to a USB drive (FAT32 or exFAT), an MX4SIO card or the internal HDD, and open
APPS/SD2Cloud/SD2CLOUD.ELF from there. SD2Cloud keeps its settings and its data on that device. Its cards are
the OPL's VMCs of that device (the VMC folder), and the Files tab has the regular Memory Cards of both slots
("Memory Card 1" and "Memory Card 2"), besides the devices: their saves are copied, moved, deleted, uploaded to
Drive and restored as any card's. What SD2Cloud syncs are the Memory Cards, save by save (Backup by save is on by
default without an sd2psx), and the first sync offers them. An sd2psx's or a MemCard PRO2's folders on that
device are just files there: open them in the Files tab. For the automatic sync, install the SAS package
(Settings > Automatic sync > "SAS package"): it goes to the Memory Card with the drivers of the device SD2Cloud
is on, and OPL's IGR opens SD2Cloud through it. The sd2psx can be taken out and put back with SD2Cloud open:
the screens follow.

MAIN SCREEN
The memory cards are listed on the left with a status indicator (green = synced; yellow = not synced or
never synced). The selected card is shown on the right with the date of its last backup. Press Up and Down to
select a card. The O button exits the application: choose where to go next (the programs in the APPS folder,
"Run an ELF..." or the PS2 browser); the last choice is remembered.
The cards are grouped into tabs shown above the list: Cards (the numbered cards, Card1, Card2..., folders with
names of your own, and "New card" as the last row), Games (Game ID cards, in folders named after each game:
X opens a folder, O goes back), Boot (the BootCard), OPL VMCs (see OPL VMCs), Library (see THE LIBRARY,
shown once it has saves) and Files (see FILES). Press Left and Right, or L1 and R1, to switch tabs. Tabs
without cards are not shown.
On a microSD with the cards of both devices, the two are shown at the top right: L2 lists the left one's cards,
R2 the right one's, to copy saves between them.
A folder that the sd2psx's .sd2psx/Game2Folder.ini gives to several games (a series that shares one card) is
shown under those games' names: what they have in common ("Need for Speed"), or the names themselves.
A game card that file left behind (the sd2psx opens another folder for its game now) has a grey arrow, and its
options offer to move its saves to the card the sd2psx opens.
- X opens the card: its saved data, displayed as in the PS2 browser, the newest first.
- TRIANGLE opens the selected card's options:
  - Sync now; Disable sync (the card is left out of every sync, the automatic one too, until "Enable sync" is
    picked there; its status is then "Excluded"); Restore a backup (see RESTORING).
  - Copy to a device: the whole card goes, as a file, to a folder of a device. Pick the device, go to the
    folder and press TRIANGLE; SD2Cloud asks whether the card goes as a .mcd (sd2psx) or a .ps2 (PCSX2) and
    writes it inside a .zip, as the backups on Drive are; then it reads the .zip back and compares it.
  - Insert into sd2psx: makes the sd2psx take that card, as its own buttons would. The sd2psx takes a BootCard
    only with Autoboot turned on in its settings, and a game's card only with Game ID; a folder with a name of
    its own can only be picked on the sd2psx itself. On the list, a small card marks the one in use.
  - Rename, Change size, Convert to Game ID, Check card and Delete card: see CARDS.
  On a folder of the Games tab, TRIANGLE has Rename and Delete folder.
- The Games tab starts with "All saves": the saved data of every game card on one screen, the newest first,
  as if it were all on a single card. Each item still belongs to its own card, whose name is at the top of
  the item's page, and what is done to an item is done to that card. An item that more than one card has (the
  same folder with the same files, byte for byte, as a template put into every card) is shown once.
- TRIANGLE, with a card's items on screen, opens the options: "Sync now", for that card; "Sort by" (the date,
  descending or ascending, or the name of their game), for every card; "Convert to Game ID"; and, in "All
  saves", "Duplicates", hidden or shown, and "Export saves to a card". On the Memory Card: "Upload to Drive"
  and "Restore a backup" (see FILES).
- START opens the settings (see SETTINGS), and SELECT the tools (see TEMPLATES and SAFETY COPIES).
- X on a saved data item opens its page, with Copy, Move, Delete, Upload to Drive and Versions on Drive (see
  BACKUP BY SAVE).
- Copy marks the item (a green tick over its icon) and goes back to the card's items, where X now marks and
  unmarks others; O leaves the card for the list of cards, to mark items on other cards too; TRIANGLE (Cancel)
  gives the selection up, asking first when more than one item is marked. The marks stay from card to card,
  and their count is on screen. START (Paste) pastes them all into the card it is pressed on: the one that is
  open, or the one under the cursor in the list of cards (a card without enough space for them is left as it
  is). Pressed where they can't go (the card they all came from, or "All saves"), it goes to the list of
  cards. In the list's Games tab, when the items are all of one game, that game's folder comes first, in
  green, with "New card" in it; while the game has no folder, "New card" comes first among the folders. The
  Library tab of the list keeps them off the cards (see THE LIBRARY). The Files tab of the list leads to the
  folders of a device (see FILES) and to the Memory Card.
- Move takes the item to another card, chosen the same way. Upload to Drive sends that item only, in .psu
  format, to the card's Saves folder on Drive. To change the card the sd2psx is using, SD2Cloud asks first,
  then switches the sd2psx to another card, makes the change and switches back; only for a folder with a name
  of its own the card has to be switched on the sd2psx beforehand.
Every operation that uploads, writes or deletes data asks for confirmation first. During a sync, the O
button cancels the upload; cards already uploaded remain on Drive. While several cards are sent, SQUARE asks
about the one on its way: SQUARE again skips it this time, TRIANGLE skips it and turns its sync off. SELECT
shows the list of the sync's cards, with what became of each, and goes back: on it, SQUARE skips a card at
once (again: undo), and X offers to skip it, to mark it as resolved (the automatic sync leaves it alone until
it changes again) or to turn its sync off.

CARDS
- New card (the last row of the Cards tab, and of a list where saves are pasted): a numbered card, a card in a
  folder of your own (an existing one or a new one) or, in the Files tab, a card file on a device (.mcd, .mc2,
  .ps2 or OPL's .bin: pick the folder and press TRIANGLE, "Create here"). Its size is picked next: 1 to 64 MB
  on an sd2psx, 8 to 128 MB on a MemCard PRO2; the size recommended for the saves going in comes selected.
  In the Games tab, a game's "New card" adds a channel to that game's folder. Settings > Cards > "New card
  size" sets the size the sd2psx itself uses for a game's first card (the sd2psx reads it when it starts).
- Rename: a numbered card, a game's card or a BootCard keeps its file (the sd2psx finds it by that name): what
  changes is the name the sd2psx shows for it, in its folder's .ini. A folder of your own is renamed with its
  cards and files. Backups already on Drive keep the previous name.
- Change size: the saves are copied to a new card of the size picked, which takes the current one's place
  after it is verified; if anything fails, the current card stays as it was.
- Convert to Game ID: the card's saves (also the Memory Card's or a card file's) are listed by game, each with
  the card it goes to: the game's card, or "New card" with its size (TRIANGLE changes it). X leaves a game out;
  SQUARE unselects them all, to pick a few (with none selected, it selects them all again); START copies them
  all, on one screen with the total time left. A save the game's card already has in another version is asked
  about once (keep the card's, replace it, or keep the newest). New cards get the game's templates. At the end,
  R2 removes the copied saves from the source card, after asking; by default it stays as it was. Only with an
  sd2psx or a MemCard PRO2: game cards are theirs.
- Check card: looks through the card's file system (its chains of clusters, its entries) and says what is
  wrong, if anything. Nothing is written.
- Delete card: asks first. The card the device is using is let go of first (the device moves to another one).
  When it was its folder's last card, the folder goes too.
- Delete folder (TRIANGLE on a folder of the Games tab): all of its cards, after asking.

SAFETY COPIES
Before a card is deleted, resized or has saves moved out of it, and before a conversion to Game ID removes the
copied saves, SD2Cloud keeps a copy of the whole card in SD2Cloud/Safety on the MMCE (the 10 newest are
kept). Tools (SELECT) > Safety copies lists them: X puts one back, in place of the card or as the card again
when it was deleted, or deletes it.

OPL VMCS
The OPL's virtual memory cards (VMC/*.bin on the MMCE) are the cards of the OPL VMCs tab, with the games that
use each one ("Linked games", from the OPL's settings of each game in the CFG folder). They are opened,
synced, restored, resized and deleted like any card. "New VMC", the last row, makes one (8 to 64 MB, named
generic_0, generic_1... unless you type another name). Rename renames the file and the OPL's settings of the
games that use it. Settings > Cards > "OPL VMC folders" adds the VMC folders of the internal HDD (exFAT or APA),
an MX4SIO or a USB drive: each place is a folder of the tab.

THE LIBRARY
The Library keeps saves off the cards: a Memory Card that is full, or a save you want out of a card without
deleting it. Move or Copy a save (or paste marked saves, or import a .psu) and pick the Library tab: its row
says the game the save goes under ("Several games" for marked saves of more than one). Each save is kept as a
file of its own, by game, with no size limit, in SD2Cloud/Library on the MMCE. The Library tab of the main
screen, shown once it has saves, lists its games; X opens one, and its saves are moved, copied or deleted as
any other, back to any card. Settings > Google Drive > "Library on Drive" also sends them to Google Drive, each
by itself, into the Library's folder there.

FILES
The last tab of the main screen, Files, lists the devices whose folders can be browsed: the MMCE, a USB drive
(FAT32 or exFAT), the internal HDD (exFAT, FAT32 or APA; on a disk with both, as the PSBBN installer leaves
it, SD2Cloud asks which side to open: "exFAT partition" or "APA partitions") and an MX4SIO (listed when slot 2
has no PS2 memory card in it: its driver would take the slot). The drivers of a device are loaded only when
it is opened. X opens the device, a folder, a .psu file or a memory card file; O goes back one folder; Left and
Right move a page at a time. Every folder and file is listed, but only those files can be opened: the buttons
at the bottom of the screen are the ones that do something with the selected entry.
- Importing: X on a .psu file opens the page of the saved data it holds (name, icon, date and size). "Import
  to card" then asks for the card, chosen from the same tabs as the main screen. The file is checked whole
  before the card is touched, and what is written is read back and compared. A card that already has data
  with the same name, or without enough space, is left as it is: delete or move that item first.
  SQUARE on the file, in the list, goes straight to the card, without opening the page.
- Saves in folder format are installed like a .psu. Select the folder, or go into it, and press SQUARE
  ("Install save"): the save's page opens, and "Import to card" puts it on a card. X still opens the folder
  as usual. Files a computer adds to a folder (Thumbs.db, desktop.ini, .DS_Store) are ignored.
- Exporting: Copy, on an item's page, also lists the Files tab among the destinations of the marked items;
  pick the device, go to the folder where the files should go and press START. Each file gets its item's
  folder name (BASLUS-21065SAVE.psu, for example), is read back and compared. When a file with that name is
  already there, SD2Cloud asks: X replaces it, SQUARE keeps both (the new one gets a number:
  BASLUS-21065SAVE (2).psu).
- Memory card files: X on a .mcd, a .mc2 (MemCard PRO2), a .ps2 (PCSX2), a .bin (OPL's VMC) or a .zip written
  by "Copy to a device" opens the card it holds, shown as the cards of the MMCE are. X on an item opens its
  page, from where it can be copied anywhere, alone or with other items of that file. A .mcd, .mc2 or .bin
  card outside a .zip takes items too: during a copy, X on the file opens it and START there pastes them into
  it, as does START on the file in the list. A .ps2 (its pages carry ECC bytes) and a .zip's card are only read.
- Installing a memory card file: SQUARE, on the file in the list or on the screen of its card, asks where the
  card goes, in the same tabs as the main screen. "New card" makes a new card: the lowest free number in
  Cards, the next channel of the BootCard in Boot and, in Games, the next channel of the game's folder, which is
  created when the game has none. The game is told by the file's name (SLUS-21065-1.mcd, or SLUS_210.65_0.bin
  as OPL names them) or by the saved data inside. X on an existing card replaces it with the file's card,
  after you confirm. The file is read whole before anything is written, and the card is read back afterwards.
  The Files tab of that same screen writes the card as a file on another device: pick the device, go to the
  folder, press TRIANGLE ("Save here") and choose the format (the sd2psx's .mcd, the MemCard PRO2's .mc2,
  PCSX2's .ps2 with its ECC, OPL's .bin, or the .zip as it is, when the card came in one) and the file's name.
- The Memory Card: a regular PS2 memory card in the console's other slot (both slots, without an sd2psx) is a
  row of the Files tab. X shows its items, which are copied, moved, deleted and uploaded to Drive as a card's
  are, and items go into it from the cards or from .psu files. TRIANGLE, on its items, has "Upload to Drive"
  (the whole card, read into a card file and sent as a backup of its own, with its history) and "Restore a
  backup" (the backup's saves are copied into it; saves with the same folder are replaced, the others stay).
- Google Drive: with a Google account, the last row. It opens the main folder of the backups on Drive, browsed
  as a device's folders are (SD2Cloud only sees what it made there). X on a save's .psu opens its page, with
  "Import to card", "Download" and "Delete from Drive". X on a card's backup (.zip) opens the card in it, as a
  memory card file is opened: its saves, copied out of it from their pages, and SQUARE installs the card, on the
  microSD or as a file on a device. SQUARE, on a card's backup in the list, goes straight to that installing; on
  another file, it downloads it as it is, into a folder of a device (pick the device, go to the folder and press
  TRIANGLE, "Save here"). TRIANGLE deletes it. A file deleted goes to Drive's trash, where it can be restored
  for 30 days.
Changing the card the sd2psx is using, by importing into it or by replacing it, works as any other change
to it: SD2Cloud asks, switches the sd2psx to another card and back.
Names with accented letters may be shown abbreviated, or not open at all (a limit of the FAT driver of the
PS2 SDK): prefer plain names for the folders you use here.

TEMPLATES (SELECT)
A template is a set of saved data items you pick, kept on the MMCE to be put into cards. The device makes
the card of a new game empty: a template gives it what every card of yours should have, as the network
settings an online game asks for. SELECT, on the main screen, opens the tools; Templates is the first.
- New template: its name is typed on a keyboard on the screen (X types, SQUARE erases, START finishes). Then
  the cards are shown as on the main screen: X opens one, and inside it X marks and unmarks an item. The marks
  stay from card to card, and their count is at the top. START ends the marking, and the items are copied to
  the template; TRIANGLE gives it up.
- X on a template opens it, shown as a card is. X on an item removes it from the template. TRIANGLE, on a
  template of the list or with it open, has the rest: Set as default, Add saves (marked the same way; an
  item the template already has can be replaced by the one marked), Apply template, Linked games, Rename
  and Delete template.
- Apply template puts into cards the items they lack: an item a card already has is never touched, and a
  card without enough space is left as it is. All game cards: every game card that lacks items of the
  template gets them, one after the other, and SD2Cloud says how many did. Select cards: the same, for the
  cards you mark, game cards or not (X marks and unmarks, START applies). Update existing saves is the one
  that replaces: on the game cards, the items of the template that a card already has in another version
  give way to the template's. No other item of the card is changed, an item the card lacks is not added,
  and it asks first. A template's items are no change to sync: a card that was synced stays synced.
- Set as default: the default template is the one every game card should have (a yellow light marks it in the
  list). When SD2Cloud opens and a game card lacks it, as the card of a new game does, SD2Cloud says which
  and waits for an answer: X applies it, O leaves it for later, SQUARE stops the warning for those cards. A
  card is asked about once; it is asked about again only when its templates get other items.
- Linked games: a template that isn't the main one can be made some games': X marks and unmarks
  each game (a green light). The cards of those games are then treated as the default template treats every
  game card.
- Auto-apply (the last row of the list, On or Off): with the automatic sync, when a game is left with
  IGR the game cards about to be sent get the items they lack of their templates, with no question asked,
  before they are sent. A card is only changed then when it surely isn't the one in the sd2psx (OPL has it
  back on the BootCard by that time); any other is left for the warning when SD2Cloud is next opened.
- Applied by hand to the card the sd2psx is using, a template goes in through the memory card slot, as a
  game would write it.
A template is a folder of SD2Cloud/templates on the MMCE with a .psu file for each item: a .psu put there
with a PC is part of the template too.

SETTINGS (START)
The first page has Sync all cards, the language and About, and four groups, each a page of its own (X opens
it, O goes back, START leaves the settings from any page):
- Sync all cards: uploads the cards that are not synced; if all of them are, SD2Cloud says so and lets you
  sync them all again.
- Google Drive:
  - Google account: disconnects the account (backups on Drive are kept) or connects it again.
  - Backups kept per card (3, 5, 10, 20, 50 or no limit): how many backups of each card stay on Drive; past
    that, the oldest is removed.
  - Backup format: .mcd (the sd2psx's own) or .ps2 (the format PCSX2 uses: take the file out of the backup's
    zip and put it in PCSX2's memory card folder). Either one can be restored, whatever is set.
  - Backup by save: see BACKUP BY SAVE.
  - Library on Drive: see THE LIBRARY.
  - Drive organization: shown while the backups on Drive are in the earlier organization (see GOOGLE DRIVE).
- Automatic sync:
  - Automatic sync: On or Off. Off, exiting a game with IGR goes straight to the program of "IGR path",
    without showing SD2Cloud. It also stays off while no Google account is connected.
  - SAS package: installs SD2Cloud's folder on the memory card in use (what OPL runs on IGR, and a shortcut
    for the PS2 browser; see AUTOMATIC SYNC). Once it is installed, the same option reinstalls it (or updates
    it) or uninstalls it.
  - IGR path: the program started after the IGR sync. The choices are Automatic (the OPL found on the
    MMCE), the programs in the APPS folder and the PS2 browser.
- Cards: Device (on a microSD with the cards of both devices), New card size (see CARDS) and OPL VMC folders
  (see OPL VMCS).
- Updates: Check for updates (looks for a new version right away and, if there is one, offers to update) and
  Update channel: Stable (the released versions) or Beta (see UPDATING).
- Language.
- About SD2Cloud: version, credits and licenses.
Changes are saved to sd2cloud.ini, which can also be edited on a PC.

BACKUP BY SAVE
Settings > Google Drive > "Backup by save" (Off by default with an sd2psx; On without one, when the Memory Cards
are what there is to sync): after each card is sent, each of its saves that changed is also sent by itself, as a
.psu, to the card's Saves folder on Drive, and as many versions of each are kept as the card's backups. The sync
also covers the Memory Card in the other slot (without an sd2psx, the ones in both slots), save by save.
"Versions on Drive", on a save's page, lists the versions of that save: X on one puts it back in the save's
place (the save as it is now is kept until the version is in).

GOOGLE DRIVE
The backups are kept in SD2Cloud, the main folder on your Drive: a folder for the device (sd2psx or MemCard
PRO2) with a folder for each group of cards (a game's named after the game, as "Need for Speed - Underground 2
[SLUS-21065]") and, in it, a folder for each card, with its backups (.zip) and its Saves folder; beside it, the
OPL's VMCs, the Memory Card in a slot and the Library, each in a folder of its own. The folders are named in the
language of the screens when they are made; SD2Cloud finds them by a tag of its own, so they can be renamed or
moved on Drive.
A Drive with backups from a version before 2.0 (every group of cards side by side in "PS2 Memory Card
Backups") is offered the new organization once, at startup: "Organize" moves every file into it, with nothing
uploaded again (with many backups, it can take a few minutes); the old folders, empty, go to Drive's trash, and
the main folder becomes SD2Cloud. If the move stops halfway, it carries on the next time SD2Cloud opens. "Not
now" keeps the earlier organization; Settings > Google Drive > "Drive organization" offers it again.

AUTOMATIC SYNC AFTER A GAME (IGR)
1. In SD2Cloud, open Settings (START) > Automatic sync and select "SAS package". SD2Cloud shows how much space
   it takes on the memory card in use (about 240 KB) and how much is free, and asks before installing. It
   writes the APP_SD2CLOUD folder (mc0:/APP_SD2CLOUD). If your sd2psx uses Autoboot, install it while the
   BootCard is in use.
2. In OPL Settings, set "IGR Path" to: mc0:/APP_SD2CLOUD/IGR.ELF
   (mc1: with the sd2psx in slot 2: SD2Cloud shows the path to use when it installs the package)
   If a version up to 1.5 left its helper on the card (mc0:/BOOT/SD2CLOUD-IGR.ELF), that path keeps working:
   installing the package also brings that file up to date.
3. In OPL, also turn on "IGR Bootcard Slot(s)" (MMCE page), set to the sd2psx slot or BOTH: when you exit a
   game, OPL switches back to the BootCard, where the package is installed.
From then on, exiting a game with IGR starts SD2Cloud: it uploads the changed cards, shows "Upload complete"
and returns to OPL. In this mode, the O button cancels from the first screen on, also while the cards are
checked and while it connects, and cancelling also returns to OPL.
SD2Cloud can be kept in any folder, on the MMCE or on another device: every time it is opened, it records where
it is (in sd2cloud.ini on the MMCE, "app_path"; without an sd2psx, in the package itself, with the drivers of that
device), and the package starts it from there. A folder of APPS on the MMCE that was renamed or moved is found by
the package as well, even before SD2Cloud is opened from it.
To uninstall the package, select "SAS package" again and choose "Uninstall"; then change "IGR Path" in OPL.
To pause the sync and keep everything installed, set "Automatic sync" to Off in Settings: IGR then goes
straight to the program opened after it, without starting SD2Cloud. The same happens while no Google account
is connected.
Without installing anything (USB drive): OPL's IGR can also start a program from a USB drive. If the
APPS/SD2Cloud folder is on one, formatted as FAT32, skip step 1 and set "IGR Path" to:
mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF
Nothing is written to the memory card. For this, OPL loads the USB drivers USBD.IRX and USBHDFSD.IRX from
mc?:/SYS-CONF, where FMCB installs them. With the SAS package on the memory card none of that is needed,
wherever SD2Cloud is: the package starts it from the MMCE, a USB drive, an MX4SIO or the HDD.
The program opened after IGR is chosen in Settings > Automatic sync > "IGR path", from the programs in the
APPS folder of the MMCE. If your OPL is on another device, enter its path in sd2cloud.ini, in the [igr]
section, starting with the device: mc?:/ (memory card), mass:/ (USB), mx4sio:/, ata:/ (internal HDD, exFAT) or
hdd0:PARTITION:pfs:/ (internal HDD, APA), for example hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF. SD2Cloud loads
only the drivers of that device, when returning. If the file is not found, it opens the PS2 browser. The "name"
line, under "return", gives that program the name shown on screen.

FILE LOCATIONS
- APPS/SD2Cloud/   the application (updated by SD2Cloud itself; see UPDATING).
- SD2Cloud/        on the MMCE root (without an sd2psx, on the device SD2Cloud is opened from):
                   sd2cloud.ini (the settings; if deleted, SD2Cloud recreates it),
                   sd2cloud.example.ini (what each setting means; SD2Cloud never reads it),
                   sync-error.txt (what happened in the last automatic sync that failed, if any: worth
                   sending along when reporting a problem),
                   state.ini (a record of what has been uploaded) and token.dat (access to your Google
                   account; do not share this file),
                   Library/ (the Library's saves), Safety/ (the safety copies) and templates/.
- Google Drive:    SD2Cloud/<device>/<group>/<card>/<card> YYYY-MM-DD HHhMM.zip, and the card's saves sent by
                   themselves in <card>/Saves (see GOOGLE DRIVE). 10 backups per card are kept by default; the
                   oldest is removed only after the new backup has been verified.

UPDATING
SD2Cloud does not look for updates by itself. In the settings, Updates > "Check for updates" asks GitHub
whether there is a newer version and, if there is one, offers to update. The application downloads the new
files, verifies them against the SHA-256 published on GitHub, replaces the previous files and starts the new
version. Your settings are kept.
The SAS package on the memory card in use is updated by the new version when it opens. A package on a card
that isn't in use then shows as "Outdated" in the settings the next time: update it there.
To update manually, copy only the APPS/SD2Cloud folder from the new .zip file (or do not replace
sd2cloud.ini when prompted).
The beta channel: Updates > "Update channel" chooses where the updates come from. Stable is the released
versions. Beta is a build made automatically from every change to the project, before it is tested the way a
version is: it has what is new sooner, and it may have bugs. Choosing a channel looks for its update right
away; going from Beta back to Stable offers the stable version in place of the beta installed. "About
SD2Cloud" shows, after the version, the change the program was built from.

RESTORING
On the PS2: select the card, press TRIANGLE and choose "Restore a backup"; choose the backup and press X.
If the card has changed since its last backup, SD2Cloud syncs it first, so no data is lost.
The backup is then downloaded, verified, written over the card on the MMCE and verified again.
The card currently in use by the sd2psx cannot be restored, because the sd2psx keeps a copy of it in its
own memory: switch to another card on the sd2psx first.
A single save: "Versions on Drive", on its page (see BACKUP BY SAVE), or its .psu from the Files tab >
Google Drive.
On a PC: download the .zip file from Drive and extract the .mcd file. Copy it to the microSD with the PS2
turned off, or open it in mymc+ to export the data of a single game.

PRIVACY
Your data goes from the PS2 directly to your own Google Drive. No other server is involved and no
information is collected. Access can be revoked at any time at myaccount.google.com/permissions.
Besides Google, SD2Cloud only communicates with GitHub, and only when you select "Check for updates": to
see whether there is a newer version and to download it if you choose to update.

CREDITS
The design is inspired by the PlayStation BB Navigator and the PS2 memory card browser; all images in
SD2Cloud are generated by the application itself. Fonts: Varela Round, Nunito and M PLUS Rounded 1c (SIL Open
Font License).
Developed by MrRexD.

Third-party software: see THIRD-PARTY-NOTICES.txt.
