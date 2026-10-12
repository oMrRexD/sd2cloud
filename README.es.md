<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/es/dark.png">
    <source media="(prefers-color-scheme: light)" srcset="docs/logo/es/light.png">
    <img src="docs/logo/es/light.png" width="400" alt="SD2Cloud">
  </picture>
</h1>

**Administra y respalda las tarjetas de memoria de tu sd2psx, directamente en tu PS2.**

[![Downloads](https://img.shields.io/github/downloads/oMrRexD/sd2cloud/total?style=for-the-badge&logo=github)](https://github.com/oMrRexD/sd2cloud/releases/latest)
> [!WARNING]
> SD2Cloud está en una etapa temprana y todavía no se ha probado ampliamente. Antes de copiar, mover, eliminar,
> importar o restaurar partidas con él, haz una copia de tus tarjetas de memoria: copia la carpeta `MemoryCards` de la
> microSD a un PC.

SD2Cloud funciona en la propia PS2 y trabaja con las tarjetas de memoria guardadas en el MMCE (la microSD del sd2psx,
como la llama la PS2) de los dispositivos de la familia sd2psx con el firmware
[sd2psXtd](https://github.com/sd2psXtd/firmware) (sd2psx, PSXMemCard, PSXMemCard Gen2, PicoMemcard+/Zero). No hace
falta un PC. También reconoce una MemCard PRO2, de forma experimental: SD2Cloud todavía no se ha probado en una.

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
- Se conecta una sola vez con un código o un código QR. SD2Cloud solo puede acceder a los archivos que él mismo crea en
  tu Drive.

**Administrador de tarjetas de memoria**
- Muestra las partidas de cada tarjeta como lo hace el navegador de la PS2, con los iconos 3D animados.
- Copia, mueve y elimina partidas entre las tarjetas del MMCE (se muestra el espacio libre del destino) y sube una
  partida suelta a Google Drive como archivo `.psu`. Se pueden copiar varias partidas a la vez, seleccionadas en una
  tarjeta o en varias. Una partida también puede ir a una tarjeta nueva de su propio juego, creada en el momento
  ("Nueva tarjeta", en la pestaña Juegos): una tarjeta vacía de 8 MB, como las crea el propio dispositivo,
  que también recibe las plantillas del juego.
- Muestra las partidas de todas las tarjetas de juego en una sola pantalla (**Todas las partidas**, en la pestaña
  Juegos), como si fueran una sola tarjeta.
- Organiza las tarjetas en pestañas: tarjetas numeradas, tarjetas de juego (Game ID), en carpetas con el nombre de
  cada juego, y tarjetas de arranque (Boot).
- Marca la tarjeta que está usando el sd2psx y puede hacer que use otra (en las opciones de la tarjeta). Para cambiar
  la tarjeta en uso, pregunta, cambia el sd2psx a otra tarjeta y vuelve.
- Copia una tarjeta entera, como un `.zip` con el `.mcd` o el `.ps2` dentro, a una carpeta del MMCE o de una memoria
  USB (en las opciones de la tarjeta).
- Abre archivos de tarjeta de memoria en la pestaña **Archivos** (`.mcd`, el `.mc2` de una MemCard PRO2, el `.ps2`
  de PCSX2, las tarjetas virtuales de OPL (`.bin`) o ese `.zip`), copia partidas desde ellos y los instala en el
  MMCE: como una tarjeta nueva (numerada, de un juego o un canal más de la BootCard) o en lugar de una existente.
- Importa y exporta archivos `.psu`: la pestaña **Archivos** recorre las carpetas del MMCE y de una memoria USB
  (FAT32 o exFAT), instala un `.psu` en cualquier tarjeta y guarda cualquier partida de una tarjeta como `.psu`. También
  instala partidas en formato de carpeta. Lo que se escribe se vuelve a leer y se compara.
- Administra una **Memory Card** común en la otra ranura de la consola, desde la pestaña **Archivos**: sus partidas
  se muestran, copian, mueven y eliminan como las de una tarjeta, y le llegan partidas desde las tarjetas del
  dispositivo o desde archivos `.psu`. Sus partidas, o la tarjeta entera, se pueden subir a Google Drive.
- Guarda **plantillas**: conjuntos de partidas que marcas en tus tarjetas, guardados en el MMCE y puestos en cualquier
  tarjeta de una vez. El dispositivo crea vacía la tarjeta de un juego nuevo; una plantilla le da lo que todas tus
  tarjetas deberían tener, como la configuración de red que pide un juego en línea. Las partidas que la tarjeta ya
  tiene nunca se tocan. Una plantilla puede ser la **principal**, la que toda tarjeta de juego debería tener: se pone en
  todas de una vez y, cuando SD2Cloud se abre y encuentra una tarjeta de juego sin ella (la de un juego nuevo), lo avisa
  y ofrece ponerla. Una plantilla también puede ser solo de algunos juegos. Con la sincronización automática, la
  tarjeta que usó un juego recibe sus plantillas sola al salir del juego. Están en **Utilidades** (SELECT).
  Las partidas de una plantilla no cuentan como un cambio: una tarjeta sincronizada sigue sincronizada.

**Además**
- Pide confirmación antes de cualquier cosa que suba, escriba o elimine datos.
- Ajustes en la consola (START), guardados en `sd2cloud.ini`, que también se puede editar en un PC.
- Vuelve a OPL en el MMCE, una tarjeta de memoria, USB, MX4SIO o el disco duro interno (exFAT o APA), cargando solo
  los controladores de ese dispositivo.
- Inicia otros programas: cualquier ELF elegido en las carpetas del MMCE o de una memoria USB (X sobre él en
  **Archivos**, o "Ejecutar un ELF..." al salir), y una aplicación guardada en una tarjeta de memoria al estilo del Save
  Application System ("Iniciar aplicación" en su página; para ello el sd2psx cambia a esa tarjeta).
- Se actualiza desde las releases de este repositorio, verificado con el SHA-256 de GitHub. Dos canales, elegidos en
  los ajustes: **Estable**, las versiones publicadas, y **Beta**, una compilación hecha automáticamente con cada
  cambio, antes de probarse como una versión (la [pre-release beta](../../releases/tag/beta)).
- Interfaz inspirada en el PlayStation BB Navigator.
- Inglés, portugués, español, ruso, ucraniano y japonés. Las traducciones son bienvenidas: consulta
  [Translating](CONTRIBUTING.md#translating). Las partidas de juegos japoneses muestran sus títulos en japonés.

## Requisitos

- Un dispositivo de la familia sd2psx con el firmware sd2psXtd (compatible con MMCE). En una MemCard PRO2
  (experimental), SD2Cloud lee y escribe sus tarjetas `.mc2` en `/PS2`. Cambiar de tarjeta desde la aplicación
  ("Insertar esta tarjeta") es una vista previa allí, y SD2Cloud nunca cambia de tarjeta por su cuenta: para cambiar la
  tarjeta en uso, cambia primero a otra.
- Una forma de iniciar SD2Cloud: la pestaña Apps de OPL, wLaunchELF o cualquier otro lanzador que pueda ejecutar un
  ELF. Se recomienda un OPL compatible con MMCE, como [RiptOPL](https://github.com/NathanNeurotic/Open-PS2-Loader).
  Sin él, OPL no lista las aplicaciones del MMCE del sd2psx y no tiene la opción **IGR Bootcard Slot(s)**: con
  tarjetas Game ID, el sd2psx se queda en la tarjeta del juego al salir con IGR, la PS2 vuelve al navegador en lugar de
  abrir SD2Cloud y la sincronización automática no ocurre.
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
4. Conecta tu cuenta de Google cuando te lo pida (o más tarde, en los Ajustes): entra en google.com/device en tu
   teléfono o computadora, o escanea el código QR, y escribe el código que aparece en la TV.
5. Después SD2Cloud ofrece activar la sincronización automática y sincronizar todas tus tarjetas.

SD2Cloud también puede estar en una memoria USB, una tarjeta MX4SIO o el disco duro interno (exFAT o APA): pon allí
la carpeta `APPS/SD2Cloud` y abre SD2Cloud desde allí una vez. Registra dónde está, guarda en el MMCE
(`SD2Cloud/drivers`) los controladores que el paquete SAS necesita para iniciarlo desde ese dispositivo, y se actualiza
allí. Sus ajustes y sus datos siempre están en el MMCE.

**En la tarjeta de memoria: el paquete SAS.** Al activar la sincronización automática, SD2Cloud pone su paquete SAS
(Save Application System) en la tarjeta de memoria en uso: la carpeta `APP_SD2CLOUD`, con lo que OPL ejecuta en el IGR
y un acceso directo que abre SD2Cloud desde el MMCE, con su propio icono 3D para el navegador de la PS2 (unos 240 KB).
El programa en sí se queda en el MMCE. El zip también trae el paquete como `Extras/APP_SD2CLOUD.psu`, para importarlo a
mano a otra tarjeta (la propia pestaña **Archivos** de SD2Cloud lo hace); sin SD2Cloud en el MMCE, el acceso directo
solo lo avisa.

El [README.txt](package/README.txt) incluido en la release explica cada pantalla en detalle (en inglés).

## Controles

| Pantalla | Botones |
|---|---|
| Pantalla principal | Arriba/Abajo: tarjeta · Izquierda/Derecha o L1/R1: pestaña · X: abrir la tarjeta · TRIÁNGULO: opciones de la tarjeta (sincronizar ahora, restaurar una copia, copiar a un dispositivo, insertar en el sd2psx) · SELECT: utilidades · START: ajustes · O: salir |
| Dentro de una tarjeta | X: abrir una partida · TRIÁNGULO: opciones (sincronizar esta tarjeta, o subir la Memory Card entera a Drive; las partidas por nombre o por fecha; en Todas las partidas, repetidas ocultas o visibles) · O: volver |
| La página de una partida | Copiar · Mover · Eliminar · Subir a Drive |
| Después de Copiar | La partida queda seleccionada, y otras también pueden quedarlo: X selecciona o deselecciona una partida · O: salir de la tarjeta a la lista de tarjetas, para seleccionar partidas de otras tarjetas también · START: pegarlas todas en la tarjeta abierta, o en la que está bajo el cursor en la lista (donde no pueden ir, como en la tarjeta de la que vienen todas, va a la lista). En la pestaña Juegos de la lista, la carpeta de su juego aparece primero, resaltada, con "Nueva tarjeta" dentro (entre las carpetas, mientras el juego no tenga una): una tarjeta vacía de 8 MB creada para ellas, con las plantillas del juego. En la pestaña Archivos, X abre las carpetas de un dispositivo: START las pega ahí como archivos `.psu`, o dentro del archivo de tarjeta de memoria bajo el cursor; X en ese archivo lo abre, para seleccionar partidas suyas o pegarlas dentro con START. START en la Memory Card las pega en ella |
| Pestaña Archivos | X: abrir el dispositivo, una carpeta, un archivo `.psu` o un archivo de tarjeta de memoria · CUADRADO: instalar el `.psu` o la carpeta de partida seleccionados en una tarjeta, o el archivo de tarjeta seleccionado en el MMCE · Izquierda/Derecha: una página arriba o abajo · O: volver. Para exportar partidas, usa **Copiar** en la página de una: esta pestaña está entre los lugares donde se pueden pegar, y START escribe cada `.psu` en la carpeta mostrada |
| Utilidades (SELECT) > Plantillas | X: una plantilla nueva (se escribe su nombre; después se marcan sus partidas en las tarjetas: X marca, START termina) o una existente · En una plantilla: X: una partida, para quitarla · TRIÁNGULO (sobre una plantilla de la lista, o con ella abierta): fijar como principal, agregar partidas, aplicar plantilla (a todas las tarjetas de juego, a las tarjetas que elijas, o solo actualizando las partidas que las tarjetas de juego ya tienen), juegos vinculados, cambiar nombre, eliminar |
| Durante una sincronización | O: cancelar · CUADRADO: omitir la tarjeta que se está subiendo · SELECT: la lista de las tarjetas de la sincronización, con lo que pasó con cada una, y de vuelta. En ella, CUADRADO omite una tarjeta (otra vez: deshace) y X ofrece omitirla, marcarla como resuelta (la sincronización automática la deja en paz hasta que vuelva a cambiar) o desactivar su sincronización |

## Sincronización automática después de un juego (IGR)

1. En SD2Cloud, abre los Ajustes (START) y elige **Paquete SAS** (también se ofrece justo después de conectar tu
   cuenta). SD2Cloud muestra cuánto espacio ocupa (unos 240 KB) y pregunta antes de escribir la carpeta `APP_SD2CLOUD`
   en la tarjeta de memoria en uso; con Autoboot, instálalo mientras la BootCard está en uso.
2. En los ajustes de OPL, pon en **IGR Path** `mc0:/APP_SD2CLOUD/IGR.ELF` (`mc1:` con el sd2psx en la ranura 2:
   SD2Cloud muestra la ruta que hay que usar al instalar el paquete). Si una versión hasta la 1.5 dejó su asistente en
   la tarjeta (`mc0:/BOOT/SD2CLOUD-IGR.ELF`), esa ruta sigue funcionando: instalar el paquete también actualiza ese
   archivo.
3. En OPL, activa también **IGR Bootcard Slot(s)** (página MMCE), con la ranura del sd2psx o BOTH: al salir de un
   juego, OPL vuelve a la BootCard, donde está instalado el paquete.

Desde entonces, salir de un juego con IGR inicia SD2Cloud, que sube las tarjetas que cambiaron y vuelve a OPL. La misma
opción reinstala o desinstala el paquete. SD2Cloud puede estar en cualquier carpeta del MMCE: cuando no está en
`APPS/SD2Cloud`, registra dónde está en `sd2cloud.ini`, y el paquete lo inicia desde allí (también encuentra una
carpeta de `APPS` a la que se le cambió el nombre).

Para pausar la sincronización y dejar todo instalado, pon **Sincronización automática** en Desactivada en los Ajustes
(START): el IGR va entonces directo al programa que se abre después, sin iniciar SD2Cloud. Lo mismo ocurre mientras no
haya una cuenta de Google conectada.

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
