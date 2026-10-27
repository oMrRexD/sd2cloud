# SD2Cloud v2.0

**Manage and back up your PS2 memory cards, right on the console.**

### What's new in 2.0
- **Works without an sd2psx.** Started from a USB drive, an MX4SIO or the internal HDD, SD2Cloud keeps its data there and manages the regular Memory Cards in both slots (open, copy, upload to Drive, restore), the OPL's VMCs and card files. The Memory Cards' saves are backed up one by one, and the first sync offers them. The SAS package on the Memory Card opens SD2Cloud wherever it is (it carries the drivers of that device), from OPL's IGR and from the PS2 browser.
- **The Library.** Saves can be kept off the cards: Move or Copy a save (from a Memory Card that is full, an sd2psx card or a VMC) to the new Library tab, where each one is kept in a file of its own, by game, with no size limit. Its saves are moved or copied back to any card the same way. Settings > "Library on Drive" also sends them to Google Drive.
- **Google Drive, organized.** Backups are kept in folders by device, game and card, each card with its own Saves folder, named in the app's language. A Drive with backups from earlier versions is offered the new organization once: the files are moved into it with nothing uploaded again (with many backups, it can take a few minutes) (or later, from Settings > Google Drive).
- **Google Drive in the Files tab.** Browse the backups on Drive, open a save's page to import it into a card, open or install a card's backup, download any file into a folder of a device, or delete it (it goes to Drive's trash).
- **Install card, anywhere.** A card file (or a card's backup on Drive) is installed as a card of the microSD or, from the new Files tab of "Install to", saved as a file on any device, in the format you pick (`.mcd`, `.mc2`, `.ps2` or OPL's `.bin`).
- **Create, resize, rename and delete cards.** "New card" makes a numbered card, a card in a folder of your own or a card file on any device (`.mcd`, `.mc2`, `.ps2` or OPL's `.bin`). Card sizes go from 1 to 64 MB on an sd2psx and from 8 to 128 MB on a MemCard PRO2, with a recommended size for the saves going in. A card can be made again in another size, renamed or deleted, as can a game's whole folder. Settings > "New card size" sets the size the sd2psx uses for a game's first card.
- **Convert to Game ID.** A card's saves (also the Memory Card's or a card file's) are copied to their games' cards, the way Game ID keeps them, with new cards made as needed. It runs on one screen, with the total time left; the source card is only changed if you choose to remove the copied saves at the end.
- **More storage.** The Files tab reads the internal HDD (exFAT, FAT32 and APA, including disks set up by the PSBBN installer with both) and an MX4SIO (when slot 2 has no Memory Card), next to the MMCE and USB.
- **OPL VMCs.** The OPL's VMCs have a tab of their own, "OPL VMCs", with the games that use each one. They are opened, synced, restored, resized and created like any card. Settings > "OPL VMC folders" adds the VMC folders of the HDD, an MX4SIO or a USB drive.
- **Backup by save.** When turned on in the settings, each save that changed is also uploaded by itself, with its versions; a save's page lists its "Versions on Drive" and restores any of them. The automatic sync also covers the Memory Card in the other slot, with its progress shown save by save.
- **Safety.** Before a card is deleted, resized or has its saves moved, a safety copy is kept (Tools > Safety copies, where it can be put back). "Check card" looks through a card's file system without changing anything. The whole Memory Card can be restored from Drive.
- **sd2psx and MemCard PRO2 on the same microSD.** The two devices are shown at the top right; L2 and R2 switch between their cards, to copy saves between the two.
- **Export saves to a card.** In "All saves", the saves you pick (all of them at first) go into a new card, wherever you choose to make it.
- **Marking, faster.** On every screen that marks (the saves of a copy, a template, Convert to Game ID, Export saves to a card), hold X and move the cursor to mark or unmark everything it passes over; Square marks or unmarks them all. A copy can take up to 256 saves at once. The left stick moves the cursor on every screen.
- **Circle cancels more:** the automatic sync after a game from its first screen on, Convert to Game ID while it copies (the saves copied so far stay on their games' cards), and the search for a drive in the Files tab.
- **Settings in pages:** Google Drive, Automatic sync, Cards and Updates, each with its own page. Triangle cancels the selection of saves after Copy.
- **Fixes.** Replacing a save (with a newer one, a template's or a version from Drive) never loses the one that was there when the new one can't be written. After a game with nothing new, IGR no longer waits for the network. A console clock reset to 2000 no longer dates backups in 2099. Taking the sd2psx out with SD2Cloud open no longer asks to put it back: the screens follow, and follow it back. The automatic sync no longer hangs on "Connecting to Google..." when the Memory Card stops answering. Leaving SD2Cloud after opening an APA partition no longer hangs the console. Programs on the HDD or an MX4SIO are started with their own device names, so wLaunchELF finds its network settings. The settings and the tabs scroll when they don't fit.

Thanks to nuno6573 for testing the beta.

**Before using the new card tools,** keep a backup of your microSD: they write to your cards, and although every change is verified and safety copies are kept, a backup is the only sure way back.

To update, use Settings > Check for updates, or copy the new files over the old ones.

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
- Copies (several saves at once, marked by holding X over them), moves and deletes saved data between cards, and uploads a single item to Drive as a .psu file.
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
- Returns to OPL on the MMCE, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English, Portuguese, Spanish, Russian, Ukrainian and Japanese.

**Installation:** extract `SD2Cloud-v2.0.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. With no sd2psx, extract it to a USB drive, an MX4SIO or the internal HDD instead. See README.txt for details. The zip no longer has `SD2Cloud/sd2cloud.ini`: SD2Cloud creates it the first time it runs, so extracting the zip over an installation keeps your settings. `Extras/sd2cloud.example.ini` explains every setting.

The full list of changes since 1.7.1: https://github.com/oMrRexD/sd2cloud/compare/v1.7.1...v2.0

---

**Gerencie e proteja os cartões de memória do seu PS2, direto no console.**

**Novidades da 2.0**
- **Funciona sem sd2psx:** aberto de um pendrive, de um MX4SIO ou do HD interno, o SD2Cloud guarda os dados ali e gerencia os Memory Cards comuns dos dois slots (abrir, copiar, enviar ao Drive, restaurar), os VMCs do OPL e os arquivos de cartão. Os saves dos Memory Cards vão para o Drive um a um, e a primeira sincronização já os oferece. O pacote SAS no Memory Card abre o SD2Cloud de onde ele estiver (leva os drivers desse dispositivo), pelo IGR do OPL e pelo navegador do PS2.
- **Biblioteca:** os saves podem ficar guardados fora dos cartões. Mova ou copie um save (de um Memory Card cheio, de um cartão do sd2psx ou de um VMC) para a nova aba Biblioteca, onde cada um fica num arquivo próprio, separado por jogo, sem limite de espaço. Para devolver, é só mover ou copiar de volta para qualquer cartão. Ajustes > "Biblioteca no Drive" também envia os saves ao Google Drive.
- **Google Drive organizado:** os backups ficam em pastas por aparelho, jogo e cartão, cada cartão com a sua pasta Saves, com os nomes no idioma do app. Um Drive com backups de versões anteriores recebe uma vez a oferta da nova organização: os arquivos são movidos sem enviar nada de novo (com muitos backups, pode levar alguns minutos) (ou depois, em Ajustes > Google Drive).
- **Google Drive na aba Arquivos:** navegue pelos backups no Drive, abra a página de um save para importá-lo para um cartão, abra ou instale o backup de um cartão, baixe qualquer arquivo para uma pasta de um dispositivo ou exclua (ele vai para a lixeira do Drive).
- **Instalar cartão em qualquer lugar:** um arquivo de cartão (ou o backup de um cartão no Drive) é instalado como cartão do microSD ou, pela nova aba Arquivos de "Instalar em", salvo como arquivo em qualquer dispositivo, no formato que você escolher (`.mcd`, `.mc2`, `.ps2` ou `.bin` do OPL).
- **Criar, redimensionar, renomear e excluir cartões:** "Novo cartão" cria um cartão numerado, um cartão numa pasta sua ou um arquivo de cartão em qualquer dispositivo (`.mcd`, `.mc2`, `.ps2` ou `.bin` do OPL). Os tamanhos vão de 1 a 64 MB no sd2psx e de 8 a 128 MB no MemCard PRO2, com um tamanho recomendado para os saves. Um cartão pode ser refeito em outro tamanho, renomeado ou excluído, assim como a pasta inteira de um jogo. Ajustes > "Tamanho dos cartões novos" define o tamanho que o sd2psx usa no primeiro cartão de um jogo.
- **Converter para Game ID:** os saves de um cartão (também do Memory Card ou de um arquivo de cartão) são copiados para os cartões dos seus jogos, como o Game ID os organiza, criando cartões novos quando preciso. Tudo numa tela só, com o tempo total restante; o cartão de origem só muda se, no fim, você escolher remover os saves copiados.
- **Mais armazenamento:** a aba Arquivos lê o HD interno (exFAT, FAT32 e APA, inclusive os discos preparados pelo instalador do PSBBN, com os dois) e o MX4SIO (quando o slot 2 não tem Memory Card), além do MMCE e do USB.
- **VMCs do OPL:** os VMCs do OPL têm aba própria, "VMCs do OPL", com os jogos que usam cada um. Abrem, sincronizam, restauram, mudam de tamanho e são criados como qualquer cartão. Ajustes > "Pastas de VMC do OPL" adiciona as pastas de VMC do HD, de um MX4SIO ou de um pendrive.
- **Backup por save:** quando ativado nos Ajustes, cada save alterado também é enviado separadamente, com as suas versões; a página do save lista as "Versões no Drive" e restaura qualquer uma. A sincronização automática também inclui o Memory Card do outro slot, com o progresso save por save.
- **Segurança:** antes de excluir, redimensionar ou mover os saves de um cartão, uma cópia de segurança é guardada (Ferramentas > Cópias de segurança, de onde pode ser restaurada). "Verificar cartão" confere o sistema de arquivos de um cartão sem alterar nada. O Memory Card inteiro pode ser restaurado do Drive.
- **sd2psx e MemCard PRO2 no mesmo microSD:** os dois aparelhos aparecem no canto superior direito; L2 e R2 alternam entre os cartões de um e do outro, para copiar saves entre os dois.
- **Exportar saves para um cartão:** em "Todos os saves", os saves que você marcar (todos, de início) vão para um cartão novo, criado onde você escolher.
- **Marcar ficou mais rápido:** em toda tela que marca (os saves de uma cópia, um template, Converter para Game ID, Exportar saves para um cartão), segure o X e mova o cursor para marcar ou desmarcar tudo por onde ele passa; o Quadrado marca ou desmarca todos. Uma cópia aceita até 256 saves de uma vez. O analógico esquerdo move o cursor em todas as telas.
- **O Círculo cancela mais coisas:** a sincronização automática depois de um jogo desde a primeira tela, o Converter para Game ID enquanto copia (os saves já copiados ficam nos cartões dos jogos) e a busca por um dispositivo na aba Arquivos.
- **Ajustes em páginas:** Google Drive, Sincronização automática, Cartões e Atualizações, cada um com a sua página. O Triângulo cancela a seleção de saves depois de Copiar.
- **Correções:** substituir um save (por um mais novo, pelo de um template ou por uma versão do Drive) nunca perde o que estava lá quando o novo não pode ser gravado. Depois de um jogo sem nada novo, o IGR não espera mais pela rede. Um relógio zerado em 2000 não data mais os backups em 2099. Tirar o sd2psx com o SD2Cloud aberto não pede mais para recolocá-lo: as telas acompanham, na ida e na volta. A sincronização automática não fica mais parada em "Conectando ao Google..." quando o Memory Card deixa de responder. Sair do SD2Cloud depois de abrir uma partição APA não trava mais o console. Programas no HD ou no MX4SIO abrem pelo nome do próprio dispositivo, e o wLaunchELF encontra a configuração de rede. Os Ajustes e as abas rolam quando não cabem na tela.

Agradecimentos a nuno6573 pelos testes da beta.

**Antes de usar as ferramentas novas de cartão,** faça um backup do seu microSD: elas gravam nos seus cartões e, embora cada alteração seja conferida e as cópias de segurança fiquem guardadas, o backup é a única garantia.

**Instalação:** extraia o `SD2Cloud-v2.0.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Sem sd2psx, extraia num pendrive, num MX4SIO ou no HD interno. Consulte o LEIA-ME.txt para mais detalhes. O zip não traz mais o `SD2Cloud/sd2cloud.ini`: o SD2Cloud o cria na primeira vez que é aberto, então extrair o zip por cima de uma instalação mantém as suas configurações. O `Extras/sd2cloud.example.ini` explica cada configuração.

A lista completa de alterações desde a 1.7.1: https://github.com/oMrRexD/sd2cloud/compare/v1.7.1...v2.0
