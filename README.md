<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/dark.png">
    <source media="(prefers-color-scheme: light)" srcset="docs/logo/light.png">
    <img src="docs/logo/light.png" width="400" alt="SD2Cloud">
  </picture>
</h1>

**Manage and back up your PS2 memory cards, right on the console.**

[![Downloads](https://img.shields.io/github/downloads/oMrRexD/sd2cloud/total?style=for-the-badge&logo=github)](https://github.com/oMrRexD/sd2cloud/releases/latest)
> [!WARNING]
> SD2Cloud writes to your memory cards whenever it copies, moves, deletes, imports or restores saves, and whenever
> it creates, resizes or deletes cards. Make a backup first: sync your cards to Google Drive, or copy the
> `MemoryCards` folder from the microSD to a PC.

SD2Cloud is an app that runs on the PS2 itself, with no PC needed. It backs up your memory cards to Google Drive
and lets you manage your saves right on the console.

It works with:
- **sd2psx-family devices** (sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero) running the
  [sd2psXtd](https://github.com/sd2psXtd/firmware) firmware. On the PS2, these devices (and their microSD) are known
  as **MMCE**, the name this page uses for them.
- **MemCard PRO2**.
- **Regular Memory Cards**: the physical cards in the console's slots.
- **OPL's virtual memory cards (VMCs)** and other memory card files kept on a USB drive, an MX4SIO or the internal
  HDD.

**English** · [Português](README.pt-BR.md) · [Español](README.es.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [日本語](README.ja.md)

<p>
  <img src="docs/screens/menu-en.png" width="49%" alt="Main screen">
  <img src="docs/screens/saves-en.png" width="49%" alt="Saved data of a memory card">
</p>
<p>
  <img src="docs/screens/save-en.png" width="49%" alt="The page of a game's saved data">
  <img src="docs/screens/upload-en.png" width="49%" alt="A game's save being uploaded to Google Drive">
</p>
<p>
  <img src="docs/screens/files-en.png" width="49%" alt="The Files tab: a folder of the MMCE with .psu files">
  <img src="docs/screens/psu-en.png" width="49%" alt="A .psu file about to be imported into a memory card">
</p>

## Features

**Backup to Google Drive**
- Syncs your memory cards with your Google Drive whenever you want, or automatically every time you quit a game
  with IGR (In-Game Reset, OPL's button combination for leaving a game). Only the cards that changed are uploaded.
- Keeps a history of backups for each card (10 by default), and you can restore any of them. The card is checked
  before and after it is written. If a card changed since its last backup, SD2Cloud backs it up before restoring
  another one over it, so nothing is lost.
- Uploads each card as an `.mcd` file, the sd2psx's own format. If you prefer, choose `.ps2` in the settings: it is
  the format PCSX2 uses, so a card taken out of its backup's zip is ready for the emulator. Both can be restored.
- **Backup by save** (optional): also uploads each save that changed as a file of its own, and keeps its older
  versions. A save's page lists its **Versions on Drive**, and you can restore any of them.
- On Drive, the backups are sorted into folders by device, game and card, and each card has a Saves folder. You
  can browse them from the **Files** tab: open a save's page, open a card's backup as if it were a card, install
  it, or download or delete any file.
- You connect your Google account only once, with a code or a QR code. SD2Cloud can only see the files it creates
  in your Drive.

**Memory card manager**
- Shows each card's saves the way the PS2 browser does, with the animated 3D icons.
- Copies, moves and deletes saves between cards, several at once, even from more than one card. It can also
  upload a single save to Google Drive as a `.psu` file (the usual file format for one PS2 save).
- **The Library** keeps saves outside the cards, each one in a file of its own, sorted by game, with no size limit.
  It is useful when a Memory Card is full, or for a save you want off a card without deleting it. It can be kept on
  Drive too.
- **Card tools** create, resize, rename, check and delete cards:
  - a new card can be a numbered card, a card in a folder of your own, or a card file on any device. Sizes go from
    1 to 64 MB on an sd2psx and from 8 to 128 MB on a MemCard PRO2, and the recommended size is marked;
  - **Convert to Game ID** splits a card into one card per game: Game ID cards are the cards the sd2psx switches
    to by itself when a game starts. All of it happens on one screen, which shows the time left;
  - before any change that could lose data, SD2Cloud keeps a **safety copy** of the card, which you can restore
    from Tools.
- The cards are sorted into tabs: numbered cards and your own folders, game cards (Game ID) in folders named after
  each game, boot cards, **OPL's VMCs** and the Library. The VMCs tab shows which games use each VMC, and also has
  the VMC folders of the HDD, an MX4SIO or a USB drive.
- **All saves**, in the Games tab, shows the saves of every game card on one screen, as if they were a single card.
- Shows which card the sd2psx is using, and can switch it to another card. To change the card that is in use,
  SD2Cloud asks first, moves the sd2psx to another card while it works, and then moves it back.
- The **Files** tab browses the MMCE, a USB drive, the internal HDD and an MX4SIO. The HDD can be exFAT, FAT32 or
  APA, including disks set up by the PSBBN installer, which have both. From there you can:
  - import and export `.psu` files and saves in folder format;
  - open memory card files (`.mcd`, `.mc2`, `.ps2`, OPL's `.bin` or a `.zip`) and install them as cards, or as a
    card file on any device, in the format you choose;
  - copy a whole card to a folder as a `.zip`.
- Manages the regular **Memory Cards** in the console's slots: their saves, and the whole card uploaded to Drive
  and restored from there.
- If the microSD has cards from both an sd2psx and a MemCard PRO2, **L2 and R2** switch between the two sets of
  cards, so you can copy saves from one to the other.
- **Templates**: a template is a set of saves you pick from your cards, to put into any card in one go. One of
  them can be the **default** template, the one every game card should have: it can be put into all of them at
  once, and when a new game's card lacks it, SD2Cloud offers it when it opens. With automatic sync, the card a
  game used gets its templates by itself when you quit the game. Templates are in **Tools** (SELECT).

**Also**
- Works **without an sd2psx**: run it from a USB drive, an MX4SIO or the internal HDD, and it keeps its data
  there. It manages the Memory Cards in both slots (backing them up save by save), OPL's VMCs and card files.
- You can take the sd2psx out and put it back while SD2Cloud is open: the screens follow.
- Asks before anything that uploads, writes or deletes data.
- The settings are on the console (START), in pages, and are saved to `sd2cloud.ini`, which you can also edit on
  a PC.
- When you leave, it can go back to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or
  APA), loading only the drivers of that device.
- Starts other programs: any ELF in a device's folders (X on it in **Files**, or "Run an ELF..." when leaving),
  and apps kept on a memory card in the Save Application System format ("Start application" on the app's page).
- Updates itself from this repository's releases, and checks every download against the SHA-256 GitHub
  publishes. There are two channels, chosen in the settings:
  - **Stable**: the released versions;
  - **Beta**: a build made automatically from every change, before the tests a release goes through (the
    [beta pre-release](../../releases/tag/beta)).
- Interface inspired by the PlayStation BB Navigator.
- In English, Portuguese, Spanish, Russian, Ukrainian and Japanese. Translations are welcome: see
  [Translating](CONTRIBUTING.md#translating). Saves of Japanese games show their titles in Japanese.

## Requirements

- One of these:
  - an sd2psx-family device with the sd2psXtd firmware (MMCE support);
  - a MemCard PRO2. SD2Cloud reads and writes its `.mc2` cards, in the `/PS2` folder. On the PRO2, switching
    cards from the app ("Insert this card") is still a preview feature, and SD2Cloud never switches cards on its
    own: to change the card that is in use, switch to another one first;
  - neither: SD2Cloud on a USB drive, an MX4SIO or the internal HDD, with regular Memory Cards in the slots.
- A way to open SD2Cloud: OPL's Apps tab, wLaunchELF or any other program that runs ELF files. Use an OPL with
  MMCE support if you can, such as [RiptOPL](https://github.com/NathanNeurotic/Open-PS2-Loader). An OPL without it
  does not list the apps on the MMCE, and has no **IGR Bootcard Slot(s)** option. So, with Game ID cards, quitting
  a game with IGR leaves the sd2psx on the game's card, the PS2 goes back to its browser instead of opening
  SD2Cloud, and the automatic sync does not happen.
- For the automatic sync after a game: OPL, which opens SD2Cloud through IGR.
- For the cloud features: a Google account, and a PS2 with a network adapter (built into Slim models) connected to
  your router by cable.

## Installation

1. Download `SD2Cloud-vX.Y.zip` from the [latest release](../../releases/latest).
2. Extract it to the root of the MMCE. This adds:
   - `APPS/SD2Cloud`: the app;
   - `ART`: the cover and the logo OPL shows for SD2Cloud in its Apps tab;
   - `Extras`: the same art with its line in Portuguese (to copy over the two files in `ART`), the SAS package
     (see below) and `sd2cloud.example.ini`, which explains every setting.

   The first time it runs, SD2Cloud creates its settings file, `SD2Cloud/sd2cloud.ini`. The zip doesn't have
   that file, so extracting a new version over an old one keeps your settings.
3. Open SD2Cloud from OPL's Apps tab, or run `APPS/SD2Cloud/SD2CLOUD.ELF` from any program that runs ELF files
   (wLaunchELF, for example). The folder doesn't have to stay in `APPS`.
4. When asked, connect your Google account (or later, in Settings > Google Drive): go to google.com/device on your
   phone or computer, or scan the QR code, and enter the code shown on the TV.
5. SD2Cloud then offers to turn on automatic sync and to sync all your cards.

**Without an sd2psx**, extract the zip to a USB drive (FAT32 or exFAT), an MX4SIO or the internal HDD instead, and
open `APPS/SD2Cloud/SD2CLOUD.ELF` from there. SD2Cloud keeps its settings and data on that device. What it backs
up are the Memory Cards in the slots, save by save (backup by save comes turned on), and the first sync already
offers them. If that device has an sd2psx's folders on it, they are just files to SD2Cloud: open them in the
**Files** tab.

**With an sd2psx, the app on another device.** SD2Cloud can also live on a USB drive, an MX4SIO or the internal
HDD (exFAT or APA). Put the `APPS/SD2Cloud` folder there and open SD2Cloud from there once. It then records where
it is, keeps on the MMCE (in `SD2Cloud/drivers`) the drivers the SAS package needs to start it from that device,
and updates itself in that place. Its settings and data stay on the MMCE.

**The SAS package, on the memory card.** When you turn on automatic sync, SD2Cloud puts its SAS (Save Application
System) package on the memory card in use. It is the `APP_SD2CLOUD` folder, about 240 KB, with:
- the program OPL runs on IGR;
- a shortcut that opens SD2Cloud wherever it is, with its own 3D icon in the PS2 browser;
- without an sd2psx, also the drivers of the device SD2Cloud is on (about 90 KB more).

SD2Cloud itself stays where it is. The zip also has the package as `Extras/APP_SD2CLOUD.psu`, to import into
another card by hand (SD2Cloud's own **Files** tab does it). After that, open SD2Cloud once from where it is, and
the shortcut finds it.

**Coming from a version before 2.0?** Version 2.0 sorts the backups on Drive in a new way: by device, game and
card. The first time it opens, SD2Cloud offers to move your files into the new folders, with nothing uploaded
again. "Not now" keeps the old layout, and Settings > Google Drive offers the move again.

The [README.txt](package/README.txt) in the zip explains every screen in detail.

## Controls

| Screen | Buttons |
|---|---|
| Main screen | Up/Down: choose a card · Left/Right or L1/R1: change tab · L2/R2: the other device's cards (on a microSD with both) · X: open the card · TRIANGLE: card options (sync now, restore a backup, copy to a device, insert into the sd2psx, rename, change size, convert to Game ID, check card, delete card) · SELECT: Tools · START: Settings · O: exit |
| Inside a card | X: open a save · TRIANGLE: options (sync this card; sort the saves by name or by date; convert to Game ID; in All saves, show or hide duplicates and export them all to a card; on a Memory Card, upload it to Drive or restore a backup) · O: back |
| A save's page | Copy · Move · Delete · Upload to Drive · Versions on Drive |
| After Copy | The save stays selected, and you can select more: X selects or unselects a save (hold X and move to do several in a row) · O: go back to the list of cards, to select saves on other cards too · TRIANGLE: cancel · START: paste them all into the open card, or into the card under the cursor in the list. In the list's Games tab, the folder of their game comes first, highlighted, with "New card" inside. The Library tab keeps them outside the cards. In the Files tab, X opens a device's folders, and START pastes the saves there as `.psu` files, or into the memory card file under the cursor. START on a Memory Card pastes them into it |
| Files tab | X: open a device, a folder, a `.psu` file or a memory card file · SQUARE: install the selected `.psu` or save folder into a card, or install the selected memory card file as an MMCE card or as a file on a device (in that device's Files tab) · Left/Right: page up or down · O: back |
| Files tab > Google Drive | X: open a folder, a save's `.psu` (its page: import into a card, download, delete from Drive) or a card's backup (opens as a card) · SQUARE: install a card's backup (as from the card), or download any other file · TRIANGLE: delete it (it goes to Drive's trash) · O: back |
| Tools (SELECT) | Templates (X: create a template or open one; TRIANGLE: set as default, add saves, apply template, template games, rename, delete) · Safety copies (X: restore one, or delete it) |
| Settings (START) | Sync all cards · Google Drive (account, backups kept, format, backup by save, Library on Drive) · Automatic sync (on or off, SAS package, IGR path) · Cards (device, new card size, OPL VMC folders) · Updates · Language · About |
| During a sync | O: cancel · SQUARE: skip the card being uploaded · SELECT: the list of the cards in this sync and how each one went (SELECT again goes back). On that list, SQUARE skips a card (press again to undo), and X offers to skip it, mark it as resolved (automatic sync leaves it alone until it changes again) or turn off its sync |

## Automatic sync after a game (IGR)

1. In SD2Cloud, open Settings (START) > Automatic sync and select **SAS package** (it is also offered right after
   you connect your account). SD2Cloud shows how much space it needs (about 240 KB) and asks before writing the
   `APP_SD2CLOUD` folder to the memory card in use. If you use Autoboot, install it while the BootCard is the card
   in use.
2. In OPL's settings, set **IGR Path** to `mc0:/APP_SD2CLOUD/IGR.ELF` (`mc1:` if the sd2psx is in slot 2;
   SD2Cloud shows the right path when it installs the package). If version 1.5 or earlier left its helper on the
   card, at `mc0:/BOOT/SD2CLOUD-IGR.ELF`, that path still works: installing the package also updates that file.
3. In OPL, also turn on **IGR Bootcard Slot(s)** (MMCE page), set to the sd2psx's slot or to BOTH. When you quit a
   game, OPL then switches back to the BootCard, where the package is.

From then on, quitting a game with IGR opens SD2Cloud, which uploads the cards that changed and goes back to OPL.
The same option in Settings reinstalls or removes the package.

SD2Cloud can be in any folder, on the MMCE or on another device. Each time it opens, it records where it is (in
`sd2cloud.ini` on the MMCE; without an sd2psx, in the package itself, along with that device's drivers), and the
package starts it from there. It even finds a folder of `APPS` that was renamed.

To pause the sync without uninstalling anything, set **Automatic sync** to Off in Settings. IGR then skips
SD2Cloud and goes straight to the program that would open after it. The same happens while no Google account is
connected.

**Without installing anything (USB drive).** OPL's IGR can also start a program from a USB drive. If the
`APPS/SD2Cloud` folder is on a USB drive formatted as FAT32, skip step 1 and set **IGR Path** to
`mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF`. Nothing is written to the memory card. For this, OPL loads the USB drivers
`USBD.IRX` and `USBHDFSD.IRX` from `mc?:/SYS-CONF`, where FMCB installs them. With the SAS package on the memory
card you need none of this, wherever SD2Cloud is: the package starts it from the MMCE, a USB drive, an MX4SIO or
the HDD.

If your OPL is not on the MMCE, put its path in the `[igr]` section of `sd2cloud.ini`, starting with the device:
`mc?:/` (memory card), `mass:/` (USB), `mx4sio:/`, `ata:/` (internal HDD, exFAT) or `hdd0:PARTITION:pfs:/`
(internal HDD, APA). For example: `hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF`.

## Privacy

Your memory cards go from the PS2 straight to your own Google Drive. No other server is involved, and nothing is
collected. SD2Cloud uses the `drive.file` scope, so it can only see the files it creates. You can revoke its
access at any time at [myaccount.google.com/permissions](https://myaccount.google.com/permissions). Besides
Google, SD2Cloud only contacts GitHub, and only when you select Check for updates in the settings.

## Building

SD2Cloud is written in C with the [ps2dev](https://github.com/ps2dev) toolchain (ps2sdk, ps2sdk-ports, gsKit).

1. `tools/build_ports.sh` builds wolfSSL and curl with 4096-bit RSA support into `ports4096/` (Google's HTTPS needs it).
2. `tools/build_mmceman.sh` builds mmceman, the sd2psx driver, into `third_party/mmceman/` (the one that comes with
   the SDK can hang the sd2psx during long transfers).
3. Create an OAuth client of type "TVs and Limited Input devices" in the Google Cloud Console, with the Drive API
   enabled, and generate `src/credentials.h`: `python tools/make_credentials.py client_secret.json src/credentials.h`.
4. `make` builds `dist/SD2CLOUD.ELF` and, in `igr/`, the two small programs of the SAS package (the IGR helper and
   the shortcut); `make DEBUG=1` builds a debug version for PCSX2.
5. `python tools/make_release.py` packages the release in `dist/`, with the texts in `package/`.

`make -C tests` runs the regression tests, with a PC's own compiler and no PS2 toolchain: see
[tests/README.md](tests/README.md). [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) is the map of the code.

## Reporting bugs and contributing

Found a problem? Open an [issue](../../issues/new/choose) and fill in the form: the SD2Cloud version, the console,
the sd2psx device and its firmware, how SD2Cloud was opened, and the steps that lead to the problem. Ideas and
suggestions are welcome there too. Issues can be written in English or in Portuguese.

Pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) explains how to build, the conventions of the code
and what to check before sending one.

## Credits and license

SD2Cloud is developed by MrRexD and distributed under the [GNU General Public License v3](LICENSE).

The design is inspired by the PlayStation BB Navigator. SD2Cloud uses the
[Varela Round](https://github.com/alefalefalef/Varela-Round-Hebrew) font, with the Cyrillic letters of
[Nunito](https://github.com/googlefonts/nunito) and the Japanese characters of
[M PLUS Rounded 1c](https://github.com/coz-m/MPLUS_FONTS) (all three under the SIL Open Font License), and the
libraries listed in [THIRD-PARTY-NOTICES.txt](package/THIRD-PARTY-NOTICES.txt).

PlayStation is a registered trademark of Sony Interactive Entertainment. SD2Cloud is not affiliated with or
endorsed by Sony or Google.
