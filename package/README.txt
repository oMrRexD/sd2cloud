SD2Cloud 1.4
============

Manage and back up your sd2psx memory cards, right on your PS2.

WARNING: SD2Cloud is at an early stage and has not been widely tested yet. Before copying, moving, deleting,
importing or restoring saved data with it, back up your memory cards: copy the MemoryCards folder of the microSD
to a PC.

SD2Cloud runs on the PS2 itself and works with the memory cards stored on the microSD of sd2psx-family devices
running the sd2psXtd firmware (sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No PC is required.

WHAT IT DOES
Cloud sync
- Syncs your memory cards with your Google Drive, on demand or automatically every time you exit a game with
  IGR. Only the cards that changed are uploaded.
- Keeps a history of backups for each card and restores any of them, verified before and after writing.
Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between the cards on the microSD, and uploads a single item to Google
  Drive as a .psu file. Saved data can also go to a new card of its own game, made on the spot.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards, in folders named after each game, and
  boot cards.
- Imports .psu files into any card and exports any item of a card as a .psu file, from and to the microSD or
  a USB drive (see FILES).
- Opens memory card files from the microSD or a USB drive (.mcd, a MemCard PRO2's .mc2, PCSX2's .ps2, OPL's
  virtual memory cards (.bin), or the .zip SD2Cloud itself writes) and installs them as cards of the sd2psx
  (see FILES).
- Starts other programs: X on an ELF in the folders of the microSD or of a USB drive runs it (also from "Run
  an ELF..." in the list shown when leaving), and an application kept on a memory card with a title.cfg (the
  Save Application System's way) gets "Start application" on its page: the sd2psx is switched to that card
  and the program runs from the memory card slot.

REQUIREMENTS
- An sd2psx-family device with the sd2psXtd firmware (MMCE support).
  A MemCard PRO2 is recognized too (its cards are the .mc2 files in the PS2 folder of its microSD), but
  SD2Cloud has not been tried on one yet: take that as experimental. On it, "Insert this card" is a
  preview, and SD2Cloud never switches cards by itself: to change the card in use, switch to another one first.
- A way to start SD2Cloud: the Apps tab of OPL, wLaunchELF or any other launcher that can run an ELF. An
  OPL with MMCE support is recommended, such as RiptOPL (https://github.com/NathanNeurotic/Open-PS2-Loader).
  Without it, OPL does not list the apps on the sd2psx microSD and has no "IGR Bootcard Slot(s)" option: with
  Game ID cards, the sd2psx stays on the game's card when you exit with IGR, the PS2 goes back to the browser
  instead of opening SD2Cloud, and the automatic sync does not happen.
- For the automatic sync after a game: OPL, which starts SD2Cloud through its IGR.
- For the cloud features: a PS2 with a network adapter (built into Slim models), connected to your router
  with a cable, and a Google account.

INSTALLATION
1. Extract this .zip file to the root of the sd2psx microSD. It adds APPS/SD2Cloud (the application) and
   SD2Cloud/sd2cloud.ini (the settings; it can be edited on a PC before the first run). The file only has
   the options: each one is explained in sd2cloud.example.ini, next to it.
2. Open SD2Cloud: from the Apps tab of OPL, or by running APPS/SD2Cloud/SD2CLOUD.ELF with any launcher
   (wLaunchELF, for example). The folder does not have to stay in APPS.
   If your OPL does not list the apps on the sd2psx microSD (a build without MMCE support), also copy the
   APPS/SD2Cloud folder to the APPS folder of your USB drive or MX4SIO card and open SD2Cloud from there. It
   hands over to the copy on the microSD, where the settings and the updates are kept.
   On the memory card, the SAS package: when automatic sync is turned on, SD2Cloud puts its SAS (Save
   Application System) package on the memory card in use: the APP_SD2CLOUD folder, with what OPL runs on IGR
   and a shortcut that opens SD2Cloud from the microSD, with its own 3D icon for the PS2 browser (about
   240 KB). The program itself stays on the microSD. This .zip also has the package as
   Extras/APP_SD2CLOUD.psu, to import into another card by hand (SD2Cloud's own Files tab does that); with
   no SD2Cloud on the microSD, the shortcut only says so.
3. Without a connected Google account, SD2Cloud asks whether to connect one now. Choosing "Not now" opens the
   main screen directly; "Don't ask again" (TRIANGLE) does the same and stops the question for good (ask_connect = no in sd2cloud.ini).
   The account can be connected later in Settings (START). To connect, visit
   google.com/device on your phone or computer (or use the QR code) and enter the code shown on the TV.
   SD2Cloud can only access the files it creates in your Drive.
4. Right after the account is connected, SD2Cloud asks whether to turn on automatic sync (see AUTOMATIC
   SYNC). If you accept, it installs the SAS package and shows the path to set in OPL; if you decline, it can
   be turned on later in Settings (START).
5. SD2Cloud then offers to sync all memory cards. After that, only changed cards are uploaded. Each sync
   creates a backup of the card on Drive.
   Even without syncing, SD2Cloud records the state of each card the first time it sees it: the automatic sync
   after IGR then uploads only the cards that change from that point on.

MAIN SCREEN
The memory cards are listed on the left with a status indicator (green = synced; yellow = not synced or
never synced). The selected card is shown on the right with the date of its last backup. Press Up and Down to
select a card. The O button exits the application: choose where to go next (the programs in the APPS folder or
the PS2 browser); the last choice is remembered.
The cards are grouped into tabs shown above the list: Cards (the numbered cards, Card1, Card2..., and folders
with custom names), Games (Game ID cards, in folders named after each game: X opens a
folder, O goes back) and Boot (the BootCard). Press Left and Right, or L1
and R1, to switch tabs. Tabs without cards are not shown. The last tab, Files, has no cards: it browses the
microSD and a USB drive for .psu files and memory card files (see FILES).
A folder that the sd2psx's .sd2psx/Game2Folder.ini gives to several games (a series that shares one card) is
shown under those games' names: what they have in common ("Need for Speed"), or the names themselves.
A game card that file left behind (the sd2psx opens another folder for its game now) has a grey arrow, and its
options offer to move its saves to the card the sd2psx opens.
- X opens the card: its saved data, displayed as in the PS2 browser, the newest first. On that screen, SQUARE
  syncs the card; if it is already synced, SD2Cloud says so and lets you sync it again.
- TRIANGLE opens the selected card's options: "Sync now", "Disable sync" (the card is left out of every sync,
  the automatic one too, until "Enable sync" is picked there; its status is then "Excluded"), "Restore a
  backup" (see RESTORING) and "Copy to a device": the whole card goes, as a file, to a folder of the microSD or of a USB drive. Pick the device, go to
  the folder and press TRIANGLE; SD2Cloud asks whether the card goes as a .mcd (sd2psx) or a .ps2 (PCSX2) and
  writes it inside a .zip, as the backups on Drive are (a card is mostly empty space, and its .zip takes a
  fraction of the time to write); then it reads the .zip back and compares it. The last one, "Insert into
  sd2psx", makes the sd2psx take that card, as its own buttons would. The sd2psx takes a BootCard only with
  Autoboot turned on in its settings, and a game's card
  only with Game ID; a folder with a name of its own can only be picked on the sd2psx itself. On the list, a
  small card marks the one the sd2psx is using.
- The Games tab starts with "All saves": the saved data of every game card on one screen, the newest first,
  as if it were all on a single card. Each item still belongs to its own card, whose name is at the top of
  the item's page, and what is done to an item is done to that card.
- START opens the settings (see SETTINGS), and SELECT the tools (see TEMPLATES).
- X on a saved data item opens its page, with Copy and Move (to another card on the microSD, chosen from the
  same tabs as the main screen; the destination's free space is shown, and a card without enough space cannot
  be selected; when the item's name tells its game, the Games tab also has "New card", in that game's folder
  or among the folders while the game has none: an empty 8 MB card is made for the item, as the device itself
  makes them), Delete and Upload to Drive (that item only, in .psu format, in a "Saves" folder next to the
  card's backups). To change the card the sd2psx is using, SD2Cloud asks first, then switches the sd2psx to
  another card, makes the change and switches back; only for a folder with a name of its own the card has to
  be switched on the sd2psx beforehand.
Every operation that uploads, writes or deletes data asks for confirmation first. During a sync, the O
button cancels the upload; cards already uploaded remain on Drive. While several cards are sent, SQUARE asks
about the one on its way: X skips it this time, SQUARE skips it and turns its sync off.

FILES
The last tab of the main screen, Files, lists the devices whose folders can be browsed: the sd2psx microSD
and a USB drive (FAT32 or exFAT). The USB drivers are loaded only when the USB drive is opened. X opens the
device, a folder, a .psu file or a memory card file; O goes back one folder; Left and Right move a page at a
time. Every folder and file is listed, but only those files can be opened: the buttons at the bottom of the
screen are the ones that do something with the selected entry.
- Importing: X on a .psu file opens the page of the saved data it holds (name, icon, date and size). "Import
  to card" then asks for the card, chosen from the same tabs as the main screen. The file is checked whole
  before the card is touched, and what is written is read back and compared. A card that already has data
  with the same name, or without enough space, is left as it is: delete or move that item first.
  SQUARE on the file, in the list, goes straight to the card, without opening the page.
- Exporting: on an item's page (X on it, in a card), Copy also lists the Files tab among the destinations;
  pick the device, go to the folder where the file should go and press TRIANGLE. The file gets the item's
  folder name (BASLUS-21065SAVE.psu, for example), is read back and compared. When a file with that name is
  already there, SD2Cloud asks: X replaces it, SQUARE keeps both (the new one gets a number:
  BASLUS-21065SAVE (2).psu).
- Memory card files: X on a .mcd, a .mc2 (MemCard PRO2), a .ps2 (PCSX2), a .bin (one of OPL's virtual memory
  cards, VMC) or a .zip written by "Copy to a device" opens the card it holds, shown as the cards of the
  microSD are. The file is only read. X on an item opens its page, from where it can be copied to a card of
  the microSD.
- Installing a memory card file: SQUARE, on the file in the list or on the screen of its card, asks where the
  card goes, in the same tabs as the main screen. "New card", at the top of each list, makes a new card: the
  lowest free number in Cards, the next channel of the BootCard in Boot and, in Games, the next channel of
  the game's folder, which is created when the game has none. The game is told by the file's name
  (SLUS-21065-1.mcd, or SLUS_210.65_0.bin as OPL names them) or by the saved data inside; when there is more
  than one, SD2Cloud asks which. Inside a game's folder, "New card" adds a channel to that game. When a
  folder already has every channel the sd2psx goes to in it (8, unless the folder's .ini says otherwise),
  SD2Cloud offers to raise that limit by one (MaxChannels, in that .ini). X on an existing card replaces it
  with the file's card, after you confirm. The file is read whole before anything is written, and the card is
  read back afterwards. A .zip whose card doesn't fit in the PS2's memory (more than 16 MB) can't be opened,
  but can be installed.
Changing the card the sd2psx is using, by importing into it or by replacing it, works as any other change
to it: SD2Cloud asks, switches the sd2psx to another card and back.
Names with accented letters may be shown abbreviated, or not open at all (a limit of the FAT driver of the
PS2 SDK): prefer plain names for the folders you use here.

TEMPLATES (SELECT)
A template is a set of saved data items you pick, kept on the microSD to be put into cards. The device makes
the card of a new game empty: a template gives it what every card of yours should have, as the network
settings an online game asks for. SELECT, on the main screen, opens the tools; Templates is the first.
- New template: its name is typed on a keyboard on the screen (X types, SQUARE erases, START finishes). Then
  the cards are shown as on the main screen: X opens one, and inside it X marks and unmarks an item. The marks
  stay from card to card, and their count is at the top. START ends the marking, and the items are copied to
  the template.
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
  and it asks first.
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
A template is a folder of SD2Cloud/templates on the microSD with a .psu file for each item: a .psu put there
with a PC is part of the template too.

SETTINGS (START)
- Sync all cards: uploads the cards that are not synced; if all of them are, SD2Cloud says so and lets you
  sync them all again.
- Automatic sync: On or Off. Off, exiting a game with IGR goes straight to the program of "After IGR,
  open", without showing SD2Cloud. It also stays off while no Google account is connected.
- SAS package: installs SD2Cloud's folder on the memory card in use (what OPL runs on IGR, and a shortcut
  for the PS2 browser; see AUTOMATIC SYNC). Once it is installed, the same option reinstalls it (or updates
  it) or uninstalls it.
- IGR path: the program started after the IGR sync. The choices are Automatic (the OPL found on the
  microSD), the programs in the APPS folder and the PS2 browser.
- Language and Backups kept per card (3, 5, 10, 20, 50 or no limit): how many backups of each card stay on
  Drive; past that, the oldest is removed.
- Backup format: .mcd (the sd2psx's own) or .ps2 (the format PCSX2 uses: take the file out of the backup's zip
  and put it in PCSX2's memory card folder). Either one can be restored, whatever is set.
- Check for updates: looks for a new version right away and, if there is one, offers to update.
- Update channel: Stable (the released versions) or Beta (see UPDATING).
- Google account: disconnects the account (backups on Drive are kept) or connects it again.
- About SD2Cloud: version, credits and licenses.
Changes are saved to sd2cloud.ini, which can also be edited on a PC.

AUTOMATIC SYNC AFTER A GAME (IGR)
1. In SD2Cloud, press START and select "SAS package". SD2Cloud shows how much space it takes on the memory
   card in use (about 240 KB) and how much is free, and asks before installing. It writes the APP_SD2CLOUD
   folder (mc0:/APP_SD2CLOUD). If your sd2psx uses Autoboot, install it while the BootCard is in use.
2. In OPL Settings, set "IGR Path" to: mc0:/APP_SD2CLOUD/IGR.ELF
   If a version up to 1.5 left its helper on the card (mc0:/BOOT/SD2CLOUD-IGR.ELF), that path keeps working:
   installing the package also brings that file up to date.
3. In OPL, also turn on "IGR Bootcard Slot(s)" (MMCE page), set to the sd2psx slot or BOTH: when you exit a
   game, OPL switches back to the BootCard, where the package is installed.
From then on, exiting a game with IGR starts SD2Cloud: it uploads the changed cards, shows "Upload complete"
and returns to OPL. In this mode, cancelling the upload also returns to OPL.
SD2Cloud can be kept in any folder of the sd2psx microSD: every time it is opened, it records where it is
in sd2cloud.ini ("app_path"), and the package starts it from there. After moving it, open it once.
To uninstall the package, select "SAS package" again and choose "Uninstall"; then change "IGR Path" in OPL.
To pause the sync and keep everything installed, set "Automatic sync" to Off in Settings (START): IGR then
goes straight to the program opened after it, without starting SD2Cloud. The same happens while no Google
account is connected.
Without installing anything (USB drive): OPL's IGR can also start a program from a USB drive. If the
APPS/SD2Cloud folder is on one, formatted as FAT32, skip step 1 and set "IGR Path" to:
mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF
Nothing is written to the memory card. For this, OPL loads the USB drivers USBD.IRX and USBHDFSD.IRX from
mc?:/SYS-CONF, where FMCB installs them. The SAS package on the memory card is only needed when SD2Cloud is on
the sd2psx microSD alone, or on a device OPL's IGR cannot read (MX4SIO, HDD).
The program opened after IGR is chosen in Settings (START), from the programs in the APPS folder of the
microSD. If your OPL is on another device, enter its path in sd2cloud.ini, in the [igr] section, starting
with the device: mc?:/ (memory card), mass:/ (USB), mx4sio:/, ata:/ (internal HDD, exFAT) or
hdd0:PARTITION:pfs:/ (internal HDD, APA), for example hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF. SD2Cloud loads only the drivers
of that device, when returning. If the file is not found, it opens the PS2 browser. The "name" line, under
"return", gives that program the name shown on screen.

FILE LOCATIONS
- APPS/SD2Cloud/   the application (updated by SD2Cloud itself; see UPDATING).
- SD2Cloud/        on the microSD root: sd2cloud.ini (the settings; if deleted, SD2Cloud recreates it),
                   sd2cloud.example.ini (what each setting means; SD2Cloud never reads it),
                   sync-error.txt (what happened in the last automatic sync that failed, if any: worth
                   sending along when reporting a problem),
                   state.ini (a record of what has been uploaded) and token.dat (access to your Google
                   account; do not share this file).
- Google Drive:    "PS2 Memory Card Backups/<card folder>/<card> YYYY-MM-DD HHhMM.zip".
                   10 backups per card are kept by default; the oldest is removed only after the new
                   backup has been verified.

UPDATING
SD2Cloud does not look for updates by itself. In the settings (START), "Check for updates" asks GitHub
whether there is a newer version and, if there is one, offers to update. The application downloads the new
files, verifies them against the SHA-256 published on GitHub, replaces the previous files and starts the new
version. Your settings are kept.
Then update the SAS package as well: the settings show it as "Outdated" when what the memory card has is
different.
To update manually, copy only the APPS/SD2Cloud folder from the new .zip file (or do not replace
sd2cloud.ini when prompted).
The beta channel: "Update channel" in the settings chooses where the updates come from. Stable is the released
versions. Beta is a build made automatically from every change to the project, before it is tested the way a
version is: it has what is new sooner, and it may have bugs. Choosing a channel looks for its update right
away; going from Beta back to Stable offers the stable version in place of the beta installed. "About
SD2Cloud" shows, after the version, the change the program was built from.

RESTORING
On the PS2: select the card, press TRIANGLE and choose "Restore a backup"; choose the backup and press X.
If the card has changed since its last backup, SD2Cloud syncs it first, so no data is lost.
The backup is then downloaded, verified, written over the card on the microSD and verified again.
The card currently in use by the sd2psx cannot be restored, because the sd2psx keeps a copy of it in its
own memory: switch to another card on the sd2psx first.
On a PC: download the .zip file from Drive and extract the .mcd file. Copy it to the microSD with the PS2
turned off, or open it in mymc+ to export the data of a single game.

PRIVACY
Your data goes from the PS2 directly to your own Google Drive. No other server is involved and no
information is collected. Access can be revoked at any time at myaccount.google.com/permissions.
Besides Google, SD2Cloud only communicates with GitHub, and only when you select "Check for updates": to
see whether there is a newer version and to download it if you choose to update.

CREDITS
The design is inspired by the PlayStation BB Navigator and the PS2 memory card browser; all images in
SD2Cloud are generated by the application itself. Font: Varela Round (SIL Open Font License).
Developed by MrRexD.

Third-party software: see THIRD-PARTY-NOTICES.txt.
