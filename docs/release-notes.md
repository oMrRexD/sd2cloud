# SD2Cloud v1.5.1

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.5.1
- **Update channels.** Settings has "Update channel": Stable, the released versions, or Beta, a build made from every change (the `beta` pre-release), which may have bugs. Going back to Stable offers the stable version again.
- **SAS package.** What automatic sync keeps on the memory card is now the `APP_SD2CLOUD` folder, the Save Application System way: the IGR helper and a shortcut that opens SD2Cloud from the PS2 browser, with its 3D icon (about 240 KB). In OPL, "IGR Path" is `mc0:/APP_SD2CLOUD/IGR.ELF`. A helper installed by an earlier version (`mc0:/BOOT/SD2CLOUD-IGR.ELF`) keeps working. `APP_SD2CLOUD.psu` now comes inside the zip (`Extras/`) and is that folder only: the program stays on the microSD.
- **A new card for a game.** When saved data is copied, moved or imported, the Games tab offers "New card": an empty 8 MB card for that game is made on the spot, as the device itself makes them.
- **IGR path.** "After IGR, open" is now "IGR path", and "Choose an ELF..." takes any program from the folders of the microSD or of a USB drive.
- **MemCard PRO2.** The device is now recognized by its microSD's layout (it was taken for an sd2psx and its cards were not found).
- The card in use follows a change made with the sd2psx's own buttons.
- Saved data with a title in Japanese shows its game's name instead of its code.
- Automatic sync waits longer for the network link: fixes "No network connection" right after a game.
- "About" shows the commit the program was built from.

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

**Installation:** extract `SD2Cloud-v1.5.1.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.5.1**
- Canais de atualização: Estável ou Beta (uma compilação a cada alteração; pode ter erros).
- Pacote SAS: a pasta `APP_SD2CLOUD` no cartão de memória, com o assistente de IGR e um atalho com ícone no browser do PS2. No OPL, "Definir saída do IGR": `mc0:/APP_SD2CLOUD/IGR.ELF`. O `.psu` agora vem dentro do zip (`Extras/`).
- "Novo cartão" ao copiar, mover ou importar dados salvos: cria na hora um cartão de 8 MB para o jogo.
- "Saída do IGR" (antes "Após o IGR, abrir"), com "Escolher um ELF...".
- MemCard PRO2 reconhecido pelo formato do microSD.
- O cartão em uso acompanha a troca feita nos botões do sd2psx.
- Dados salvos com título em japonês mostram o nome do jogo.
- Sincronização automática: espera mais pelo cabo de rede logo depois do jogo.

**Instalação:** extraia o `SD2Cloud-v1.5.1.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
