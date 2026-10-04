# SD2Cloud v1.3

**Manage and back up your sd2psx memory cards, right on your PS2.**

### What's new in 1.3
- Copy to a device: a card's options (TRIANGLE on the main screen) can now copy the whole card, as a file, to a folder of the microSD or of a USB drive. SD2Cloud asks whether to write a `.mcd` (sd2psx) or a `.ps2` (PCSX2), then reads the file back and compares it.

### What's new in 1.2.2
- The question about connecting a Google account at startup has a third answer, "Don't ask again" (TRIANGLE): it is kept in `sd2cloud.ini` (`ask_connect = no`) and the account can still be connected in Settings.
- Closing the IGR helper's window in Settings no longer freezes for a few seconds: the helper on the memory card is only read again after it was installed or removed.

### What's new in 1.2.1
- Copy, on a save's page, now lists the Files tab among its destinations: pick the microSD or the USB drive, go to a folder and press TRIANGLE to export that save there as a `.psu`.

### What's new in 1.2
- Files: a new tab on the main screen browses the folders of the microSD and of a USB drive (FAT32 or exFAT). X on a `.psu` file opens the saved data it holds, to import it into any card; TRIANGLE exports an item of a card, as a `.psu`, into the folder shown. A file is checked whole before the card is changed, and what is written is read back and compared. No Google account or network is needed for this.
- From 1.1: the backup format setting, `.mcd` (the sd2psx's own) or `.ps2` (the format PCSX2 uses).

To update, use Settings > Check for updates, or copy the new files over the old ones.

SD2Cloud runs on the PS2 itself and works with the memory cards on the microSD of sd2psx-family devices (sd2psXtd firmware: sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No PC required.

### Cloud sync
- Syncs your memory cards with Google Drive, on demand or automatically every time you exit a game with IGR. Only the cards that changed are uploaded.
- One-time Google account link with a code or a QR code; SD2Cloud can only access the files it creates.
- Keeps 10 backups per card by default; the oldest is removed only after the new backup is verified (SHA-256).
- Restores any backup from Drive, verified before and after writing; a changed card is synced first, so nothing is lost.
- The first setup offers to turn on automatic sync: it says how much space the IGR helper takes on the memory card, installs it once you agree and shows the path to set in OPL. The helper can be uninstalled in the settings, and SD2Cloud can be kept in any folder of the microSD.

### Memory card manager
- Shows the saved data of each card the way the PS2 browser does, with animated 3D icons.
- Copies, moves and deletes saved data between the cards on the microSD (the destination's free space is shown), and uploads a single item to Drive as a .psu file.
- Organizes the cards in tabs: numbered cards, game (Game ID) cards and boot cards.
- Imports `.psu` files into any card and exports any item of a card as a `.psu`, from and to the microSD or a USB drive (the Files tab).

### Also
- Every operation that uploads, writes or deletes data asks for confirmation; uploads can be cancelled with the Circle button.
- For OPL builds that do not list the apps on the sd2psx microSD: copy the `APPS/SD2Cloud` folder to a USB drive or an MX4SIO card as well and open SD2Cloud from there. On a USB drive, OPL's IGR can run the helper from that folder, with nothing installed on the memory card.
- Settings on START: sync all cards, IGR helper, the program to open after IGR, language, backups per card, backup format, check for updates, Google account and About. Changes are saved to `SD2Cloud/sd2cloud.ini` on the microSD.
- Returns to OPL on the microSD, a memory card, USB, MX4SIO or the internal HDD (exFAT or APA), loading only the drivers of that device.
- Built-in updates, verified against the SHA-256 published on GitHub.
- Interface inspired by the PlayStation BB Navigator, with sound effects.
- English and Portuguese.

**Installation:** extract `SD2Cloud-v1.3.zip` to the root of the sd2psx microSD and open SD2Cloud from the OPL Apps tab. See README.txt for details.

---

**Gerencie e proteja os cartões de memória do seu sd2psx, direto no PS2.**
Novidade da 1.3: nas opções de um cartão (TRIÂNGULO na tela principal), "Copiar para dispositivo" grava o cartão inteiro, como `.mcd` ou `.ps2`, numa pasta do microSD ou de um pendrive USB. Novidade da 1.2.2: a pergunta sobre conectar a conta do Google ao abrir ganhou a resposta "Não perguntar mais" (TRIÂNGULO), e fechar a janela do assistente de IGR nas Configurações não trava mais por alguns segundos. Novidade da 1.2.1: o Copiar da página de um item também lista a aba Arquivos entre os destinos, para exportar o `.psu` direto para uma pasta do microSD ou do pendrive. Novidade da 1.2: a aba Arquivos, na tela principal, percorre as pastas do microSD e de um pendrive USB (FAT32 ou exFAT). X em um arquivo `.psu` abre os dados salvos que ele contém, para importar em qualquer cartão; TRIÂNGULO exporta um item de um cartão, como `.psu`, para a pasta exibida. O arquivo é verificado por inteiro antes de o cartão ser alterado, e o que é gravado é lido de volta e comparado. Não precisa de conta do Google nem de rede.
Instalação: extraia o `SD2Cloud-v1.3.zip` na raiz do microSD do sd2psx e abra o SD2Cloud pela aba Apps do OPL. Consulte o LEIA-ME.txt para mais detalhes.
