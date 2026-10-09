# SD2Cloud v1.6

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.6
- **Templates.** Tools (SELECT) > Templates: sets of saves kept on the MMCE, made by selecting saves on your cards, to put into other cards (the network settings every game's card should have, for example). A template can be applied to all game cards, to the cards you select, or only to update the saves the cards already have. One template can be the default: SD2Cloud tells when a game card lacks it, and "Auto-apply" puts it into a game's card when the game is left, with the automatic sync. "Linked games" gives a game templates of its own.
- **Copy several saves at once.** "Copy" on a save's page now selects it: X selects others on the same card, O leaves the card to select saves on other cards too, and START pastes them all into the card under the cursor, or into the card that is open. Pressed on the card they came from, START asks where they go: another card, a new card of their game, or a folder of the MMCE or of a USB drive (as `.psu` files).
- **SD2Cloud outside the sd2psx.** The program can also be installed on a USB drive, an MX4SIO or the internal HDD (exFAT or APA): the shortcut and the IGR helper on the memory card open it from there, and its settings and data stay on the MMCE. Tested on a console with a USB drive and an MX4SIO; the internal HDD, and updating the program in place on these devices, have not been tested on a console yet.
- **SAS package.** It finds SD2Cloud in any folder of `APPS` (a renamed folder no longer breaks the shortcut and IGR), and it is updated together with the program.
- **Sync per card.** A card's options (TRIANGLE) have "Disable sync" and "Enable sync", for any card. While several cards are sent, SQUARE skips the one on its way, this time or for good.
- **Cards grouped by Game2Folder.ini.** A folder shared by several games shows their names instead of its code. A card the sd2psx no longer opens for its game is marked, and its options can move its saves to the card that is used.
- **MMCE.** What the screens called "microSD" is now "MMCE", as the PS2 and its launchers name it.
- The texts on screen were reviewed: shorter, and in the usual words.
- The screen of a card being worked on shows a 3D sd2psx, now also while a card is restored, installed, copied to a device or created.
- A large memory card file (32 MB or more) is installed in one pass, and SD2Cloud warns before it replaces a card.
- Fixes: whether a card is in use is checked on the slot before it is written, and "New card" never replaces a file that is already there.

To update, use Settings > Check for updates, or copy the new files over the old ones.

SD2Cloud runs on the PS2 itself and works with the memory cards on the microSD of sd2psx-family devices (the MMCE; sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No PC required.

### Cloud sync
- Syncs your memory cards with Google Drive, on demand or automatically every time you exit a game with IGR. Only the cards that changed are uploaded.
- Backups are the sd2psx's own `.mcd` or, if you choose so in the settings, a `.ps2`, the format PCSX2 uses.
- One-time Google account link with a code or a QR code; SD2Cloud can only access the files it creates.
- Keeps 10 backups per card by default; the oldest is removed only after the new backup is verified (SHA-256).
- Restores any backup from Drive, verified before and after writing; a changed card is synced first, so nothing is lost.
- The first setup offers to turn on automatic sync: it says how much space the SAS package takes on the memory card, installs it once you agree and shows the path to set in OPL. The package can be uninstalled in the settings.

### Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between the cards on the MMCE (the destination's free space is shown), and uploads a single item to Drive as a .psu file.
- Copies a whole card, as a `.zip` with the `.mcd` or the `.ps2` inside, to a folder of the MMCE or of a USB drive.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards, in folders named after each game, and boot cards.
- Marks the card the sd2psx is using and can make it take another one.
- Imports `.psu` files into any card and exports any item of a card as a `.psu`, from and to the MMCE or a USB drive (the Files tab).
- Keeps templates: sets of saves to put into cards (Tools, on SELECT).

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- Settings on START: sync all cards, automatic sync on or off, the SAS package, the IGR path, language, backups per card, backup format, update channel, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the MMCE.
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English and Portuguese.

**Installation:** extract `SD2Cloud-v1.6.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.6**
- Templates (Ferramentas, no SELECT): conjuntos de saves guardados no MMCE para aplicar aos cartões, com template padrão, aplicação automática ao sair do jogo e jogos vinculados.
- Copiar vários saves de uma vez: "Copiar" seleciona o save, X seleciona outros (em mais de um cartão) e START cola todos no cartão sob o cursor ou no cartão aberto (no cartão de origem, pergunta o destino: outro cartão, um cartão novo ou uma pasta).
- O programa pode ficar em um pendrive USB, em um MX4SIO ou no HD interno; o atalho e o IGR o abrem de lá. Testado em console com pendrive e MX4SIO; o HD interno e a atualização do programa nesses dispositivos ainda não.
- Pacote SAS: encontra o SD2Cloud em qualquer pasta de `APPS` e é atualizado junto com o programa.
- Sincronização por cartão: "Desativar sincronização" nas opções do cartão; QUADRADO pula um cartão durante o envio.
- Pastas de grupo do Game2Folder.ini com o nome dos jogos; os saves de um cartão que o sd2psx não usa mais podem ser movidos para o que ele usa.
- "MMCE" no lugar de "microSD" nas telas, e textos revisados ("Configurações" agora é "Ajustes").
- Arquivo de cartão grande (32 MB ou mais) instalado em uma só passada, com aviso antes de substituir um cartão.

**Instalação:** extraia o `SD2Cloud-v1.6.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
