# SD2Cloud v1.7.1

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.7.1
- **Pasting, without the "Copy to" box.** After Copy, the saves are pasted from the list of cards: START on the card they all came from, or in All saves, goes to that list. In its Games tab, the folder of their game comes first, highlighted, with "New card" in it; a new card made for them gets the game's templates too. START on a memory card file in the Files tab pastes into it.
- **Back where you were.** Answering Circle to the question after a card was picked (importing a .psu, moving a save, pasting) goes back to the same tab, folder and card.
- **The whole Memory Card to Drive.** "Upload to Drive", in the Memory Card's options, sends the whole card as a backup with its own history, in the "Memory Card" folder on Drive. While it is read and sent, the screen shows the PS2's own memory card.
- **Templates.** A template's saves no longer take a card out of the sync: a card that was synced stays synced. A game card's icon is its own game's save, not a template's.
- **Settings.** A long list of cards left out of the sync could be cut when written to `sd2cloud.ini`, breaking the line after it. It is written whole now.
- The README is in Japanese too.

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
- Manages a regular Memory Card in the console's other slot (the Files tab), and uploads it whole to Drive.
- Keeps templates: sets of saves to put into cards (Tools, on SELECT).
- Shows every game card's saves on one screen, sorted by date or by name.

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- Settings on START: sync all cards, automatic sync on or off, the SAS package, the IGR path, language, backups per card, backup format, update channel, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the MMCE.
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English, Portuguese, Spanish, Russian, Ukrainian and Japanese.

**Installation:** extract `SD2Cloud-v1.7.1.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.7.1**
- **Colar sem a janela "Copiar para":** depois de Copiar, os saves são colados pela lista de cartões; START no cartão de onde todos vieram, ou em Todos os saves, leva à lista. Na aba Jogos, a pasta do jogo vem primeiro, em destaque, com "Novo cartão" dentro; o cartão novo criado para eles recebe também os templates do jogo. START em um arquivo de cartão de memória na aba Arquivos cola dentro dele.
- **De volta onde estava:** responder O à pergunta depois de escolher um cartão (importar .psu, mover, colar) volta para a mesma aba, pasta e cartão.
- **Memory Card inteiro no Drive:** "Enviar ao Drive", nas opções do Memory Card, envia o cartão inteiro como um backup com histórico próprio, na pasta "Memory Card" do Drive. Durante a leitura e o envio, a tela mostra o Memory Card do PS2.
- **Templates:** os saves de um template não tiram mais o cartão da sincronização; um cartão sincronizado continua sincronizado. O ícone de um cartão de jogo é o save do próprio jogo, não o de um template.
- **Ajustes:** uma lista longa de cartões fora da sincronização podia ser cortada ao ser gravada no `sd2cloud.ini`, estragando a linha seguinte. Agora é gravada inteira.
- O LEIA-ME do GitHub também está em japonês.
**Instalação:** extraia o `SD2Cloud-v1.7.1.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
