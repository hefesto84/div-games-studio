# Prehistorik 2 — remake en DIV (proyecto personal)

Remake del juego "Prehistorik 2" (Titus Software, DOS 1993) hecho desde cero como programa DIV,
usando el runtime DIV portado a Windows 11 de este mismo repositorio. Proyecto personal, sin
ánimo de publicación ni distribución de assets originales.

## Estado

- [x] Estructura de carpetas creada.
- [x] Assets de referencia descargados desde https://pre2.mine.nu (fan site, no oficial):
      sprites (460 GIFs ya descomprimidos) y mapas de los 16 niveles PC en PNG.
- [x] Notas de formato de nivel/sprite/tile transcritas en `docs/format_notes.md`.
- [x] Esqueleto `.PRG` compila y ejecuta con el toolchain portado (`divc_port.exe` + `div32run_port.exe`).
- [x] Conversor `tools/gif2fpg.py`: empaqueta los 460 GIF en `assets/PRE2SPR.FPG` (formato FPG de DIV,
      deducido de `src/div/fpgfile.cpp` del propio port). Los 460 sprites comparten exactamente los
      mismos 16 colores RGBA (incluida la transparencia), así que usan una única paleta global sin
      pérdida de calidad. Verificado a ojo por el usuario: los 3 sprites de prueba se ven
      correctamente (colores y transparencia) al ejecutar `div32run_port.exe PRE2.div32`.
- [x] **Tenemos los ficheros originales del juego** en `raw/` (PRE2.EXE, LEVEL1-LEVELG.SQZ,
      UNION.SQZ, SPRITES.SQZ, BACK*.SQZ, etc). Descomprimidos con el DIET.EXE original (v1.45f,
      freeware, 1992) corriendo en DOSBox — ver `tools/decompress_diet.ps1`. Resultado en
      `assets/original/decompressed/` (32 de 39 ficheros; ver "Compresión ZIV pendiente" abajo).
- [x] `tools/parse_level.py`: parsea los 16 `LEVEL*.SQZ` reales (tilemap, propiedades de tile,
      puertas, bloques móviles, enemigos, secretos, items, plataformas, jefe Kong) a JSON en
      `assets/original/levels_parsed/`. Verificado: los offsets de cada sección encajan exactos
      (offset_siguiente = offset + tamaño, sin huecos) y los recuentos por nivel varían de forma
      creíble (p.ej. niveles bonus como LEVELA/LEVELB/LEVELC/LEVELD tienen 0 enemigos).
- [x] `tools/render_level.py`: decodifica los tiles (4 bits planar 16x16, propios de cada nivel +
      compartidos de `UNION.SQZ`) con la paleta correcta de `PRE2.PAL` (una de 10, según nivel) y
      renderiza el tilemap completo a PNG en `assets/original/levels_rendered/`. **Verificado**:
      el render de LEVEL1 es un calco exacto del terreno de `assets/original/lightmaps/L1.png`
      (la imagen de referencia del fan site) — confirma que el descifrado de tiles, la tabla de
      lookup y la paleta son correctos. Los 16 niveles renderizan sin errores.
- [x] Scroll real (`START_SCROLL` nativo de DIV) y colisión jugador-terreno con gravedad y salto,
      **confirmados funcionando por el usuario** dentro del propio DIV (no solo en PNG). Ver
      "Scroll real dentro de DIV" y "Colisión jugador-terreno" más abajo.
- [ ] Diseñar el formato de nivel propio para el motor (partiendo de los JSON ya parseados).
- [ ] Animación de jugador, enemigos, objetos, plataformas móviles.

## Estructura

