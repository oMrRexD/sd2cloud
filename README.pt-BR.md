<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/pt-BR/dark.png">
    <source media="(prefers-color-scheme: light)" srcset="docs/logo/pt-BR/light.png">
    <img src="docs/logo/pt-BR/light.png" width="400" alt="SD2Cloud">
  </picture>
</h1>

**Gerencie e proteja os cartões de memória do seu PS2, direto no console.**

> [!WARNING]
> O SD2Cloud grava nos seus cartões de memória sempre que copia, move, exclui, importa ou restaura saves, e sempre
> que cria, redimensiona ou exclui cartões. Faça um backup antes: sincronize os cartões com o Google Drive ou copie
> a pasta `MemoryCards` do microSD para um PC.

O SD2Cloud é um aplicativo que roda no próprio PS2, sem precisar de PC. Ele faz backup dos seus cartões de memória
no Google Drive e permite organizar os seus saves direto no console.

Funciona com:
- **sd2psx e aparelhos da mesma família** (PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero) com o firmware
  [sd2psXtd](https://github.com/sd2psXtd/firmware). No PS2, esses aparelhos (e o microSD deles) são chamados de
  **MMCE**, e é esse o nome usado aqui.
- **MemCard PRO2**.
- **Memory Cards comuns**: os cartões físicos, nos slots do console.
- **VMCs do OPL** (os cartões de memória virtuais) e outros arquivos de cartão de memória guardados em um pendrive
  USB, em um MX4SIO ou no HD interno.

[English](README.md) · **Português** · [Español](README.es.md) · [Русский](README.ru.md) · [Українська](README.uk.md) · [日本語](README.ja.md)

<p>
  <img src="docs/screens/menu-pt.png" width="49%" alt="Tela principal">
  <img src="docs/screens/saves-pt.png" width="49%" alt="Dados salvos de um cartão de memória">
</p>
<p>
  <img src="docs/screens/save-pt.png" width="49%" alt="Página dos dados salvos de um jogo">
  <img src="docs/screens/upload-pt.png" width="49%" alt="Um save de jogo sendo enviado ao Google Drive">
</p>
<p>
  <img src="docs/screens/files-pt.png" width="49%" alt="Aba Arquivos: uma pasta do MMCE com arquivos .psu">
  <img src="docs/screens/psu-pt.png" width="49%" alt="Um arquivo .psu prestes a ser importado para um cartão de memória">
</p>

## Recursos

**Backup no Google Drive**
- Sincroniza os seus cartões de memória com o seu Google Drive quando você quiser, ou automaticamente toda vez que
  você sai de um jogo pelo IGR (In-Game Reset, a combinação de botões do OPL para sair do jogo). Só os cartões que
  mudaram são enviados.
- Guarda um histórico de backups de cada cartão (10 por padrão), e você pode restaurar qualquer um deles. O cartão é
  conferido antes e depois da gravação. Se o cartão mudou desde o último backup, o SD2Cloud faz um backup dele antes
  de restaurar outro por cima, para que nada se perca.
- Envia cada cartão como arquivo `.mcd`, o formato do próprio sd2psx. Se preferir, escolha `.ps2` nos ajustes: é o
  formato do PCSX2, então o cartão tirado do zip do backup já está pronto para o emulador. Os dois podem ser
  restaurados.
- **Backup por save** (opcional): envia também cada save que mudou num arquivo separado, e guarda as versões
  anteriores dele. A página do save mostra as **Versões no Drive**, e você pode restaurar qualquer uma.
- No Drive, os backups ficam em pastas separadas por aparelho, jogo e cartão, e cada cartão tem uma pasta Saves.
  Você navega por eles na aba **Arquivos**: abre a página de um save, abre o backup de um cartão como se fosse um
  cartão, instala esse backup, ou baixa e exclui qualquer arquivo.
- Você conecta a sua conta do Google uma vez só, com um código ou um QR code. O SD2Cloud só enxerga os arquivos que
  ele mesmo cria no seu Drive.

**Gerenciador de cartões de memória**
- Mostra os saves de cada cartão como o navegador do PS2, com os ícones 3D animados.
- Copia, move e exclui saves entre cartões, vários de uma vez, até de cartões diferentes. Também envia um save
  sozinho para o Google Drive como arquivo `.psu` (o formato comum de um save de PS2 avulso).
- A **Biblioteca** guarda saves fora dos cartões, cada um no seu arquivo, separados por jogo e sem limite de
  espaço. Serve para quando um Memory Card está cheio, ou para tirar um save de um cartão sem precisar excluí-lo.
  Ela também pode ficar no Drive.
- As **ferramentas de cartão** criam, redimensionam, renomeiam, verificam e excluem cartões:
  - um cartão novo pode ser numerado, ficar numa pasta com o nome que você quiser ou ser um arquivo de cartão em
    qualquer dispositivo. O tamanho vai de 1 a 64 MB no sd2psx e de 8 a 128 MB no MemCard PRO2, com o tamanho
    recomendado indicado;
  - **Converter para Game ID** divide um cartão em um cartão para cada jogo. Os cartões de Game ID são aqueles que
    o sd2psx troca sozinho quando um jogo começa. Tudo acontece numa tela só, que mostra o tempo restante;
  - antes de qualquer alteração que possa perder dados, o SD2Cloud guarda uma **cópia de segurança** do cartão, que
    você pode restaurar pelas Ferramentas.
- Os cartões ficam separados em abas: cartões numerados e as suas pastas, cartões de jogos (Game ID) em pastas com
  o nome de cada jogo, cartões de boot, os **VMCs do OPL** e a Biblioteca. A aba de VMCs mostra quais jogos usam
  cada VMC, e também traz as pastas de VMC do HD, de um MX4SIO ou de um pendrive.
- **Todos os saves**, na aba Jogos, mostra os saves de todos os cartões de jogos numa tela só, como se fossem um
  único cartão.
- Mostra qual cartão o sd2psx está usando e pode trocá-lo por outro. Para alterar o cartão que está em uso, o
  SD2Cloud pergunta antes, passa o sd2psx para outro cartão enquanto trabalha e depois volta para ele.
- A aba **Arquivos** navega pelo MMCE, por um pendrive USB, pelo HD interno e por um MX4SIO. O HD pode ser exFAT,
  FAT32 ou APA, inclusive os discos preparados pelo instalador do PSBBN, que têm os dois. Por ela você pode:
  - importar e exportar arquivos `.psu` e saves em formato de pasta;
  - abrir arquivos de cartão de memória (`.mcd`, `.mc2`, `.ps2`, o `.bin` do OPL ou um `.zip`) e instalá-los como
    cartões, ou como arquivo de cartão em qualquer dispositivo, no formato que você escolher;
  - copiar um cartão inteiro para uma pasta, como `.zip`.
- Gerencia os **Memory Cards** comuns dos slots do console: os saves deles, e o cartão inteiro enviado para o Drive
  e restaurado de lá.
- Se o microSD tem cartões de um sd2psx e de um MemCard PRO2, **L2 e R2** alternam entre os cartões de um e do
  outro, para você copiar saves entre os dois.
- **Templates**: um template é um conjunto de saves que você escolhe nos seus cartões, para colocar em qualquer
  cartão de uma vez. Um deles pode ser o template **principal**, aquele que todo cartão de jogo deve ter: ele pode
  ser colocado em todos de uma vez e, quando o cartão de um jogo novo não o tem, o SD2Cloud oferece colocá-lo ao
  abrir. Com a sincronização automática, o cartão que um jogo usou recebe os templates dele sozinho quando você sai
  do jogo. Os templates ficam em **Ferramentas** (SELECT).

**Além disso**
- Funciona **sem sd2psx**: abra-o de um pendrive, de um MX4SIO ou do HD interno, e ele guarda os dados ali. Ele
  gerencia os Memory Cards dos dois slots (com backup save por save), os VMCs do OPL e os arquivos de cartão.
- Você pode tirar o sd2psx e colocá-lo de volta com o SD2Cloud aberto: as telas acompanham.
- Pede confirmação antes de qualquer coisa que envie, grave ou apague dados.
- Os ajustes ficam no próprio console (START), divididos em páginas, e são salvos no `sd2cloud.ini`, que também
  pode ser editado no PC.
- Ao sair, pode voltar para o OPL que estiver no MMCE, em um cartão de memória, no USB, no MX4SIO ou no HD interno
  (exFAT ou APA), carregando só os drivers desse dispositivo.
- Abre outros programas: qualquer ELF nas pastas de um dispositivo (X sobre ele em **Arquivos**, ou "Executar um
  ELF..." ao sair) e aplicativos guardados num cartão de memória no formato do Save Application System ("Iniciar
  aplicativo" na página dele).
- Atualiza-se pelas releases deste repositório e confere cada download com o SHA-256 publicado pelo GitHub. Há dois
  canais, escolhidos nos ajustes:
  - **Estável**: as versões lançadas;
  - **Beta**: uma compilação feita automaticamente a cada alteração, antes dos testes pelos quais uma versão passa
    (a [pré-release beta](../../releases/tag/beta)).
- Visual inspirado no PlayStation BB Navigator.
- Em português, inglês, espanhol, russo, ucraniano e japonês. Traduções são bem-vindas: veja
  [Translating](CONTRIBUTING.md#translating). Os saves de jogos japoneses mostram o título em japonês.

## Requisitos

- Um destes:
  - um aparelho da família sd2psx com o firmware sd2psXtd (suporte a MMCE);
  - um MemCard PRO2. O SD2Cloud lê e grava os cartões `.mc2` dele, na pasta `/PS2`. No PRO2, trocar de cartão pelo
    app ("Inserir este cartão") ainda é um recurso em preview, e o SD2Cloud nunca troca de cartão sozinho: para
    alterar o cartão que está em uso, troque para outro antes;
  - nenhum dos dois: o SD2Cloud em um pendrive, um MX4SIO ou no HD interno, com Memory Cards comuns nos slots.
- Uma forma de abrir o SD2Cloud: a aba Apps do OPL, o wLaunchELF ou qualquer outro programa que abra arquivos ELF.
  Se puder, use um OPL com suporte a MMCE, como o [RiptOPL](https://github.com/NathanNeurotic/Open-PS2-Loader). Um
  OPL sem esse suporte não mostra os apps do MMCE e não tem a opção **Slot(s) de Bootcard IGR**. Por isso, com
  cartões de Game ID, ao sair de um jogo pelo IGR o sd2psx continua no cartão do jogo, o PS2 volta para o navegador
  em vez de abrir o SD2Cloud, e a sincronização automática não acontece.
- Para a sincronização automática depois do jogo: o OPL, que abre o SD2Cloud pelo IGR.
- Para os recursos de nuvem: uma conta do Google e um PS2 com adaptador de rede (já vem no modelo Slim) ligado ao
  roteador por cabo.

## Instalação

1. Baixe o `SD2Cloud-vX.Y.zip` da [release mais recente](../../releases/latest).
2. Extraia o zip na raiz do MMCE. Ele cria:
   - `APPS/SD2Cloud`: o aplicativo;
   - `ART`: a capa e a logo que o OPL mostra para o SD2Cloud na aba Apps;
   - `Extras`: a mesma arte com a frase em português (para copiar por cima dos dois arquivos de `ART`), o pacote SAS
     (veja abaixo) e o `sd2cloud.example.ini`, que explica cada configuração.

   Na primeira vez que é aberto, o SD2Cloud cria o arquivo de configurações dele, o `SD2Cloud/sd2cloud.ini`. Esse
   arquivo não vem no zip, então extrair uma versão nova por cima da anterior mantém as suas configurações.
3. Abra o SD2Cloud pela aba Apps do OPL, ou execute `APPS/SD2Cloud/SD2CLOUD.ELF` em qualquer programa que abra
   arquivos ELF (o wLaunchELF, por exemplo). A pasta não precisa ficar em `APPS`.
4. Quando ele pedir, conecte a sua conta do Google (ou depois, em Ajustes > Google Drive): acesse google.com/device
   no celular ou no computador, ou leia o QR code, e digite o código que aparece na TV.
5. Em seguida, o SD2Cloud oferece ativar a sincronização automática e sincronizar todos os cartões.

**Sem sd2psx**, extraia o zip em um pendrive (FAT32 ou exFAT), em um MX4SIO ou no HD interno, e abra o
`APPS/SD2Cloud/SD2CLOUD.ELF` de lá. O SD2Cloud guarda as configurações e os dados nesse dispositivo. Nesse caso, o
backup é dos Memory Cards dos slots, save por save (o backup por save já vem ativado), e a primeira sincronização
já os oferece. Se esse dispositivo tiver pastas de um sd2psx, para o SD2Cloud elas são só arquivos: abra-as pela
aba **Arquivos**.

**Com sd2psx, mas o app em outro dispositivo.** O SD2Cloud também pode ficar em um pendrive, em um MX4SIO ou no HD
interno (exFAT ou APA). Coloque a pasta `APPS/SD2Cloud` lá e abra o SD2Cloud por lá uma vez. Ele registra onde
está, guarda no MMCE (em `SD2Cloud/drivers`) os drivers de que o pacote SAS precisa para abri-lo desse dispositivo,
e passa a se atualizar ali mesmo. As configurações e os dados continuam no MMCE.

**O pacote SAS, no cartão de memória.** Quando você ativa a sincronização automática, o SD2Cloud grava o pacote SAS
(Save Application System) dele no cartão de memória em uso. É a pasta `APP_SD2CLOUD`, de uns 240 KB, com:
- o programa que o OPL executa no IGR;
- um atalho que abre o SD2Cloud onde ele estiver, com ícone 3D próprio no navegador do PS2;
- sem sd2psx, também os drivers do dispositivo onde o SD2Cloud está (uns 90 KB a mais).

O SD2Cloud em si continua onde está. O zip também traz o pacote como `Extras/APP_SD2CLOUD.psu`, para importar à mão
em outro cartão (a aba **Arquivos** do próprio SD2Cloud faz isso). Depois, abra o SD2Cloud uma vez de onde ele
estiver, e o atalho passa a encontrá-lo.

**Vindo de uma versão anterior à 2.0?** A versão 2.0 organiza os backups no Drive de um jeito novo: por aparelho,
jogo e cartão. Na primeira vez que abre, o SD2Cloud oferece mover os seus arquivos para as pastas novas, sem enviar
nada de novo. "Agora não" mantém a organização antiga, e Ajustes > Google Drive oferece a mudança de novo.

O [LEIA-ME.txt](package/LEIA-ME.txt) que vem no zip explica cada tela em detalhes.

## Controles

| Tela | Botões |
|---|---|
| Tela principal | Cima/Baixo: escolher o cartão · Esquerda/Direita ou L1/R1: trocar de aba · L2/R2: os cartões do outro aparelho (num microSD com os dois) · X: abrir o cartão · TRIÂNGULO: opções do cartão (sincronizar agora, restaurar backup, copiar para dispositivo, inserir no sd2psx, renomear, alterar tamanho, converter para Game ID, verificar cartão, excluir cartão) · SELECT: Ferramentas · START: Ajustes · O: sair |
| Dentro de um cartão | X: abrir um save · TRIÂNGULO: opções (sincronizar este cartão; ordenar os saves por nome ou por data; converter para Game ID; em Todos os saves, mostrar ou ocultar os repetidos e exportar todos para um cartão; no Memory Card, enviá-lo ao Drive ou restaurar um backup) · O: voltar |
| Página de um save | Copiar · Mover · Excluir · Enviar ao Drive · Versões no Drive |
| Depois de Copiar | O save fica selecionado, e você pode selecionar outros: X seleciona ou desmarca um save (segure X e mova para fazer vários em sequência) · O: voltar para a lista de cartões, para selecionar saves de outros cartões também · TRIÂNGULO: cancelar · START: colar todos no cartão que está aberto, ou no cartão sob o cursor na lista. Na aba Jogos da lista, a pasta do jogo deles vem primeiro, em destaque, com "Novo cartão" dentro. A aba Biblioteca os guarda fora dos cartões. Na aba Arquivos, X abre as pastas de um dispositivo, e START cola os saves ali como arquivos `.psu`, ou dentro do arquivo de cartão de memória sob o cursor. START no Memory Card cola os saves nele |
| Aba Arquivos | X: abrir um dispositivo, uma pasta, um arquivo `.psu` ou um arquivo de cartão de memória · QUADRADO: instalar o `.psu` ou a pasta de save selecionados em um cartão, ou instalar o arquivo de cartão selecionado como cartão do MMCE ou como arquivo em um dispositivo (na aba Arquivos desse dispositivo) · Esquerda/Direita: uma página para cima ou para baixo · O: voltar |
| Aba Arquivos > Google Drive | X: abrir uma pasta, o `.psu` de um save (a página dele: importar para um cartão, baixar, excluir do Drive) ou o backup de um cartão (abre como cartão) · QUADRADO: instalar o backup de um cartão (como pelo próprio cartão), ou baixar qualquer outro arquivo · TRIÂNGULO: excluir (vai para a lixeira do Drive) · O: voltar |
| Ferramentas (SELECT) | Templates (X: criar um template ou abrir um; TRIÂNGULO: definir como principal, adicionar saves, aplicar template, jogos do template, renomear, excluir) · Cópias de segurança (X: restaurar uma, ou excluí-la) |
| Ajustes (START) | Sincronizar todos os cartões · Google Drive (conta, backups mantidos, formato, backup por save, Biblioteca no Drive) · Sincronização automática (ativada ou desativada, pacote SAS, saída do IGR) · Cartões (dispositivo, tamanho dos cartões novos, pastas de VMC do OPL) · Atualizações · Idioma · Sobre |
| Durante uma sincronização | O: cancelar · QUADRADO: pular o cartão que está sendo enviado · SELECT: a lista dos cartões desta sincronização e como foi cada um (SELECT de novo volta). Nessa lista, QUADRADO pula um cartão (de novo: desfaz), e X oferece pular, marcar como resolvido (a sincronização automática deixa o cartão de lado até ele mudar de novo) ou desativar a sincronização dele |

## Sincronização automática depois do jogo (IGR)

1. No SD2Cloud, abra Ajustes (START) > Sincronização automática e selecione **Pacote SAS** (ele também é oferecido
   logo depois que você conecta a conta). O SD2Cloud mostra o espaço necessário (uns 240 KB) e pede confirmação
   antes de gravar a pasta `APP_SD2CLOUD` no cartão de memória em uso. Se você usa o Autoboot, instale com o
   BootCard em uso.
2. Nas configurações do OPL, em **Definir saída do IGR**, coloque `mc0:/APP_SD2CLOUD/IGR.ELF` (`mc1:` se o sd2psx
   estiver no slot 2; o SD2Cloud mostra o caminho certo ao instalar o pacote). Se a versão 1.5 ou anterior deixou
   o assistente dela no cartão, em `mc0:/BOOT/SD2CLOUD-IGR.ELF`, esse caminho continua funcionando: instalar o
   pacote também atualiza esse arquivo.
3. No OPL, ative também **Slot(s) de Bootcard IGR** (página MMCE), no slot do sd2psx ou em BOTH. Assim, ao sair do
   jogo, o OPL volta para o BootCard, onde o pacote está.

A partir daí, sair de um jogo pelo IGR abre o SD2Cloud, que envia os cartões que mudaram e volta para o OPL. A
mesma opção dos Ajustes reinstala ou remove o pacote.

O SD2Cloud pode ficar em qualquer pasta, no MMCE ou em outro dispositivo. Toda vez que abre, ele registra onde está
(no `sd2cloud.ini` do MMCE; sem sd2psx, no próprio pacote, junto com os drivers desse dispositivo), e o pacote o
abre a partir dali. Ele encontra até uma pasta de `APPS` que foi renomeada.

Para pausar a sincronização sem desinstalar nada, deixe **Sincronização automática** como Desativada nos Ajustes.
O IGR então pula o SD2Cloud e vai direto para o programa que abriria depois dele. O mesmo acontece enquanto não há
conta do Google conectada.

**Sem instalar nada (pendrive USB).** O IGR do OPL também abre programas de um pendrive. Se a pasta
`APPS/SD2Cloud` estiver num pendrive formatado em FAT32, pule o passo 1 e coloque
`mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF` em **Definir saída do IGR**. Nada é gravado no cartão de memória. Para isso
funcionar, o OPL carrega os drivers USB `USBD.IRX` e `USBHDFSD.IRX` de `mc?:/SYS-CONF`, onde o FMCB os instala.
Com o pacote SAS no cartão de memória nada disso é necessário, esteja o SD2Cloud onde estiver: o pacote o abre do
MMCE, de um pendrive, de um MX4SIO ou do HD.

Se o seu OPL não está no MMCE, coloque o caminho dele na seção `[igr]` do `sd2cloud.ini`, começando pelo
dispositivo: `mc?:/` (cartão de memória), `mass:/` (USB), `mx4sio:/`, `ata:/` (HD interno exFAT) ou
`hdd0:PARTIÇÃO:pfs:/` (HD interno APA). Por exemplo: `hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF`.

## Privacidade

Os seus cartões de memória vão do PS2 direto para o seu Google Drive. Nenhum outro servidor é usado, e nenhuma
informação é coletada. O SD2Cloud usa o escopo `drive.file`, então só enxerga os arquivos que ele mesmo cria. Você
pode revogar o acesso a qualquer momento em
[myaccount.google.com/permissions](https://myaccount.google.com/permissions). Além do Google, o SD2Cloud só se
comunica com o GitHub, e só quando você seleciona Verificar atualizações nos ajustes.

## Compilação

O SD2Cloud é escrito em C com o toolchain do [ps2dev](https://github.com/ps2dev) (ps2sdk, ps2sdk-ports, gsKit).

1. `tools/build_ports.sh` compila o wolfSSL e o curl com suporte a RSA de 4096 bits em `ports4096/` (o HTTPS do
   Google exige isso).
2. `tools/build_mmceman.sh` compila o mmceman, o driver do sd2psx, em `third_party/mmceman/` (o que vem com o SDK
   pode travar o sd2psx em transferências longas).
3. Crie um cliente OAuth do tipo "TVs e dispositivos de entrada limitada" no Google Cloud Console, com a Drive API
   ativada, e gere o `src/credentials.h`: `python tools/make_credentials.py client_secret.json src/credentials.h`.
4. `make` gera o `dist/SD2CLOUD.ELF` e, em `igr/`, os dois programas pequenos do pacote SAS (o assistente de IGR e
   o atalho); `make DEBUG=1` gera uma versão de debug para o PCSX2.
5. `python tools/make_release.py` monta a release em `dist/`, com os textos de `package/`.

`make -C tests` roda os testes de regressão, com o compilador do próprio PC e sem o toolchain do PS2: veja
[tests/README.md](tests/README.md). O mapa do código está em [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (em
inglês).

## Relatar bugs e colaborar

Encontrou um problema? Abra uma [issue](../../issues/new/choose) e preencha o formulário: a versão do SD2Cloud, o
console, o aparelho sd2psx e o firmware dele, como o SD2Cloud foi aberto e os passos que levam ao problema. Ideias e
sugestões também são bem-vindas por lá. As issues podem ser escritas em português ou em inglês.

Pull requests são bem-vindos. O [CONTRIBUTING.md](CONTRIBUTING.md) explica como compilar, as convenções do código e
o que conferir antes de enviar um.

## Créditos e licença

O SD2Cloud é desenvolvido por MrRexD e distribuído sob a [GNU General Public License v3](LICENSE).

O visual é inspirado no PlayStation BB Navigator. O SD2Cloud usa a fonte
[Varela Round](https://github.com/alefalefalef/Varela-Round-Hebrew), com as letras cirílicas da
[Nunito](https://github.com/googlefonts/nunito) e os caracteres japoneses da
[M PLUS Rounded 1c](https://github.com/coz-m/MPLUS_FONTS) (as três sob a SIL Open Font License), e as bibliotecas
listadas no [THIRD-PARTY-NOTICES.txt](package/THIRD-PARTY-NOTICES.txt).

PlayStation é marca registrada da Sony Interactive Entertainment. O SD2Cloud não é afiliado nem endossado pela
Sony ou pelo Google.
