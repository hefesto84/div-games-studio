# Notas de formato — Prehistorik 2 (original DOS)

Fuente: https://pre2.mine.nu/techdocs.htm (recopilado por Jesses y Dorten)
Uso: referencia para el remake en DIV. Proyecto personal, sin ánimo de publicación.

## Gráficos

- Fondos y algunas pantallas (BACK*.SQZ, MENU2.SQZ, GAMEOVER.SQZ, MOTIF.SQZ): planar 4 bits, 320x200.
- Pantallas de menú/logo (MENU.SQZ, TITUS.SQZ, CASTLE.SQZ, THEEND.SQZ): 8 bits empaquetado, 320x200,
  paleta de 256 colores (768 bytes RGB, rango 0-63) en el offset 0, píxeles desde el offset 768.
- PRESENT.SQZ: además contiene una imagen 320x93.2 (fundido de la pantalla de título).
- LEVELH.SQZ + LEVELI.SQZ: foto de los desarrolladores (640x415, escala de grises, planos repartidos
  entre los dos ficheros).
- MAP.SQZ: mapa entre niveles, 640x200.

## Tiles y sprites

- Formato planar 4 bits, 128 bytes por sprite de 16x16 (32 bytes por plano x 4 planos).
- El ancho del sprite siempre es múltiplo de 8.
- Color transparente = 0 (composición AND/XOR).
- Dimensiones de cada sprite en SPRSIZE.BIN (2 bytes: ancho, alto).

## Compresión

- DIET (mayoría de ficheros): `DIET -R fichero.SQZ`. Herramienta: DIET 1.45f.
- ZIV (selectiva): TITUS.SQZ, CASTLE.SQZ, THEEND.SQZ, PRESENT.SQZ, KEYB.SQZ, SAMPLE.SQZ.

## Sonido

- SAMPLE.SQZ (comprimido ZIV): efectos de sonido. Tras descomprimir: RAW, 8000 Hz, PCM 8 bits con signo, mono.
- Música: ficheros .MOD estándar (*.TRK).

## Formato de nivel (tras descomprimir)

**Corrección 2026-09-23**: esta sección se reescribió leyendo el `techdocs.htm` completo sin pasar
por resumen de IA (la primera pasada tenía un error: las puertas son 20 registros × 7 bytes, no
7×20 — la tabla de abajo está verificada byte a byte, cada offset de sección coincide exactamente
con `offset_anterior + tamaño_anterior`). Fuente: https://pre2.mine.nu/techdocs.htm (Jesses y Dorten).

Todos los niveles miden 256 tiles de ancho; el alto varía por nivel y **no está en el fichero**,
solo en el ejecutable original:

| Nivel | 1  | 2   | 3  | 4  | 5   | 6   | 7   | 8  | 9   | A  | B  | C  | D  | E   | F  | G  |
|-------|----|-----|----|----|-----|-----|-----|----|-----|----|----|----|----|----|-----|----|----|
| Alto  | 49 | 104 | 49 | 45 | 128 | 128 | 128 | 86 | 110 | 12 | 24 | 51 | 51 | 38 | 173 | 84 |

(Verificado por aritmética contra el tamaño real de los ficheros descomprimidos en
`assets/original/decompressed/`: `tamaño = 256×alto + 512 + SmallNum×128 + 5029` cuadra exacto
para LEVEL1 y LEVEL2 con estos valores de alto.)

| Offset                          | Tamaño     | Contenido |
|----------------------------------|-----------|-----------|
| 0                                 | 256×alto  | Mapa de índices de tile (1 byte por tile) |
| +256×alto                         | 512       | Tabla de lookup de 256 entradas x 16 bits |
| +variable                         | SmallNum×128 | Bitmaps de tiles propios del nivel (4 bits, 16x16 = 128 bytes c/u) |
| tamaño_fichero − 5029             | 5029      | Resto de estructuras (ver tabla siguiente) |

Índices de tile: cada byte del mapa (256×alto de ellos) es un índice a la tabla de lookup (256
entradas de 16 bits, justo después del mapa). El valor de la tabla de lookup es el que importa:
- **< 256**: tile propio del nivel, bitmap en el propio fichero en offset `256×alto + 512 + valor×128`.
- **= 256**: transparente, se ve el fondo.
- **> 256**: tile compartido, bitmap en `UNION.SQZ` en offset `(valor−256)×128`.
- SmallNum = valor máximo entre los índices < 256 (para saber cuántos bitmaps propios hay).

