# SD2Cloud v1.7

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.7
- **Three more languages and Japanese titles.** Spanish, Russian, Ukrainian and Japanese, with their own fonts (Cyrillic letters, Japanese characters). In Japanese, CIRCLE confirms and CROSS goes back, as on a Japanese PS2. The saves of Japanese games show their titles in Japanese. The README is in English, Portuguese, Spanish, Russian and Ukrainian.
- **The Memory Card.** A regular PS2 memory card in the console's other slot is in the Files tab: its saves are shown, copied, moved, deleted and sent to Drive as a card's are, and saves go into it from the sd2psx's cards or from `.psu` files. Thanks to nuno6573 for the idea :)
- **The sync's cards, as a list.** While several cards are sent, SELECT shows the list of them, with what became of each, and goes back. On it, SQUARE skips a card (SQUARE again takes it back) and X offers to skip it, mark it as resolved (the automatic sync leaves it alone until it changes again) or turn its sync off.
- **Copying with card files (VMCs).** Saves of a card file opened in the Files tab go anywhere: back in its folder, START pastes them as `.psu` files; out of the MMCE, they can be pasted into a card, the Memory Card, a folder or another card file. During a copy, X opens a card file: to mark saves of it, or START to paste into it (`.mcd`, `.mc2` and `.bin`).
- **The SAS package in slot 2.** With the sd2psx in slot 2 and another memory card in slot 1, the package went to that other card. It goes to the sd2psx's slot now, and the path for OPL is shown (`mc1:`). Thanks to nuno6573 for the report :)
- **Downloads that break off.** An update (from GitHub) or a backup being restored (from Drive) that breaks off is taken up where it stopped, on a new connection, up to three times. It was seen on a console as "AES-GCM Authentication check fail" (#2). Thanks to @Lithium-ps for the log :)
- **Re-enable warnings**, in a template's options: the game cards you said "Don't warn" about are warned about again.
- **Everything up to date.** The newest toolchain (ps2sdk, ps2sdk-ports, gsKit), wolfSSL 5.9.4, curl 8.22.0 and mmceman, which also fixes reading a memory card in slot 2.
- A card file of 16 MB opened in the Files tab has its memory back; the About box fits the screen again; a save can be copied from a card file to a folder.

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
- Manages a regular Memory Card in the console's other slot (the Files tab).
- Keeps templates: sets of saves to put into cards (Tools, on SELECT).
- Shows every game card's saves on one screen, sorted by date or by name.

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- Settings on START: sync all cards, automatic sync on or off, the SAS package, the IGR path, language, backups per card, backup format, update channel, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the MMCE.
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English, Portuguese, Spanish, Russian, Ukrainian and Japanese.

**Installation:** extract `SD2Cloud-v1.7.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**

**Novidades da 1.7**
- Mais três idiomas: espanhol, russo, ucraniano e japonês (no japonês, CÍRCULO confirma e XIS volta, como num PS2 japonês). Os saves de jogos japoneses mostram o título em japonês. O LEIA-ME do GitHub está em inglês, português, espanhol, russo e ucraniano.
- **Memory Card:** um cartão de memória comum no outro slot do console aparece na aba Arquivos; os saves dele são exibidos, copiados, movidos, eliminados e enviados ao Drive como os de um cartão, e recebem saves dos cartões do sd2psx ou de arquivos `.psu`. Obrigado ao nuno6573 pela ideia :)
- **Lista da sincronização:** com vários cartões sendo enviados, o SELECT mostra a lista deles, com a situação de cada um, e volta. Nela, QUADRADO pula um cartão (de novo: desfaz) e X oferece pular, marcar como resolvido (a sincronização automática o deixa de lado até ele mudar de novo) ou desativar a sincronização dele.
- **Cópia com arquivos de cartão (VMC):** os saves de um VMC aberto na aba Arquivos vão para qualquer lugar: na pasta dele, START cola como `.psu`; saindo do MMCE, podem ir para um cartão, o Memory Card, uma pasta ou outro VMC. Durante uma cópia, X abre um VMC, para selecionar saves dele ou colar dentro dele com START (`.mcd`, `.mc2` e `.bin`).
- **Pacote SAS no slot 2:** com o sd2psx no slot 2 e outro cartão no slot 1, o pacote ia para o outro cartão; agora vai para o slot do sd2psx, e o caminho para o OPL aparece (`mc1:`). Obrigado ao nuno6573 pelo relato :)
- **Downloads que caem:** uma atualização (GitHub) ou uma restauração (Drive) que cai no meio continua de onde parou, numa conexão nova, até três vezes (o erro "AES-GCM Authentication check fail", #2). Obrigado ao @Lithium-ps pelo log :)
- **Reativar avisos**, nas opções do template: os cartões de jogo marcados como "Não avisar" voltam a ser avisados.
- **Tudo atualizado:** a toolchain mais nova, wolfSSL 5.9.4, curl 8.22.0 e o mmceman, que também corrige a leitura de um cartão no slot 2.
- Um VMC de 16 MB aberto na aba Arquivos tem a memória de volta; a caixa "Sobre" cabe na tela de novo; um save de um VMC pode ser copiado para uma pasta.
**Instalação:** extraia o `SD2Cloud-v1.7.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