```
prehistorik2/
  README.md              este fichero
  docs/
    format_notes.md       notas técnicas del juego original (tiles, niveles, sprites, enemigos...)
  raw/                    ficheros originales del juego (PRE2.EXE, *.SQZ, *.TRK...), sin tocar
  assets/
    original/
      sprites.zip          descarga original del fan site
      sprites_extracted/   460 sprites en GIF, numerados 001.gif..460.gif
      lightmap.zip          descarga original del fan site (mapas renderizados de los 16 niveles PC)
      lightmaps/            L1.png..L16.png, mapa visual de cada nivel (referencia de diseño, no tilemap crudo)
      decompressed/         *.SQZ de raw/ ya descomprimidos con DIET (ver tools/decompress_diet.ps1)
      levels_parsed/         LEVEL1.json..LEVELG.json, salida de tools/parse_level.py
      levels_rendered/        LEVEL1.png..LEVELG.png, salida de tools/render_level.py (solo terreno)
    PRE2.PAL                paleta original (10 paletas x 16 colores, BGR0), de pre2.mine.nu/techdocs
    SPRSIZE.BIN              tabla de tamaños de sprite (460 x [ancho,alto]), de pre2.mine.nu/techdocs
    PRE2SPR.FPG             sprites empaquetados para DIV (generado por tools/gif2fpg.py)
  src/
    PRE2.PRG               esqueleto del programa (compila y ejecuta, sin lógica de juego todavía;
                            de momento solo muestra 3 sprites de prueba cargados de PRE2SPR.FPG)
  tools/
    gif2fpg.py              conversor de assets/original/sprites_extracted/*.gif a assets/PRE2SPR.FPG
    decompress_diet.ps1     descomprime raw/*.SQZ con el DIET.EXE original vía DOSBox
    parse_level.py           parsea assets/original/decompressed/LEVEL*.SQZ a assets/original/levels_parsed/*.json
    render_level.py          renderiza el tilemap de un nivel a PNG (assets/original/levels_rendered/)
    diet_tool/
      diet145f.lzh           descarga original de https://ftp.vector.co.jp/00/03/527/diet145f.lzh
      extracted/DIET.EXE     herramienta freeware original (Teddy Matsumoto, 1992)
```

## Notas importantes sobre el toolchain

- Los ficheros `.PRG` de DIV deben estar en **CRLF** (saltos de línea DOS), no LF. El compilador
  portado (`divc_port.exe`) falla con un error críptico ("Error 10 ... Reemplazar") si el fichero
  tiene LF puro — el mensaje de error no tiene nada que ver con la causa real, es un bug/artefacto
  del compilador original al desbordar la tabla de mensajes. Si se edita un `.PRG` con una herramienta
  que normalice a LF, hay que volver a convertir a CRLF antes de compilar.
- Compilar: `divc_port.exe PRE2.PRG` (genera `PRE2.div32`) desde `build/Release/` (es el build "real"
  que usa también el IDE).
- Ejecutar: `div32run_port.exe PRE2.div32`.

## Formato FPG (referencia rápida para `gif2fpg.py`)

Cabecera: magic `fpg\x1a\x0d\x0a\x00\x00` (8) + paleta 256×RGB en rango 0-63 (768) + 16×36 bytes de
"reglas" del editor original, sin usar (576). Por cada sprite: `HeadFPG` de 64 bytes (COD, LONG,
Descrip[32], Filename[12], Ancho, Alto, nPuntos) + puntos de control (4 bytes cada uno, punto 0 =
centro) + píxeles (1 byte = índice de paleta, color 0 = transparente). Ver comentario de cabecera en
`tools/gif2fpg.py` y `src/div/fpgfile.cpp`/`fpgfile.hpp` del port para el detalle exacto.

## Compresión ZIV pendiente

DIET solo descomprime ficheros DIET. Seis ficheros del juego usan compresión ZIV en su lugar
(`CASTLE.SQZ`, `THEEND.SQZ`, `PRESENT.SQZ`, `KEYB.SQZ`, `SAMPLE.SQZ`, `TITUS.SQZ` — pantallas de
menú/logo y efectos de sonido, no datos de nivel) y de momento siguen comprimidos en `raw/`, DIET
los ignora sin avisar. `MENU2.SQZ` dio "CRC error" con DIET (el propio origen podría estar dañado)
y tampoco está en `assets/original/decompressed/`. Ninguno de los dos bloquea trabajar con los
niveles. Si hace falta SAMPLE.SQZ (efectos de sonido) más adelante, habrá que investigar el
descompresor ZIV aparte.

