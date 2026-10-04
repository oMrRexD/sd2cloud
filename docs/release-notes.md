# SD2Cloud v1.1

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.1
- Backup format: in the settings (or `format` in `sd2cloud.ini`), choose whether each card goes to Drive as the sd2psx's own `.mcd` or as a `.ps2`, the format PCSX2 uses: take the `.ps2` out of the backup's zip and put it in PCSX2's memory card folder. Backups in either format can be restored, whatever is set.
- From 1.0.1: starting a sync from the saved data screen of a memory card with many saves no longer leaves the network without an address and freezes SD2Cloud.

To update, use Settings > Check for updates, or copy the new files over the old ones.

SD2Cloud runs on the PS2 itself and works with the memory cards on the microSD of sd2psx-family devices (sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No PC required.

### Cloud sync
- Syncs your memory cards with Google Drive, on demand or automatically every time you exit a game with IGR. Only the cards that changed are uploaded.
- One-time Google account link with a code or a QR code; SD2Cloud can only access the files it creates.
- Keeps 10 backups per card by default; the oldest is removed only after the new backup is verified (SHA-256).
- Restores any backup from Drive, verified before and after writing; a changed card is synced first, so nothing is lost.
- The first setup offers to turn on automatic sync: it says how much space the IGR helper takes on the memory card, installs it once you agree and shows the path to set in OPL. The helper can be uninstalled in the settings, and SD2Cloud can be kept in any folder of the microSD.

### Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between the cards on the microSD (the destination's free space is shown), and uploads a single item to Drive as a .psu file.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards and boot cards.

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- For OPL builds that do not list the apps on the sd2psx microSD: copy the `APPS/SD2Cloud` folder to a USB drive or an MX4SIO card as well and open SD2Cloud from there. On a USB drive, OPL's IGR can run the helper from that folder, with nothing installed on the memory card.
- Settings on START: sync all cards, IGR helper, the program to open after IGR, language, backups per card, backup format, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the microSD.
- Returns to OPL on the microSD, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English and Portuguese.

**Installation:** extract `SD2Cloud-v1.1.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**
Novidade da 1.1: nas configurações (ou em `format` no `sd2cloud.ini`) dá para escolher se cada cartão vai ao Drive como `.mcd`, o formato do sd2psx, ou como `.ps2`, o formato usado pelo PCSX2. Os dois formatos podem ser restaurados. Inclui a correção da 1.0.1 (travamento ao sincronizar a partir da tela de dados salvos de um cartão com muitos saves).
Instalação: extraia o `SD2Cloud-v1.1.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
