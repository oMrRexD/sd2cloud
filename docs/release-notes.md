# SD2Cloud v1.4

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.4
- The card the sd2psx is using is marked on the list, and a card's options (TRIANGLE) have a new item, "Insert into sd2psx", which makes the sd2psx take that card. It takes a BootCard only with Autoboot turned on in its settings, and a game's card only with Game ID.
- A card in use by the sd2psx can now be changed (copying, moving, deleting, importing): SD2Cloud asks, switches the sd2psx to another card, makes the change and switches back.
- Games tab: the cards come in folders named after each game, as the sd2psx names it. X opens a folder, O goes back.
- The saved data of a card is listed newest first, as in the PS2 browser.
- Copy to a device now writes the card inside a `.zip`, which takes less time than the whole file. To put such a copy back on the microSD, extract it on a PC for now.
- Save icons that did not show (some compressed and some plain textures) now do.
- Exit to offers "Return to Browser" (it was "PS2 menu").

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

**Installation:** extract `SD2Cloud-v1.4.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.4**
- O cartão em uso no sd2psx aparece marcado na lista, e as opções de um cartão (TRIÂNGULO) ganharam "Inserir no sd2psx", que faz o sd2psx assumir esse cartão.
- Agora é possível alterar o cartão em uso no sd2psx: o SD2Cloud pergunta, troca o sd2psx para outro cartão, faz a alteração e volta.
- Aba Jogos: os cartões ficam em pastas com o nome de cada jogo.
- Os dados salvos de um cartão aparecem do mais novo ao mais antigo, como no browser do PS2.
- "Copiar para dispositivo" grava o cartão dentro de um `.zip`, o que leva menos tempo. Para devolver essa cópia ao microSD, por enquanto é preciso extrair o `.zip` no PC.
- Ícones de saves que não apareciam agora aparecem.
- "Sair para" oferece "Voltar ao Browser".

**Instalação:** extraia o `SD2Cloud-v1.4.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
