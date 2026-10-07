# SD2Cloud v1.5

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.5
- **Memory card files.** The Files tab opens `.mcd`, a MemCard PRO2's `.mc2`, `.ps2`, OPL's virtual memory cards (`.bin`) and the `.zip` that "Copy to a device" writes. The card inside is shown like any other, its saved data can be copied to your cards, and SQUARE installs it on the microSD: as a new card (numbered, a game's, or one more BootCard channel) or in place of an existing one. A folder that already has all its channels can get one more.
- **All saves.** The Games tab starts with a screen that shows the saved data of every game card together.
- **Faster, and with a progress screen.** Importing a `.psu`, and copying or moving saved data between cards, now shows the save's icon, a progress bar and a way to cancel (the card is left as it was). It is also about four times faster: a 1.6 MB save that took close to 30 seconds on an sd2psx now takes 6. Deleting, exporting and reading icons got faster too.
- **Start other programs.** X on an ELF in the Files tab runs it, and "Exit to" has a new "Run an ELF..." item. Saved data that is an application (a folder with a `title.cfg`, as the Save Application System keeps them) gets "Start application": SD2Cloud switches the sd2psx to that card and starts it.
- **The sd2psx taken out.** If the device is removed while SD2Cloud is open (to change its microSD, for example), SD2Cloud says so and, once it is back, reads everything again from the microSD that is in it.
- **Automatic sync.** Fixed the "Unable to obtain an IP address from the router" failures after a game. A sync that fails now leaves a note in `SD2Cloud/sync-error.txt`.
- **MemCard PRO2 (experimental).** The device is recognized (its `.mc2` cards, in `/PS2`), and switching cards from the app is offered as a preview. SD2Cloud has not been tried on one yet.
- **Files tab.** SQUARE on a `.psu` installs it without opening it first; when a file of the same name is already there, SQUARE keeps both.
- Saved data without an icon shows the PS2 browser's blue cube; a game's folder is marked with a small arrow.
- **New download: `APP_SD2CLOUD.psu`**, SD2Cloud as a Save Application System (SAS) package: the `APP_SD2CLOUD` folder, with its own 3D icon for the PS2 browser.

To update, use Settings > Check for updates, or copy the new files over the old ones.

SD2Cloud runs on the PS2 itself and works with the memory cards on the microSD of sd2psx-family devices (sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No PC required.

### Cloud sync
- Syncs your memory cards with Google Drive, on demand or automatically every time you exit a game with IGR. Only the cards that changed are uploaded.
- Backups are the sd2psx's own `.mcd` or, if you choose so in the settings, a `.ps2`, the format PCSX2 uses.
- One-time Google account link with a code or a QR code; SD2Cloud can only access the files it creates.
- Keeps 10 backups per card by default; the oldest is removed only after the new backup is verified (SHA-256).
- Restores any backup from Drive, verified before and after writing; a changed card is synced first, so nothing is lost.
- The first setup offers to turn on automatic sync: it says how much space the IGR helper takes on the memory card, installs it once you agree and shows the path to set in OPL. The helper can be uninstalled in the settings, and SD2Cloud can be kept in any folder of the microSD.

### Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between the cards on the microSD (the destination's free space is shown), and uploads a single item to Drive as a .psu file.
- Copies a whole card, as a `.zip` with the `.mcd` or the `.ps2` inside, to a folder of the microSD or of a USB drive.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards, in folders named after each game, and boot cards.
- Marks the card the sd2psx is using and can make it take another one.
- Imports `.psu` files into any card and exports any item of a card as a `.psu`, from and to the microSD or a USB drive (the Files tab).

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- For OPL builds that do not list the apps on the sd2psx microSD: copy the `APPS/SD2Cloud` folder to a USB drive or an MX4SIO card as well and open SD2Cloud from there. On a USB drive, OPL's IGR can run the helper from that folder, with nothing installed on the memory card.
- Settings on START: sync all cards, automatic sync on or off, IGR helper, the program to open after IGR, language, backups per card, backup format, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the microSD.
- Returns to OPL on the microSD, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English and Portuguese.

**Installation:** extract `SD2Cloud-v1.5.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.5**
- Arquivos de cartão: a aba Arquivos abre `.mcd`, `.mc2`, `.ps2`, os cartões virtuais do OPL (`.bin`) e o `.zip` do "Copiar para dispositivo"; dá para copiar os dados salvos de dentro e instalar o cartão no microSD (QUADRADO).
- "Todos os saves": uma tela que junta os dados salvos de todos os cartões de jogo.
- Importar, copiar e mover dados salvos: tela com ícone, barra de progresso e cancelar, e cerca de 4 vezes mais rápido (1,6 MB: de quase 30 s para 6 s).
- Iniciar outros programas: X sobre um ELF na aba Arquivos, "Executar um ELF..." no "Sair para" e "Iniciar aplicativo" nos apps guardados em cartões (formato SAS).
- sd2psx removido com o app aberto: o SD2Cloud avisa e, quando ele volta, lê tudo de novo do microSD.
- Sincronização automática: corrigida a falha "Não foi possível obter um endereço IP"; uma falha deixa um registro em `SD2Cloud/sync-error.txt`.
- MemCard PRO2 reconhecido (experimental), com a troca de cartão pelo app em preview.
- Novo download: `APP_SD2CLOUD.psu`, o SD2Cloud como pacote SAS, com ícone 3D próprio.

**Instalação:** extraia o `SD2Cloud-v1.5.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
