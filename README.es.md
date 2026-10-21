<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/es/dark.png">
    <source media="(prefers-color-scheme: light)" srcset="docs/logo/es/light.png">
    <img src="docs/logo/es/light.png" width="400" alt="SD2Cloud">
  </picture>
</h1>

**Administra y respalda las tarjetas de memoria de tu PS2, directamente en la consola.**

[![Downloads](https://img.shields.io/github/downloads/oMrRexD/sd2cloud/total?style=for-the-badge&logo=github)](https://github.com/oMrRexD/sd2cloud/releases/latest)
> [!WARNING]
> SD2Cloud modifica tus tarjetas de memoria cuando copia, mueve, elimina, importa o restaura partidas, y cuando crea,
> cambia de tamaño o elimina tarjetas. Haz una copia antes: sincroniza tus tarjetas con Google Drive, o copia la
> carpeta `MemoryCards` de la microSD a un PC.

SD2Cloud funciona en la propia PS2. Trabaja con las tarjetas de memoria de la microSD de los dispositivos de la familia
sd2psx con el firmware [sd2psXtd](https://github.com/sd2psXtd/firmware) (sd2psx, PSXMemCard, PSXMemCard Gen2,
PicoMemcard+/Zero; la PS2 la llama MMCE) y de la MemCard PRO2, y con Memory Cards comunes, las tarjetas virtuales de OPL
(VMC) y archivos de tarjeta de memoria en una memoria USB, un MX4SIO o el disco duro interno. No hace falta un PC.

[English](README.md) · [Português](README.pt-BR.md) · **Español** · [Русский](README.ru.md) · [Українська](README.uk.md) · [日本語](README.ja.md)

<p>
  <img src="docs/screens/menu-es.png" width="49%" alt="Pantalla principal">
  <img src="docs/screens/saves-es.png" width="49%" alt="Partidas de una tarjeta de memoria">
</p>
<p>
  <img src="docs/screens/save-es.png" width="49%" alt="La página de la partida de un juego">
  <img src="docs/screens/upload-es.png" width="49%" alt="La partida de un juego subiendo a Google Drive">
</p>
<p>
  <img src="docs/screens/files-es.png" width="49%" alt="La pestaña Archivos: una carpeta del MMCE con archivos .psu">
  <img src="docs/screens/psu-es.png" width="49%" alt="Un archivo .psu a punto de importarse a una tarjeta de memoria">
</p>

## Funciones

**Sincronización en la nube**
- Sincroniza tus tarjetas de memoria con tu Google Drive, cuando quieras o automáticamente cada vez que sales de un
  juego con IGR. Solo se suben las tarjetas que cambiaron.
- Guarda un historial de copias de cada tarjeta (10 por defecto) y restaura cualquiera de ellas, verificada antes y
  después de escribirla. Una tarjeta que cambió se sincroniza antes de restaurarse, así que no se pierde nada.
- Envía cada tarjeta como el `.mcd` del propio sd2psx o, si lo eliges en los ajustes, como `.ps2`, el formato que usa
  PCSX2: sácalo del zip de la copia y queda listo para el emulador. Los dos se pueden restaurar.
- **Copia por partida**: si quieres, envía también cada partida que cambió por separado, con sus versiones; la página
  de la partida muestra sus **Versiones en Drive** y restaura cualquiera.
- Guarda las copias organizadas por dispositivo, juego y tarjeta, cada tarjeta con su propia carpeta de partidas, y
  las recorre desde la pestaña **Archivos**: la página de una partida, la copia de una tarjeta abierta como tarjeta o
  instalada, cualquier archivo descargado o eliminado.
- Se conecta una sola vez con un código o un código QR. SD2Cloud solo puede acceder a los archivos que él mismo crea en
  tu Drive.

**Administrador de tarjetas de memoria**
- Muestra las partidas de cada tarjeta como lo hace el navegador de la PS2, con los iconos 3D animados.
- Copia, mueve y elimina partidas entre tarjetas (varias a la vez, seleccionadas en una tarjeta o en varias) y sube una
  partida suelta a Google Drive como archivo `.psu`.
- **Biblioteca**: guarda partidas fuera de las tarjetas (una Memory Card llena, una partida que quieres sacar de una
  tarjeta sin eliminarla), cada una en un archivo propio, por juego, sin límite de espacio; si quieres, también en Drive.
- **Herramientas de tarjeta**: crea tarjetas (numeradas, en una carpeta tuya o como archivo de tarjeta en cualquier
  dispositivo; de 1 a 64 MB en un sd2psx, de 8 a 128 MB en una MemCard PRO2, con un tamaño recomendado), les cambia el
  tamaño, el nombre, las comprueba y las elimina, y **convierte una tarjeta a Game ID** (las partidas de cada juego
  copiadas a la tarjeta de su juego, en una sola pantalla, con el tiempo restante). Antes de cualquier cambio que pueda
  perder datos, guarda una **copia de seguridad** de la tarjeta, que se restaura desde Utilidades.
- Organiza las tarjetas en pestañas: tarjetas numeradas y carpetas tuyas, tarjetas de juego (Game ID) en carpetas con el
  nombre de cada juego, tarjetas de arranque, los **VMC del OPL** (con los juegos que usan cada uno; también las
  carpetas de VMC del disco duro, de un MX4SIO o de una memoria USB) y la Biblioteca.
- Muestra las partidas de todas las tarjetas de juego en una sola pantalla (**Todas las partidas**, en la pestaña
  Juegos), como si fueran una sola tarjeta.
- Marca la tarjeta que está usando el sd2psx y puede hacer que use otra. Para cambiar la tarjeta en uso, pregunta,
  cambia el sd2psx a otra tarjeta y vuelve.
- La pestaña **Archivos** recorre el MMCE, una memoria USB, el disco duro interno (exFAT, FAT32 y APA, incluidos los
  discos preparados por el instalador de PSBBN con los dos) y un MX4SIO: importa y exporta archivos `.psu` y partidas en
  formato de carpeta, abre archivos de tarjeta de memoria (`.mcd`, `.mc2`, `.ps2`, el `.bin` de OPL o un `.zip`) y los
  instala como tarjetas o como archivo de tarjeta en cualquier dispositivo, en el formato que elijas, y copia una
  tarjeta entera a una carpeta como `.zip`.
- Administra las **Memory Cards** comunes de las ranuras de la consola: sus partidas, la tarjeta entera subida a Drive
  y restaurada desde allí.
- En una microSD con las tarjetas de un sd2psx y de una MemCard PRO2, **L2 y R2** cambian entre las tarjetas de uno y
  de otro, para copiar partidas entre los dos.
- Guarda **plantillas**: conjuntos de partidas que marcas en tus tarjetas, puestos en cualquier tarjeta de una vez. Una
  puede ser la **principal**, la que toda tarjeta de juego debería tener: se pone en todas de una vez, y la tarjeta de
  un juego nuevo la recibe cuando SD2Cloud se abre. Con la sincronización automática, la tarjeta que usó un juego recibe
  sus plantillas sola al salir del juego. Están en **Utilidades** (SELECT).

**Además**
- Funciona **sin sd2psx**: abierto desde una memoria USB, un MX4SIO o el disco duro interno, SD2Cloud guarda sus datos
  allí y administra las Memory Cards de las dos ranuras (con copia de cada partida), los VMC del OPL y los archivos
  de tarjeta.
- El sd2psx se puede quitar y volver a poner con SD2Cloud abierto: las pantallas lo siguen.
- Pide confirmación antes de cualquier cosa que suba, escriba o elimine datos.
- Ajustes en la consola (START), en páginas, guardados en `sd2cloud.ini`, que también se puede editar en un PC.
- Vuelve a OPL en el MMCE, una tarjeta de memoria, USB, MX4SIO o el disco duro interno (exFAT o APA), cargando solo
  los controladores de ese dispositivo.
- Inicia otros programas: cualquier ELF elegido en las carpetas de un dispositivo (X sobre él en **Archivos**, o
  "Ejecutar un ELF..." al salir), y una aplicación guardada en una tarjeta de memoria al estilo del Save Application
  System ("Iniciar aplicación" en su página).
- Se actualiza desde las releases de este repositorio, verificado con el SHA-256 de GitHub. Dos canales, elegidos en
  los ajustes: **Estable**, las versiones publicadas, y **Beta**, una compilación hecha automáticamente con cada
  cambio, antes de probarse como una versión (la [pre-release beta](../../releases/tag/beta)).
- Interfaz inspirada en el PlayStation BB Navigator.
- Inglés, portugués, español, ruso, ucraniano y japonés. Las traducciones son bienvenidas: consulta
  [Translating](CONTRIBUTING.md#translating). Las partidas de juegos japoneses muestran sus títulos en japonés.

## Requisitos

- Uno de estos:
  - un dispositivo de la familia sd2psx con el firmware sd2psXtd (compatible con MMCE);
  - una MemCard PRO2: SD2Cloud lee y escribe sus tarjetas `.mc2` en `/PS2`. Cambiar de tarjeta desde la aplicación
    ("Insertar esta tarjeta") es una vista previa allí, y SD2Cloud nunca cambia de tarjeta por su cuenta: para cambiar
    la tarjeta en uso, cambia primero a otra;
  - ninguno de los dos: SD2Cloud en una memoria USB, un MX4SIO o el disco duro interno, con Memory Cards comunes en las
    ranuras.
- Una forma de iniciar SD2Cloud: la pestaña Apps de OPL, wLaunchELF o cualquier otro lanzador que pueda ejecutar un
  ELF. Se recomienda un OPL compatible con MMCE, como [RiptOPL](https://github.com/NathanNeurotic/Open-PS2-Loader).
  Sin él, OPL no lista las aplicaciones del MMCE y no tiene la opción **IGR Bootcard Slot(s)**: con tarjetas Game ID,
  el sd2psx se queda en la tarjeta del juego al salir con IGR, la PS2 vuelve al navegador en lugar de abrir SD2Cloud y
  la sincronización automática no ocurre.
- Para la sincronización automática después de un juego: OPL, que inicia SD2Cloud con su IGR.
- Para las funciones en la nube: una PS2 con adaptador de red (incorporado en los modelos Slim), conectada a tu router
  con un cable, y una cuenta de Google.

## Instalación

1. Descarga `SD2Cloud-vX.Y.zip` desde la [última release](../../releases/latest).
2. Extráelo en la raíz del MMCE. Añade `APPS/SD2Cloud` (la aplicación) y `SD2Cloud/sd2cloud.ini` (los ajustes,
   explicados en el `sd2cloud.example.ini` que está al lado). La carátula y el logo que OPL muestra para SD2Cloud en su
   pestaña Apps van a la carpeta `ART`. El mismo arte con su línea en portugués está en `Extras/ART pt-BR`, para copiarlo
   sobre esos dos archivos.
3. Abre SD2Cloud: desde la pestaña Apps de OPL o ejecutando `APPS/SD2Cloud/SD2CLOUD.ELF` con cualquier lanzador
   (wLaunchELF, por ejemplo). La carpeta no tiene que quedarse en `APPS`.
4. Conecta tu cuenta de Google cuando te lo pida (o más tarde, en Ajustes > Google Drive): entra en google.com/device
   en tu teléfono o computadora, o escanea el código QR, y escribe el código que aparece en la TV.
5. Después SD2Cloud ofrece activar la sincronización automática y sincronizar todas tus tarjetas.

**Sin sd2psx**, extrae el zip en una memoria USB (FAT32 o exFAT), un MX4SIO o el disco duro interno, y abre
`APPS/SD2Cloud/SD2CLOUD.ELF` desde allí: SD2Cloud guarda sus ajustes y sus datos en ese dispositivo. Lo que sincroniza
son las Memory Cards de las ranuras, partida por partida (la copia por partida viene activada), y la primera
sincronización ya las ofrece. Las carpetas de un sd2psx en ese dispositivo son solo archivos allí: ábrelas desde la
pestaña **Archivos**.

Con un sd2psx, SD2Cloud también puede estar en una memoria USB, una tarjeta MX4SIO o el disco duro interno (exFAT o
APA): pon allí la carpeta `APPS/SD2Cloud` y abre SD2Cloud desde allí una vez. Registra dónde está, guarda en el MMCE
(`SD2Cloud/drivers`) los controladores que el paquete SAS necesita para iniciarlo desde ese dispositivo, y se actualiza
allí. Sus ajustes y sus datos se quedan en el MMCE.

**En la tarjeta de memoria: el paquete SAS.** Al activar la sincronización automática, SD2Cloud pone su paquete SAS
(Save Application System) en la tarjeta de memoria en uso: la carpeta `APP_SD2CLOUD`, con lo que OPL ejecuta en el IGR
y un acceso directo que abre SD2Cloud desde donde esté, con su propio icono 3D para el navegador de la PS2 (unos
240 KB; sin sd2psx, también los controladores del dispositivo donde está SD2Cloud, unos 90 KB más). El programa en sí
se queda donde está. El zip también trae el paquete como `Extras/APP_SD2CLOUD.psu`, para importarlo a mano a otra
tarjeta (la propia pestaña **Archivos** de SD2Cloud lo hace): abre SD2Cloud una vez desde donde esté, y el acceso
directo lo encuentra.

**¿Vienes de una versión anterior a la 2.0?** A las copias en Drive se les ofrece una vez, al abrir, la nueva
organización (por dispositivo, juego y tarjeta): los archivos se mueven a ella sin volver a subirse. "Ahora no"
mantiene la anterior, y Ajustes > Google Drive la vuelve a ofrecer.

El [README.txt](package/README.txt) incluido en la release explica cada pantalla en detalle (en inglés).

## Controles

| Pantalla | Botones |
|---|---|
| Pantalla principal | Arriba/Abajo: tarjeta · Izquierda/Derecha o L1/R1: pestaña · L2/R2: las tarjetas del otro dispositivo (en una microSD con los dos) · X: abrir la tarjeta · TRIÁNGULO: opciones de la tarjeta (sincronizar ahora, restaurar una copia, copiar a un dispositivo, insertar en el sd2psx, cambiar nombre, cambiar tamaño, convertir a Game ID, comprobar tarjeta, eliminar tarjeta) · SELECT: utilidades · START: ajustes · O: salir |
| Dentro de una tarjeta | X: abrir una partida · TRIÁNGULO: opciones (sincronizar esta tarjeta; las partidas por nombre o por fecha; convertir a Game ID; en Todas las partidas, repetidas ocultas o visibles y exportar todas a una tarjeta; en la Memory Card, subirla a Drive o restaurar una copia) · O: volver |
| La página de una partida | Copiar · Mover · Eliminar · Subir a Drive · Versiones en Drive |
| Después de Copiar | La partida queda seleccionada, y otras también pueden quedarlo: X selecciona o deselecciona una partida · O: salir de la tarjeta a la lista de tarjetas, para seleccionar partidas de otras tarjetas también · TRIÁNGULO: cancelar · START: pegarlas todas en la tarjeta abierta, o en la que está bajo el cursor en la lista. En la pestaña Juegos de la lista, la carpeta de su juego aparece primero, resaltada, con "Nueva tarjeta" dentro. La pestaña Biblioteca las guarda fuera de las tarjetas. En la pestaña Archivos, X abre las carpetas de un dispositivo: START las pega ahí como archivos `.psu`, o dentro del archivo de tarjeta de memoria bajo el cursor. START en la Memory Card las pega en ella |
| Pestaña Archivos | X: abrir el dispositivo, una carpeta, un archivo `.psu` o un archivo de tarjeta de memoria · CUADRADO: instalar el `.psu` o la carpeta de partida seleccionados en una tarjeta, o el archivo de tarjeta seleccionado como tarjeta del MMCE o como archivo en un dispositivo (su pestaña Archivos) · Izquierda/Derecha: una página arriba o abajo · O: volver |
| Pestaña Archivos > Google Drive | X: abrir una carpeta, el `.psu` de una partida (su página: importar a tarjeta, descargar, eliminar de Drive) o la copia de una tarjeta (la tarjeta) · CUADRADO: instalar la copia de una tarjeta (como desde la tarjeta), descargar cualquier otro archivo · TRIÁNGULO: eliminarlo (va a la papelera de Drive) · O: volver |
| Utilidades (SELECT) | Plantillas (X: una plantilla nueva o una existente; TRIÁNGULO: fijar como principal, agregar partidas, aplicar plantilla, juegos vinculados, cambiar nombre, eliminar) · Copias de seguridad (X: restaurar una, o eliminarla) |
| Ajustes (START) | Sincronizar todas las tarjetas · Google Drive (cuenta, copias conservadas, formato, copia por partida, Biblioteca en Drive) · Sincronización automática (activada o desactivada, paquete SAS, salida del IGR) · Tarjetas (dispositivo, tamaño de tarjetas nuevas, carpetas de VMC del OPL) · Actualizaciones · Idioma · Acerca de |
| Durante una sincronización | O: cancelar · CUADRADO: omitir la tarjeta que se está subiendo · SELECT: la lista de las tarjetas de la sincronización, con lo que pasó con cada una, y de vuelta. En ella, CUADRADO omite una tarjeta (otra vez: deshace) y X ofrece omitirla, marcarla como resuelta (la sincronización automática la deja en paz hasta que vuelva a cambiar) o desactivar su sincronización |

## Sincronización automática después de un juego (IGR)

1. En SD2Cloud, abre Ajustes (START) > Sincronización automática y elige **Paquete SAS** (también se ofrece justo
   después de conectar tu cuenta). SD2Cloud muestra cuánto espacio ocupa (unos 240 KB) y pregunta antes de escribir la
   carpeta `APP_SD2CLOUD` en la tarjeta de memoria en uso; con Autoboot, instálalo mientras la BootCard está en uso.
2. En los ajustes de OPL, pon en **IGR Path** `mc0:/APP_SD2CLOUD/IGR.ELF` (`mc1:` con el sd2psx en la ranura 2:
   SD2Cloud muestra la ruta que hay que usar al instalar el paquete). Si una versión hasta la 1.5 dejó su asistente en
   la tarjeta (`mc0:/BOOT/SD2CLOUD-IGR.ELF`), esa ruta sigue funcionando: instalar el paquete también actualiza ese
   archivo.
3. En OPL, activa también **IGR Bootcard Slot(s)** (página MMCE), con la ranura del sd2psx o BOTH: al salir de un
   juego, OPL vuelve a la BootCard, donde está instalado el paquete.

Desde entonces, salir de un juego con IGR inicia SD2Cloud, que sube las tarjetas que cambiaron y vuelve a OPL. La misma
opción reinstala o desinstala el paquete. SD2Cloud puede estar en cualquier carpeta, en el MMCE o en otro dispositivo:
registra dónde está cada vez que se abre (en `sd2cloud.ini` del MMCE; sin sd2psx, en el propio paquete, con los
controladores de ese dispositivo), y el paquete lo inicia desde allí (también encuentra una carpeta de `APPS` a la que
se le cambió el nombre).

Para pausar la sincronización y dejar todo instalado, pon **Sincronización automática** en Desactivada en los Ajustes:
el IGR va entonces directo al programa que se abre después, sin iniciar SD2Cloud. Lo mismo ocurre mientras no haya una
cuenta de Google conectada.

**Sin instalar nada (memoria USB).** El IGR de OPL también puede iniciar un programa desde una memoria USB. Si la
carpeta `APPS/SD2Cloud` está en una, formateada en FAT32, omite el paso 1 y pon en **IGR Path**
`mass:/APPS/SD2Cloud/SD2CLOUD-IGR.ELF`: no se escribe nada en la tarjeta de memoria. Para ello, OPL carga los
controladores USB `USBD.IRX` y `USBHDFSD.IRX` desde `mc?:/SYS-CONF`, donde los instala FMCB. Con el paquete SAS en la
tarjeta de memoria nada de eso hace falta, esté donde esté SD2Cloud: el paquete lo inicia desde el MMCE, una memoria
USB, un MX4SIO o el disco duro. Si tu OPL no está en el MMCE, pon su ruta en la sección `[igr]` de `sd2cloud.ini`,
empezando por el dispositivo: `mc?:/` (tarjeta de memoria), `mass:/` (USB), `mx4sio:/`, `ata:/` (disco duro interno,
exFAT) o `hdd0:PARTICIÓN:pfs:/` (disco duro interno, APA), por ejemplo `hdd0:__common:pfs:/APPS/OPL/OPNPS2LD.ELF`.

## Privacidad

Tus tarjetas de memoria van de la PS2 directo a tu propio Google Drive; no interviene ningún otro servidor y no se
recopila nada. SD2Cloud usa el permiso `drive.file`, así que solo puede ver los archivos que él mismo crea. El acceso se
puede revocar en cualquier momento en [myaccount.google.com/permissions](https://myaccount.google.com/permissions).
Además de Google, SD2Cloud solo se conecta a GitHub, y solo cuando eliges Buscar actualizaciones en los ajustes.

## Compilación

SD2Cloud está escrito en C con la toolchain de [ps2dev](https://github.com/ps2dev) (ps2sdk, ps2sdk-ports, gsKit).

1. `tools/build_ports.sh` compila wolfSSL y curl con soporte de RSA de 4096 bits en `ports4096/` (el HTTPS de Google
   lo necesita).
2. `tools/build_mmceman.sh` compila mmceman, el controlador del sd2psx, en `third_party/mmceman/` (el que viene con el
   SDK puede colgar el sd2psx en transferencias largas).
3. Crea un cliente OAuth del tipo "TVs and Limited Input devices" en Google Cloud Console, con la API de Drive
   activada, y genera `src/credentials.h`: `python tools/make_credentials.py client_secret.json src/credentials.h`.
4. `make` compila `dist/SD2CLOUD.ELF` y, en `igr/`, los dos programas pequeños del paquete SAS (el asistente del IGR y
   el acceso directo); `make DEBUG=1` compila una versión de depuración para PCSX2.
5. `python tools/make_release.py` empaqueta la release en `dist/`, con los textos de `package/`.

`make -C tests` ejecuta las pruebas de regresión, con el compilador del propio PC y sin la toolchain de la PS2: consulta
[tests/README.md](tests/README.md). [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) es el mapa del código.

## Reportar errores y contribuir

¿Encontraste un problema? Abre un [issue](../../issues/new/choose) y completa el formulario: la versión de SD2Cloud, la
consola, el dispositivo sd2psx y su firmware, cómo se inició SD2Cloud y los pasos que llevan al problema. Las ideas y
sugerencias también son bienvenidas allí. Los issues se pueden escribir en inglés o en portugués.

Los pull requests son bienvenidos. [CONTRIBUTING.md](CONTRIBUTING.md) explica cómo compilar, las convenciones del código
y qué revisar antes de enviar uno.

## Créditos y licencia

SD2Cloud es desarrollado por MrRexD y se distribuye bajo la [GNU General Public License v3](LICENSE).

El diseño está inspirado en el PlayStation BB Navigator. SD2Cloud usa la fuente
[Varela Round](https://github.com/alefalefalef/Varela-Round-Hebrew), con las letras cirílicas de
[Nunito](https://github.com/googlefonts/nunito) y los caracteres japoneses de
[M PLUS Rounded 1c](https://github.com/coz-m/MPLUS_FONTS) (las tres bajo la SIL Open Font License), y las bibliotecas
listadas en [THIRD-PARTY-NOTICES.txt](package/THIRD-PARTY-NOTICES.txt).

PlayStation es una marca registrada de Sony Interactive Entertainment. SD2Cloud no está afiliado a Sony ni a Google, ni
cuenta con su respaldo.