Estructuras (offset relativo a `tamaño_fichero − 5029`; el nº de registros × tamaño de cada uno
está verificado: cada offset de la tabla = offset anterior + tamaño anterior, sin huecos):

| Offset      | Tamaño | Registros × bytes | Descripción |
|-------------|--------|---------------------|-------------|
| 0           | 768    | 256×3              | Propiedades de tile, bytes 1-3 (4º byte está en el offset 4031) |
| 768         | 2      | —                  | Desconocido (varía por nivel, ver tabla de curiosidades abajo) |
| 770         | 2      | —                  | Posición X inicial |
| 772         | 2      | —                  | Posición Y inicial |
| 774         | 1      | —                  | Límite de scroll horizontal, en tiles (sumar 20 para el ancho real) |
| 775         | 1      | —                  | Desconocido (siempre 0 en los niveles originales) |
| 776         | 1      | —                  | Flags de comportamiento del scroll |
| 777         | 512    | 256×2              | Máscaras de opacidad: nº de sprite de FRONT.SQZ que tapa parcialmente cada tile |
| 1289        | 140    | 20×7               | Puertas (in/out, posición de pantalla tras cruzar, si permite scroll) |
| 1429        | 150    | 15×10              | Bloques móviles ("shifting tile blocks", sin describir del todo) |
| 1579        | 2048   | variable           | Registros de enemigos (el primer byte de cada uno es su longitud total) |
| 3627        | 2      | —                  | Offset de sprite de objeto/plataforma: `real = guardado + 53 − offset` |
| 3629        | 2      | —                  | Offset de sprite de enemigo: `real = guardado + 312 − offset` |
| 3631        | 400    | 80×5               | Tiles secretos (tile origen, tile destino [sin uso real], bonus/vida, x, y) |
| 4031        | 256    | 256×1              | Propiedades de tile, byte 4 (pendientes) |
| 4287        | 490    | 70×7               | Objetos/items (incluye la salida del nivel) |
| 4777        | 240    | 16×15              | Plataformas móviles |
| 5017        | 11     | —                  | Configuración del jefe Kong |

### Registros de enemigos (offset 1579, 2048 bytes en total)

Cada registro empieza con un byte de longitud que incluye la cabecera fija (13 bytes) más los
bytes específicos del tipo. Cabecera fija: `length(1) type/expert(1) sprite(2) unknown1(1)
hitpoints(1) pause(1) unknown2(1) score(1) x(2) y(2)` = 13 bytes.

`type/expert`: bits 0-6 = tipo de enemigo, bit 7 = solo en modo experto.

| Tipo | Comportamiento | Longitud total | Bytes extra (offset relativo al inicio del registro) |
|------|-----------------|-----------------|--------------------------------------------------------|
| 0    | Cae del cielo, anda, gravedad; activo cuando el jugador entra en "el Área" | 13 | 9-10 = x,y de "el Área" (sustituye a X,Y); 11-12 = ancho,alto del Área |
| 1    | Decoración estática (telarañas) | 13 | — |
| 2    | Sube/baja por un hilo (arañas) | 15 | 13 = longitud máx. del hilo; 14 = velocidad |
| 3    | Genera arañas | 14 | 13 = distancia de activación (tiles) |
| 4    | Araña péndulo | 17 | 13 = radio; 14 = ángulo; 15-16 = FF? |
| 5    | Quieto hasta línea de visión, baja en diagonal y luego horizontal (abejas) | 16 | 13,14 = ancho,alto del rectángulo de visión (tiles); 15 = velocidad |
| 6    | Volador "listo" | 21 | 13 = distancia de activación (tiles); resto ¿FF? |
| 7    | Quieto hasta línea de visión, baja en diagonal | 15 | 13 = distancia de activación; 14 = velocidad |
| 8    | Salta (dinos, leopardos...) | 17 | 13 = distancia x de activación; 14 = potencia salto vertical (altura=VJP·(VJP+1)/2 px); 15 = potencia salto horizontal; 16 = distancia vertical de activación |
| 9    | Va izq-der ignorando paredes (según unknown1) | 19 | 13-14,15-16 = límites izq/der; 17 = FF?; 18 = velocidad |
| 10   | Sale del suelo, anda, vuelve a esconderse | 14 | 9-12 = como tipo 0; 13 = velocidad/comportamiento (1=quieto, 10=lento, 150=muy rápido) |
| 11   | Ardillas voladoras, varios puntos de spawn | 16 | 13 = velocidad horizontal; 14 = potencia salto inicial; 15 = velocidad vertical |
| 12   | Corredores rápidos (pingüinos, cavernícolas) | 15 | 13 = velocidad; 14 = FF? |