## Nota sobre valores "vacíos"

En los registros de longitud fija (puertas, bloques, secretos, items, plataformas) y en los
registros de enemigos que sobran hasta rellenar sus 2048 bytes, el juego rellena las entradas no
usadas con `0xFF`, no con ceros. `parse_level.py` filtra por eso comparando contra `0xFF`
(`_is_empty()`), y corta la lista de enemigos en cuanto encuentra un byte de longitud `0x00` o
`0xFF`. Si se toca este parser, ojo con ese detalle — la primera versión filtraba por cero y
contaba "enemigos" fantasma rellenos de `FF` hasta agotar el bloque.

## Próximos pasos sugeridos

1. Diseñar el formato de nivel propio para el motor DIV, partiendo de los JSON ya parseados en
   `assets/original/levels_parsed/` (no hace falta que sea binario idéntico al original, basta con
   la misma info: tilemap, propiedades de tile, enemigos, plataformas, secretos, puertas).
2. Empaquetar los tiles decodificados (`render_level.py` ya sabe leerlos) en algo que DIV pueda
   cargar — probablemente un FPG de tiles por nivel o uno global, igual que se hizo con los sprites
   de personajes en `gif2fpg.py`.
3. Prototipo del motor: scroll, colisión con tiles, jugador con salto/ataque.

## Demo de nivel dentro de DIV (no solo PNG)

`tools/leveltiles2fpg.py` + `tools/gen_level_demo.py` generan un `.PRG` que pinta un nivel real
usando procesos `TILE(x,y,graph)` (mismo patrón `PROCESS`+`GRAPH` ya validado con los sprites de
personajes). Por defecto usa la resolución original del juego, 320x200, centrada en la posición de
inicio del nivel (`start_x`/`start_y` del JSON parseado):

```
cd prehistorik2/tools
python leveltiles2fpg.py LEVEL1     # -> assets/original/LEVEL1_TILES.FPG
python gen_level_demo.py LEVEL1     # -> src/LEVELDEMO.PRG (320x200 por defecto)
```

Luego, desde `build/Release/` (copiando ahí `LEVELDEMO.PRG` y `LEVEL1_TILES.FPG`):

```
divc_port.exe LEVELDEMO.PRG
div32run_port.exe LEVELDEMO.div32
```

## Scroll real dentro de DIV, con el jugador como cámara

`tools/level_combined_fpg.py` + `tools/gen_scroll_demo.py` generan un scroll de verdad
(`START_SCROLL` nativo de DIV, patrón calcado de `build/Release/ZELDA.PRG`) en vez de un viewport
estático: fondo del nivel (todo el tilemap, una imagen grande) y los 460 sprites de personaje, y un
proceso `PLAYER` (`CTYPE=C_SCROLL; SCROLL.CAMERA=ID`) que hace de cámara, controlable con flechas.

```
cd prehistorik2/tools
python level_combined_fpg.py LEVEL1   # -> assets/original/LEVEL1_ALL.FPG
python gen_scroll_demo.py LEVEL1      # -> src/SCROLLDEMO.PRG (320x200)
```

Desde `build/Release/` (con `SCROLLDEMO.PRG` y `LEVEL1_ALL.FPG` copiados ahí):

```
divc_port.exe SCROLLDEMO.PRG
div32run_port.exe SCROLLDEMO.div32
```
Flechas para moverte, ESC para salir.

### Por qué un único FPG (fondo + sprites juntos)

