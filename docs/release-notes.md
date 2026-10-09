# SD2Cloud v1.6.1

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.6.1
- **Options on a card's saves.** TRIANGLE, with a card's saves on screen, opens the options: "Sync now" (it was on SQUARE), and "Sort by": the date, descending or ascending, or the name of the game. X changes a value, and the saves are listed the new way when the options are closed.
- **Duplicates in "All saves".** A save that more than one game card has (the same files, byte for byte, as a template put into every card) is shown once. The options there show them all again. Saves of the same name with anything different in them are always shown, whatever their dates.
- **Pasting to a folder.** The list of cards where saves are selected for a copy has the Files tab again: X opens the device's folders and START pastes the selected saves into the one shown, as `.psu` files.
- **MemCard PRO2.** A microSD that was used in an sd2psx and in a MemCard PRO2 has the cards of both, and was taken for an sd2psx's. SD2Cloud now asks, once, which device it is in, and "Device", in the settings, changes the answer.
- A card that cannot be read when the cards are checked is tried again.
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
- Copies (several saves at once), moves and deletes saved data between the cards on the MMCE, and uploads a single item to Drive as a .psu file.
- Copies a whole card, as a `.zip` with the `.mcd` or the `.ps2` inside, to a folder of the MMCE or of a USB drive.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards, in folders named after each game, and boot cards.
- Marks the card the sd2psx is using and can make it take another one.
- Imports `.psu` files into any card and exports any item of a card as a `.psu`, from and to the MMCE or a USB drive (the Files tab).
- Keeps templates: sets of saves to put into cards (Tools, on SELECT).
- Shows every game card's saves on one screen, sorted by date or by name.

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- Settings on START: sync all cards, automatic sync on or off, the SAS package, the IGR path, language, backups per card, backup format, update channel, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the MMCE.
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English and Portuguese.

**Installation:** extract `SD2Cloud-v1.6.1.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.6.1**
- Opções na tela de saves de um cartão (TRIÂNGULO): "Sincronizar agora" e "Ordenar por" data decrescente, data crescente ou nome do jogo.
- "Todos os saves": um save repetido em vários cartões (os mesmos arquivos, byte a byte) aparece uma vez só; as opções exibem todos de novo. Saves de mesmo nome com conteúdo diferente sempre aparecem.
- A aba Arquivos voltou à lista onde os saves são selecionados para cópia: START cola os selecionados na pasta exibida, como `.psu`.
- MemCard PRO2: com um microSD usado nos dois dispositivos, o SD2Cloud pergunta em qual deles o microSD está ("Dispositivo", nos Ajustes, altera).
- Um cartão que não é lido na verificação é tentado de novo.
**Instalação:** extraia o `SD2Cloud-v1.6.1.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