Tipos 3,4,6,7,11 mencionados antes como "intermedios" en la primera pasada de notas se corresponden
con esta tabla más detallada.

### Curiosidades (campo "desconocido" en offset 768, por nivel — no se usa para el motor)

| Nivel   | 1  | 2  | 3  | 4  | 5  | 6   | 7   | 8  | 9 | A   | B  | C  | D  | E | F | G   |
|---------|----|----|----|----|----|-----|-----|----|---|-----|----|----|----|---|---|-----|
| Valor   | 38 | 93 | 93 | 38 | 38 | 113 | 124 | 38 | 3 | 180 | 93 | 93 | 93 | 8 | 0 | 124 |

(offset 775, byte, es 0 en todos los niveles originales; no incluido aquí por no aportar nada)

### Propiedades de tile

- Byte 1 (horizontal): lado sólido (1), lado letal (2).
- Byte 2 (vertical): techo sólido (1), variantes resbaladizas (2-4), no-sólido al pulsar abajo (5), techo letal (6).
- Byte 3 (bitfield): suelo sólido (bit 0), suelo letal (bit 1), fuente de spawn (bit 4), interruptor de tile (bit 5),
  máscara de sprite (bit 6), flag de animación (bit 7).
- Byte 4 (pendiente): bits 0-3 = altura de la esquina izquierda invertida; bit 4 = pendiente baja-derecha;
  bit 5 = pendiente sube-derecha.

### Enemigos

Registros de longitud variable: tipo/flag experto (bits 0-6 tipo, bit 7 = solo modo experto), número de sprite,
puntos de vida, temporizador de pausa, valor en puntos, parámetros específicos del tipo.

Tipos: 0 = andarines con gravedad, 2 = arañas que trepan telarañas, 5 = línea de visión diagonal (abejas),
8 = saltarines (dinos, leopardos), 9 = andarines que ignoran paredes (pingüinos), 10 = generadores de enemigos,
12 = corredores rápidos (pingüinos, cavernícolas). Tipos 3,4,6,7,11 = comportamientos intermedios.

### Plataformas

Bits de dirección (0-2, brújula de 8 puntos), flag de caída (bit 3), auto-activación (bit 7),
velocidad, retardo de caída, distancia de recorrido.

## Recursos descargables del sitio

- SPRSIZE.BIN: tabla de dimensiones de sprites.
- PRE2.PAL: 10 paletas de 16 colores (formato B-G-R-0).
- Cadena de asignación de paletas: "0134568899222298"

## Assets ya descargados a este repo

- `assets/original/sprites.zip` → extraído en `assets/original/sprites_extracted/` (460 GIFs, 001.gif...460.gif,
  ya descomprimidos/legibles, no hace falta DIET). Tamaño típico ~40x36 aprox, paleta indexada.
- `assets/original/lightmap.zip`: mapas de los 16 niveles PC en brillo normal (PNG), útil como referencia visual
  para reconstruir el diseño de niveles.

## Pendiente de investigar/descargar

- Paletas de nivel (PRE2.PAL) y tabla SPRSIZE.BIN — no descargadas aún, están descritas pero hay que confirmar URL exacta.
- Música (.TRK / MOD) — ver /music.htm.
- Mapas por nivel para reconstruir el tilemap real (no solo la imagen renderizada) — probablemente no disponibles
  como datos crudos, solo como PNG en /levels/. Si se necesita el tilemap exacto habría que extraerlo de los .SQZ
  originales del juego (no distribuidos en este sitio) usando DIET y las tablas de arriba.