Primer intento: fondo del nivel y sprites de personaje en dos FPG separados. Salió con los colores
del personaje rotos. Causa: el runtime de DIV solo mantiene **una** paleta global activa a la vez
— la del primer FPG que se carga (`load_fpg()` en `src/div32run/f.cpp`). El segundo FPG que se
carga "adapta" sus píxeles al color más parecido de esa paleta activa (`adaptar()`/`find_color()`),
no usa la suya propia. Segundo intento: meter los 16 colores del personaje en los índices 16-31 de
la paleta del fondo, para que el remapeo encontrara coincidencia exacta — probado por el usuario,
**seguía mal**. No se identificó la causa exacta (candidatos: `find_color()` se salta un índice
"protegido" `last_c1`; o el checksum `palcrc` no se compara como cabría esperar) y no vale la pena
seguir tirando de ese hilo sin depurador. **Solución que sí funciona**: `level_combined_fpg.py`
mete fondo y sprites en el MISMO fichero FPG — una sola paleta, sin adaptación cruzada de por
medio, cero ambigüedad. Si se necesita cargar dos FPG con paletas distintas en DIV más adelante,
mejor evitarlo o investigar esto más a fondo con calma.

**Ojo con los códigos de gráfico en FPGs generados**:
- `COD==0` es el terminador de la lista de gráficos del fichero (`load_fpg()`) — un gráfico con
  código 0 trunca en silencio el resto del FPG. `leveltiles2fpg.py` usa `valor_lookup + 1` por esto.
- `COD>=1000` también corta la lectura (`load_fpg()` usa un array `lst[1000]`). Por eso en
  `level_combined_fpg.py` los sprites de personaje van en `500+n`, no `1000+n`.

## Colisión jugador-terreno (gravedad, salto, paredes)

`level_combined_fpg.py` añade al FPG combinado una capa de colisión (COD 2): mismo tamaño en
píxeles que el fondo, 1 byte por píxel, 0=libre/1=sólido. Se consulta en tiempo real desde el
propio `.PRG` con `MAP_GET_PIXEL(fpg, 2, x, y)` — así no hace falta meter el tilemap en arrays de
DIV ni hacer división entera en el script. `gen_scroll_demo.py` usa esa capa para dar al proceso
`PLAYER` gravedad, salto (flecha arriba, solo si toca suelo) y bloqueo de movimiento contra el
terreno (se comprueban las dos esquinas del lado que avanza, antes de mover, para no atravesar
paredes/suelo).

**Qué cuenta como "sólido" — importante, no simplificar**: la definición correcta es la literal
del formato (`horizontal==1` o `vertical==1` o bit 0 de `misc_bits`, ver
`docs/format_notes.md`), **no** "cualquier tile con gráfico". Se probó esa simplificación primero
(parecía razonable, la roca de fondo "se ve" sólida) y el jugador aparecía completamente bloqueado
nada más arrancar: la mayoría de la roca visible en estos niveles es decorado de fondo puro sin
las propiedades de sólido activadas — solo una capa fina de "superficie funcional" bloquea de
verdad. Con la roca de fondo tratada como sólida, el punto de arranque del nivel (que está tallado
dentro de esa roca decorativa) quedaba enterrado. Verificar cualquier regla de colisión contra
datos reales de un nivel (columna de arranque, posición de enemigos que caminan por el suelo)
antes de darla por buena — aquí se detectó el patrón correcto en minutos una vez se comprobó.

Nota: `start_x`/`start_y` del nivel son la posición de los **pies** del jugador, no el centro del
sprite — `gen_scroll_demo.py` resta la mitad de la altura de la caja de colisión al arrancar.

## Bug de parseo conocido, sin arreglar (no bloquea nada de lo hecho hasta ahora)

En `parse_level.py`, los enemigos tipo 0 y 10 (`falls_then_walks`, `ground_spawner`) usan los
bytes 9-12 de su registro con un significado distinto al resto de tipos: 4 bytes sueltos
(x,y,ancho,alto de "el Área"), no dos enteros de 16 bits como X,Y normales. El parser los lee
igual que a todos los demás, dando coordenadas absurdas para esos dos tipos (se vio con un
`ground_spawner` de LEVEL1: x=8969, y=2072, muy fuera del nivel de 4096x784). Arreglar antes de
dibujar o mover enemigos de esos dos tipos.
