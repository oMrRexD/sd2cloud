# SD2Cloud v1.6.2

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.6.2
- **A PS2 whose clock is wrong.** Most PS2s have a flat clock battery by now, and their clock goes back to 2000. SD2Cloud 1.6.1 could then reach neither Google nor GitHub ("error signaled by ssl ctx callback", #2). It now connects whatever the date; a clock that is ahead is told on the screen. Thanks to @Lithium-ps for the report and the log :)
- **Templates after a game, without the sync.** With the automatic sync off, or no Google account, the templates still go into the new game cards after a game, when "after a game" is on in Tools.
- **Skipping a card.** While cards are sent, SQUARE is answered at once, and SQUARE again skips the card; TRIANGLE turns off its sync. Under the bar, the time left.
- **Cards that can't be read.** A read the device fails for a moment is tried again. A card still not read is read again by the list of cards every few seconds, and X on it reads it again.
- **The SAS package on large cards.** When the sd2psx drops the card in the middle of the writing (seen with a 64 MB BootCard), the file is written again once the card is back; if it still fails, the screen says so, with the error.
- **MemCard PRO2.** On that device, the card turning while a card is sent is a MemCard PRO2.
- The beta channel's builds say "beta" and their commit at the top. An update that isn't installed leaves nothing on the screen. A controller that stops answering is opened again.

To update, use Settings > Check for updates, or copy the new files over the old ones. With the PS2's clock before 2008, version 1.6.1 can't reach GitHub: set the date once to update, or copy the files.

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

**Installation:** extract `SD2Cloud-v1.6.2.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.6.2**
- Relógio do PS2 errado (bateria do relógio gasta, que volta para 2000): o SD2Cloud conecta mesmo assim; com o relógio adiantado, a tela avisa (#2). Obrigado ao @Lithium-ps pelo relato e pelo log :)
- Templates depois do jogo também com a sincronização automática desligada ou sem conta do Google.
- Durante o envio, o QUADRADO responde na hora: QUADRADO de novo pula o cartão e TRIÂNGULO desativa a sincronização dele. Tempo restante embaixo da barra.
- Uma leitura que falha por um instante é tentada de novo; a lista de cartões relê sozinha um cartão não lido, e X nele lê de novo.
- Pacote SAS: se o sd2psx desconectar o cartão durante a gravação (visto num BootCard de 64 MB), o arquivo é gravado de novo quando ele volta; se ainda falhar, a tela diz o erro.
- MemCard PRO2: o cartão que gira no envio é o próprio PRO2.
- A beta mostra "beta" e o commit no topo; uma atualização recusada não deixa nada na tela; um controle que para de responder é reaberto.
Com o relógio do PS2 antes de 2008, a versão 1.6.1 não alcança o GitHub: acerte a data uma vez para atualizar, ou copie os arquivos.
**Instalação:** extraia o `SD2Cloud-v1.6.2.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
