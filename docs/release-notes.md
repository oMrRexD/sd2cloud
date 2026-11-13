# SD2Cloud v2.1

**Manage and back up your PS2 memory cards, right on the console.**

### What's new in 2.1
- **After IGR, your choice.** While the automatic sync runs after a game, Triangle (**Exit**) picks what opens when it ends, this time only: another program of `APPS`, SD2Cloud itself, the PS2 Browser or **Turn off the PS2**. The sync goes on while you pick, the choice shows at the top right, and it also applies if the sync is cancelled. To turn the PS2 off after every sync, choose **Turn off the PS2** in Settings > Automatic sync > IGR path. Before the console turns off, SD2Cloud says so for 5 seconds, and Circle (**Don't turn off**) opens the program instead. The internal HDD is put to rest first.
- **Turn off the PS2 from the main screen.** Circle (exit) now offers **Turn off the PS2** too, next to the programs and the PS2 Browser.
- **Replace a save when copying, moving or importing.** A card that already has a save in the same folder no longer refuses the one you paste or import: SD2Cloud asks, showing when the one on the card and the new one were saved (in yellow when the one on the card is the newer), and X (**Replace**) puts the new one in its place. The old one is kept aside until the new one is written, and put back if it can't be. When other saves go too, Square (**Keep existing**) leaves the ones there and copies the rest.
- **A card without room.** Pasting into a card without enough space now says how much is needed and how much is free, and offers a **New card** of their game (when the saves are all of one game) or the **Library**.
- **Safety copies, now optional, and for saves too.** Settings > Cards > **Safety copies**, off unless you turn it on (in 2.0 the whole card was always copied). When on, SD2Cloud keeps a copy of a card before it is deleted, resized or has its saves moved, and a copy of a save before it is deleted or replaced. Tools > Safety copies lists them by date, says what each one is of, and puts one back (a save into the card you pick). The newest 10 copies of cards and 30 of saves are kept.
- **The sd2psx's card in the Files tab.** With an sd2psx, the card it is using is in the Files tab too, as **Memory Card 1** (or 2, by its slot), next to the Memory Card in the other slot, with its picture and name. X opens it, as in the Cards tab. The Memory Card's row also shows its free space.
- **Fixes.** Deleting a card inside a game's folder no longer jumps to another game's folder: the folder stays open, on the card that is left (when the last card goes, the folder next to it is shown). In Spanish, Russian and Ukrainian, the safety copies' note names the Tools screen the way the screen itself does.

To update, use Settings > Updates > Check for updates, or extract the new zip over your installation (your settings are kept).

SD2Cloud runs on the PS2 itself and works with the memory cards on the microSD of sd2psx-family devices (the MMCE; sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero) and of the MemCard PRO2, and with regular Memory Cards. No PC required.

### Cloud sync
- Syncs your memory cards with Google Drive, on demand or automatically every time you exit a game with IGR. Only the cards that changed are uploaded.
- Backups are the sd2psx's own `.mcd` or, if you choose so in the settings, a `.ps2`, the format PCSX2 uses.
- One-time Google account link with a code or a QR code; SD2Cloud can only access the files it creates.
- Keeps the backups in folders by device, game and card, and browses them from the Files tab.
- Keeps 10 backups per card by default; the oldest is removed only after the new backup is verified (SHA-256).
- Restores any backup from Drive, verified before and after writing; a changed card is synced first, so nothing is lost.
- Optionally uploads each changed save by itself, with its versions.
- The first setup offers to turn on automatic sync: it says how much space the SAS package takes on the memory card, installs it once you agree and shows the path to set in OPL. The package can be uninstalled in the settings.

### Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies (several saves at once, marked by holding X over them), moves and deletes saved data between cards, replacing a save of the same folder when you say so, and uploads a single item to Drive as a .psu file.
- Optional safety copies of cards and saves, before they are deleted or changed, put back from Tools.
- Keeps saves off the cards in the Library, by game, with no size limit.
- Creates, resizes, renames, checks and deletes cards, and converts a card to Game ID (each game's saves on its own card).
- Copies a whole card, as a `.zip` with the `.mcd` or the `.ps2` inside, to a folder of the MMCE, a USB drive, the internal HDD or an MX4SIO.
- Organizes the cards in tabs: numbered cards and your own folders, game (Game ID) cards in folders named after each game, boot cards, the OPL's VMCs and the Library.
- Marks the card the sd2psx is using and can make it take another one.
- Imports `.psu` files into any card and exports any item of a card as a `.psu` (the Files tab).
- Manages the regular Memory Cards in the console's slots (the Files tab), uploads them whole to Drive and restores them.
- Keeps templates: sets of saves to put into cards (Tools, on SELECT).
- Shows every game card's saves on one screen, sorted by date or by name.

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- Settings on START, in pages: sync all cards; Google Drive (account, backups per card, backup format, backup by save, Library on Drive); automatic sync (on or off, the SAS package, the IGR path); cards (device, new card size, OPL VMC folders); language; updates (check, channel); and About. Changes are saved to `SD2Cloud/sd2cloud.ini`.
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device; after IGR it can also turn the console off.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English, Portuguese, Spanish, Russian, Ukrainian and Japanese.

**Installation:** extract `SD2Cloud-v2.1.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. With no sd2psx, extract it to a USB drive, an MX4SIO or the internal HDD instead. See README.txt for details. Extracting the zip over an installation keeps your settings; `Extras/sd2cloud.example.ini` explains every setting.

The full list of changes since 2.0: https://github.com/oMrRexD/sd2cloud/compare/v2.0...v2.1

---

**Gerencie e proteja os cartões de memória do seu PS2, direto no console.**

**Novidades da 2.1**
- **Depois do IGR, você escolhe:** enquanto a sincronização automática roda depois de um jogo, o Triângulo (**Saída**) escolhe o que abre quando ela termina, só desta vez: outro programa da `APPS`, o próprio SD2Cloud, o Browser do PS2 ou **Desligar o PS2**. A sincronização continua enquanto você escolhe, a escolha aparece no canto superior direito e vale também se a sincronização for cancelada. Para desligar o PS2 depois de toda sincronização, escolha **Desligar o PS2** em Ajustes > Sincronização automática > Saída do IGR. Antes de o console desligar, o SD2Cloud avisa por 5 segundos, e o Círculo (**Não desligar**) abre o programa em vez disso. O HD interno é colocado em repouso antes.
- **Desligar o PS2 pela tela principal:** o Círculo (sair) agora também oferece **Desligar o PS2**, ao lado dos programas e do Browser do PS2.
- **Substituir um save ao copiar, mover ou importar:** um cartão que já tem um save na mesma pasta não recusa mais o que você cola ou importa. O SD2Cloud pergunta, mostrando quando o do cartão e o novo foram salvos (em amarelo quando o do cartão é o mais recente), e o X (**Substituir**) põe o novo no lugar. O antigo fica guardado até o novo ser gravado, e volta se o novo não puder ser gravado. Quando outros saves também vão, o Quadrado (**Manter os existentes**) deixa os que já estão lá e copia o resto.
- **Cartão sem espaço:** colar num cartão sem espaço suficiente agora mostra quanto é necessário e quanto está livre, e oferece um **Novo cartão** do jogo (quando os saves são todos de um jogo) ou a **Biblioteca**.
- **Cópias de segurança, agora opcionais e também de saves:** Ajustes > Cartões > **Cópias de segurança**, desativadas até você ativar (na 2.0 o cartão inteiro era sempre copiado). Ativadas, o SD2Cloud guarda uma cópia do cartão antes de ele ser excluído, redimensionado ou ter os saves movidos, e uma cópia do save antes de ele ser excluído ou substituído. Ferramentas > Cópias de segurança lista as cópias por data, diz de que é cada uma e restaura (um save vai para o cartão que você escolher). Ficam as 10 cópias mais novas de cartões e as 30 de saves.
- **O cartão do sd2psx na aba Arquivos:** com sd2psx, o cartão que ele está usando também aparece na aba Arquivos, como **Memory Card 1** (ou 2, pelo slot), ao lado do Memory Card do outro slot, com a figura e o nome. O X o abre, como na aba Cartões. A linha do Memory Card também mostra o espaço livre.
- **Correções:** excluir um cartão dentro da pasta de um jogo não pula mais para a pasta de outro jogo: a pasta continua aberta, no cartão que sobrou (quando sai o último cartão, aparece a pasta ao lado). Em espanhol, russo e ucraniano, o aviso das cópias de segurança chama a tela Ferramentas pelo mesmo nome que a própria tela usa.

**Como atualizar:** Ajustes > Atualizações > Verificar atualizações, ou extraia o zip novo por cima da instalação (as suas configurações são mantidas).

**Instalação:** extraia o `SD2Cloud-v2.1.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Sem sd2psx, extraia num pendrive, num MX4SIO ou no HD interno. Consulte o LEIA-ME.txt para mais detalhes. O `Extras/sd2cloud.example.ini` explica cada configuração.

A lista completa de alterações desde a 2.0: https://github.com/oMrRexD/sd2cloud/compare/v2.0...v2.1
