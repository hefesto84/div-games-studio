# Formatos de archivo propietarios

Este documento amplía, con lectura directa de las especificaciones [Kaitai Struct](http://kaitai.io/)
que viven en `formats/*.ksy` y del código fuente en `src/` que efectivamente lee o escribe cada
formato, la descripción de los formatos de archivo propios de DIV Games Studio 2. Los `.ksy` están en
YAML y son legibles como documentación de la estructura binaria; según `formats/README.md`, sirven
tanto para generar parsers automáticos con Kaitai Struct como de referencia para escribir rutinas
propias de lectura/escritura, y complementan (sin sustituir) la documentación más informal de la
[wiki del proyecto](https://github.com/vii1/DIV/wiki/Formatos-de-archivo).

Convención común a casi todos los formatos "clásicos" de DIV (`fnt`, `fpg`, `map`, `pak`, `pal`): un
`header` de 8 bytes formado por una extensión de 3 letras ASCII más la secuencia `0x1A 0x0D 0x0A`
(un patrón clásico de "detector de texto/EOF" de MS-DOS heredado de formatos como PCX) y un byte de
versión, casi siempre `0x00`. Esto se confirmó en el propio código: por ejemplo
`src/div/divforma.cpp` compara literalmente contra las cadenas `"fnt\x1a\x0d\x0a\x00\x00"`,
`"pal\x1a\x0d\x0a\x00\x00"` y `"map\x1a\x0d\x0a\x00\x00"` (8 bytes exactos), y `src/div/fpgfile.cpp`
usa `"fpg\x1a\x0d\x0a\x00\x00"`.

Muchos formatos gráficos (`fnt`, `fpg`, `map`, y el `pal` independiente) comparten además una
estructura de **paleta** de 256 colores RGB de 8 bits (768 bytes, formato "DAC" de VGA, valores
0-63 por canal, típico de los registros de paleta VGA que se multiplican por 4 para obtener 0-255 —
ver `create_dac4()` en `src/div/divpalet.cpp`, que hace `dac4[a]=dac[a]*4`) seguida de 16 "rangos" de
36 bytes cada uno (`n_colors`, `type` enum `direct/edit1/edit2/edit4/edit8`, `fixed`, `black`, y 32
bytes de índices de color) que DIV usa en su editor de paletas para animación cíclica de color y para
edición agrupada de rangos (sombreados, degradados).

---

## a3d — Animación de matrices para modelos 3D

**Estructura (`formats/a3d.ksy`).** Cabecera de 8 bytes: magia `"A3D",0`, `version` (s2), `n_objects`
(s2, número de objetos/huesos del modelo asociado), `n_anims` (s2) y un `dummy` (s2) de relleno. A
continuación, un array de `n_anims` registros `anim` (offset s4, n_frames s2, dummy s2) que apuntan
(vía `offset`) a bloques `frame`, cada uno formado por `n_objects` matrices de 4x4 floats (16 `f4`
= 64 bytes) — es decir, una matriz de transformación por objeto/hueso y por fotograma de la animación.

**Código que lo procesa.** `src/div/visor/fileanim.cpp` (`fileanim_create`) lee exactamente esta
estructura: `fread(&header, sizeof(a3d_header), 1, file)`, comprueba `strcmp(header.Chunk,"A3D")`,
reserva `nAnims` registros `a3d_anim` y luego calcula el desplazamiento acumulado de cada animación en
un buffer único de matrices (`nmatrix += nObjects * anims[x].nFrames`) antes de leer todas las
matrices de golpe — confirma que en la práctica los offsets de cada `anim` son contiguos aunque el
formato los declare como punteros absolutos independientes.

**Rol en el pipeline.** A3D no es un formato que use el lenguaje DIV directamente ni el motor VPE:
es el formato de animación (matrices de huesos/objetos) que consume el **visor** (`src/div/visor/`,
una herramienta interna de render 3D) junto con el modelo O3D correspondiente. `src/div/divspr.cpp`
(el "Generador de sprites", una utilidad del IDE) referencia rutas como
`GENSPR\HOMBRE\ANIM.O3D` / `GENSPR\HOMBRE\ANIM.A3D`: DIV trae una librería de modelos humanoides
prefabricados (hombre/mujer/enano) que este visor renderiza fotograma a fotograma, en varios ángulos,
para generar automáticamente los sprites 2D (FPG) de un personaje a partir de un modelo 3D animado.
A3D/O3D son por tanto formato *de autor/herramienta*, no formato consumido por los juegos compilados.

## o3d — Modelo 3D (objetos, materiales, dummies, cintas)

**Estructura (`formats/o3d.ksy`).** Cabecera con magia `"O3D",0`, `version` (s2), y contadores
`n_objects`, `n_materials`, `n_dummies`, `n_tapes`, `n_frames` (todos s2), seguidos de offsets
absolutos de 32 bits a cada tabla (`offset_objs`, `offset_mats`, `offset_dummies`, `offset_tapes`,
`offset_anms`) y una `bbox` (dos vectores f4 x,y,z = caja englobante global). Cada `obj` tiene su
propio conteo de vértices/caras, nombre (20 bytes), bbox local, vértices (posición+normal+UV) y caras
(3 índices de vértice + índice de material). `material` guarda color difuso/ambiental/especular (RGB
float), transparencia, nombre de textura (16 bytes) y un modo de renderizado enumerado (`wire, flat,
gouraud, phong, metal`). `tape` describe enlaces entre vértices de dos objetos (para animación de
"cintas"/deformaciones ligadas). `frame` (usado para animación embebida en el propio O3D, alternativa
a un A3D externo) repite una matriz 4x4 por objeto.

**Código que lo procesa.** `src/div/visor/complex.cpp` (`complex_create`) lee el formato campo a
campo en el mismo orden que el `.ksy`: `fread(cControl,1,4,file)` + `strcmp(cControl,"O3D")`,
versión, `nObjects`, `nMaterials`, `nDummies`, `nTapes`, `nFrames` (todos de 2 bytes), luego los 5
offsets de 4 bytes y los 6 floats de la bbox global, confirmando exactamente el orden y tamaño de
campos del `.ksy`.

**Rol en el pipeline.** Igual que A3D, es consumido por el visor/generador de sprites 3D→2D del IDE,
no por el runtime de los juegos. Sirve para pre-renderizar personajes 3D como sprites de un FPG.

## ifs — Fuente vectorial/multiresolución (fuente fuente de un FNT)

**Estructura (`formats/ifs.ksy`).** Cabecera `"IFS",0` con 5 offsets de 32 bits (`offset8`,
`offset10`, `offset12`, `offset14`, `offset128`) que apuntan a 5 tablas paralelas (`tabla_ifs`), cada
una de 256 entradas `{desp, size}` — una entrada por carácter del juego de 256 símbolos. Cada entrada
apunta a un glifo (`letra`): alto, `inc_y` (avance vertical/interlineado), ancho, un campo `pixels` y
el buffer de píxeles (`alto*ancho` bytes).

**Código que lo procesa.** `src/div/ifs.cpp` (`initStruct`, `CargaLetra`) confirma la estructura
exactamente: lee `IFSheader` completo con `fread(&IFSheader,sizeof(IFSheader),1,...)`, comprueba
`strcmp(IFSheader.id,"IFS")`, y según el tamaño de fuente pedido (`ifs.tamX`/`tamY`, entre 8 y 128)
elige uno de los 5 offsets (`offset8`/`10`/`12`/`14`/`128`) como tabla de glifos a usar — es decir, un
único fichero IFS contiene el mismo alfabeto pre-rasterizado en 5 resoluciones distintas (8x8 hasta
14x14, más una tabla especial "128" para el tamaño grande). `CargaLetra` después lee, por glifo,
`Alto`, `despY` (inc_y), `Ancho` y `pixels`, en ese orden — igual que `letra` en el `.ksy`.

**Rol en el pipeline.** IFS es un formato *intermedio de autor*: no lo carga el motor DIV ni el
lenguaje del juego. Es la fuente de datos que la herramienta de fuentes del IDE (el propio
`src/div/ifs.cpp`, cuya cabecera global usa la constante `FNTHEADER FNTheader = {"DIVFNT", 0x1A}`)
usa para generar un `.FNT` final rasterizado a partir de un alfabeto vectorial/multiresolución,
probablemente compartido con la wiki como "IFS = fuente de letras" usada por el editor de fuentes.

## fnt — Tipografía de mapa de bits

**Estructura (`formats/fnt.ksy`).** Cabecera de 8 bytes `"fnt",0x1A,0x0D,0x0A,0` + `version` (u1),
seguida de la paleta estándar de 256 colores + 16 rangos descrita arriba, un campo `charset` de 4
bytes interpretado como bitfield (bits `numbers`, `uppercase`, `lowercase`, `symbols`, `extended`, que
indican qué subconjuntos de caracteres incluye la fuente) y finalmente una tabla fija de 256 entradas
`fnt_table` (una por código de carácter): `width`, `height`, `inc_y` (avance vertical) y `offset`
(puntero absoluto al bitmap del glifo, de `width*height` bytes, un byte por píxel = índice de color
de paleta).

**Código que lo procesa.** `src/div/divforma.cpp` (`cargadac_FNT`) confirma el header de 8 bytes
exacto (`"fnt\x1a\x0d\x0a\x00\x00"`) y que justo después vienen los 768 bytes de paleta
(`fread(dac4,768,1,file)`). No se localizó en el código explorado la rutina exacta que escribe/lee la
tabla de 256 `fnt_table` con nombre reconocible en `src/div/divfont.cpp` (el módulo de edición de
fuentes del IDE, de 1871 líneas) — se deja constancia explícita de que **no se confirmó por lectura
directa de código el layout exacto de la tabla de glifos**, aunque el `.ksy` es consistente con el
mismo esquema de `fpg` (offset + tamaño) y con el uso que hace el instalador de un fichero FNT
comprimido embebido (ver sección `install`).

**Rol en el pipeline.** FNT es el formato de fuente bitmap que usa el lenguaje DIV (sentencias como
`WRITE`/`TEXT`) para dibujar texto con una paleta y un juego de glifos propios; también lo usan
internamente el instalador generado por DIV (fuentes pequeña/grande del asistente de instalación,
ver `install.ksy`) y el propio IDE.

## fpg — Paquete de gráficos (sprites) por código numérico

**Estructura (`formats/fpg.ksy`).** Cabecera de 8 bytes `"fpg",0x1A,0x0D,0x0A,0` + `version`, paleta
de 256 colores + 16 rangos (igual que `fnt`/`map`/`pal`), y a continuación una secuencia de registros
`map` hasta el final del fichero (`repeat: eos`): `code` (u4, el código numérico con el que el
lenguaje DIV referencia el gráfico, p.ej. `GRAPH=1`), `length` (tamaño total del bloque, incluida su
propia cabecera, útil para saltar al siguiente registro sin decodificar los píxeles), `description`
(32 bytes), `filename` de origen (12 bytes), `width`/`height`, `n_cpoints` (puntos de control) y su
array `cpoints` (pares x,y de 16 bits con signo — usados por DIV para puntos de anclaje/centros de
gráfico, referenciados desde el lenguaje) y finalmente los píxeles crudos (`width*height` bytes,
índices de color de 8 bits, **sin compresión**).

**Código que lo procesa.**
- `src/div/fpgfile.cpp` (`Crear_FPG`, `Abrir_FPG`, `ReadHeadImageAndPoints`) es la implementación de
  referencia en el IDE: escribe la cabecera con `fwrite("fpg\x1a\x0d\x0a\x00\x00",8,1,fpg)`, luego
  768 bytes de paleta (`dac`) y `sizeof(reglas)` bytes de rangos; `Abrir_FPG` valida el mismo magic de
  8 bytes y calcula `Fpg->OffsGrf[cod] = ftell(fpg)-FPG_HEAD` para indexar cada gráfico por su código.
  El struct `HeadFPG` (`fpgfile.hpp`) confirma campo a campo el orden de `map` en el `.ksy`
  (`COD, LONG, Descrip[32], Filename[12], Ancho, Alto, nPuntos`) y `FPG_HEAD` vale 64 (= 4+4+32+12+4+4+4,
  la suma exacta de esos campos).
- `src/install2/fpg.c` (`cargar_fpg`) es una segunda implementación, minimalista, usada por el
  **instalador generado** para leer en tiempo de ejecución el FPG embebido y comprimido con zlib
  dentro del propio `.EXE` de instalación: descomprime el buffer, valida el mismo magic de 7 bytes
  (`memcmp(...,"fpg\x1a\x0d\x0a",7)`), fija `paleta = installFpg + 8` y arranca a leer registros `map`
  en el offset `0x548` = 8 (cabecera) + 768 (paleta) + 576 (16 rangos × 36 bytes) — coincide
  exactamente con el tamaño de cabecera+paleta que describe el `.ksy`.

**Rol en el pipeline.** FPG es el contenedor central de gráficos/sprites de DIV: cada elemento gráfico
del juego (fondos, sprites, iconos de fuente del instalador, etc.) se referencia desde el código DIV
por su `code` numérico, y el intérprete lo busca en el/los FPG cargados. `install.ksy` reutiliza este
mismo formato (`imports: [fpg, fnt]`) para el FPG con los gráficos del propio instalador.

## map — Mapa de scroll (fondo de un solo bitmap con paleta propia)

**Estructura (`formats/map.ksy`).** Cabecera de 8 bytes (mismo patrón: `"map",0x1A,0x0D,0x0A,0` +
`version`) seguida directamente — a diferencia de `fpg`/`fnt`, sin un byte intermedio — por `width`
(u2), `height` (u2), `code` (u4) y `description` (32 bytes). Después va la paleta estándar de 256
colores + 16 rangos, un contador `n_cpoints` (u2) y su array de puntos de control, y finalmente el
bitmap de píxeles (`width*height` bytes, 8 bits por píxel).

**Código que lo procesa.** `src/div/divforma.cpp` (`cargadac_MAP`) confirma el magic de 8 bytes
(`"map\x1a\x0d\x0a\x00\x00"`) y, notablemente, lee `width`/`height` directamente de los bytes 8 y 10
del buffer de cabecera ya leído (`man=*(word*)(par+8); mal=*(word*)(par+10)`), confirmando que en el
fichero real `width` y `height` están pegados justo después de los 8 bytes de cabecera, sin campos
intermedios — igual que en el `.ksy`.

**Rol en el pipeline.** MAP es el formato de "mapa" en el sentido clásico de DIV: una única imagen de
fondo/nivel grande con su propia paleta (por eso incluye paleta embebida, como fpg/fnt), usada para
scroll de fondos en juegos 2D. Es distinto y no debe confundirse con WLD (mapas de escenario 3D del
motor VPE/Modo-8, ver más abajo) ni con FPG (que agrupa muchos gráficos pequeños, no uno grande).

## pal — Paleta de 256 colores independiente

**Estructura (`formats/pal.ksy`).** El formato más simple del grupo: cabecera de 8 bytes
(`"pal",0x1A,0x0D,0x0A,0` + `version`), 256 colores RGB (768 bytes) y 16 rangos de 36 bytes (mismo
esquema que en `fnt`/`fpg`/`map`).

**Código que lo procesa.** `src/div/divforma.cpp` (`cargadac_PAL`) valida el mismo magic de 8 bytes y
tiene además una ruta de compatibilidad: si el fichero **no** tiene el magic pero mide exactamente 768
bytes, lo trata igualmente como una paleta "pelada" (solo los 256 colores RGB, sin cabecera ni
rangos) — es decir, en la práctica DIV acepta dos variantes de `.PAL`: con cabecera completa (la que
describe el `.ksy`) y un `.PAL` "crudo" de exactamente 768 bytes.

**Rol en el pipeline.** Es la paleta de color activa del proyecto/juego (registros DAC de la VGA en
modo 256 colores); se puede cargar independientemente o extraer de cualquiera de los formatos que la
embeben (fpg, fnt, map, wld) mediante las mismas rutinas `cargadac_*` de `divforma.cpp`.

## pak — Paquete de datos (recursos empaquetados de un juego)

**Estructura (`formats/pak.ksy`).** Cabecera con magia de 7 bytes `"dat",0x1A,0x0D,0x0A,0` +
`version` (u1) = 8 bytes en total, seguida de 3 enteros `crc` (de 32 bits cada uno, hasta 3 CRC de
programas autorizados a usar el pack) y `num_files` (u4). Cada entrada `file` tiene: `filename` (16
bytes ASCIIZ, sin ruta), `offset` (posición absoluta del contenido dentro del pak), `compressed`
(tamaño almacenado) y `uncompressed` (tamaño real). El propio `.ksy` decide en tiempo de parseo si el
contenido está comprimido comparando `compressed < uncompressed`; si lo está, se decodifica con
`process: zlib`.

**Código que lo procesa.** `src/div/divinsta.cpp` (el generador de instalaciones del IDE, dentro de
`Setup0`/el bloque que arma `PACKFILE.DAT`) es la implementación de referencia y confirma el formato
campo a campo:
- Escribe la cabecera con `memcpy(dirhead.head,"dat\x1a\x0d\x0a\x00\x00",8)` (8 bytes, coincide con
  magia de 7 + versión) y después `fwrite(&dirhead.head,1,8+3*4+4,fout)` — 8 (head) + 12 (3 CRC de
  4 bytes) + 4 (num_files) = 24 bytes, igual que el `.ksy`.
- El struct `HeaderSetup` (`name[16]`, `offset`, `len1` comprimido, `len2` descomprimido) es
  exactamente `file_desc` del `.ksy` (`name`, `offset`, `z_size`, `u_size`).
- El nombre real del paquete se construye como `<NombreDelExe>.PAK` (`strcat(dirhead.pack,".PAK")`).

**Rol en el pipeline.** PAK empaqueta los recursos de distribución de un juego compilado (el propio
`.EXE`, `DIV32RUN.DLL`, `SETUP.EXE` opcional, y los ficheros de datos del proyecto) para reducir el
número de archivos sueltos y aplicar compresión. **Importante**: por lo investigado, el `.PAK` no lo
lee el juego en tiempo de ejecución — no se encontró en `src/div32run` (el reproductor de juegos) ni en
`src/wstub`/`src/div_stub` ningún código que abra un `.PAK` con esta cabecera `"dat"`. En su lugar, el
propio instalador generado (`src/install2/i.cpp` / `src/install/i.cpp`) descomprime cada entrada del
paquete a un fichero individual en disco durante la instalación (ver siguiente sección), de modo que
cuando el juego se ejecuta ya sólo hay ficheros sueltos, no un `.PAK` accedido en caliente.

## install — Instalador autocontenido generado (.EXE)

**Estructura (`formats/install.ksy`).** No es un formato "de cero"; describe cómo el generador de
instaladores de DIV (`divinsta.cpp`) *anexa* datos a un stub ejecutable de instalación. Declara
`imports: [fpg, fnt]` porque reutiliza esos dos formatos tal cual para los recursos gráficos y
tipográficos embebidos del propio instalador. El `.ksy` modela el fichero "desde el final": una
`instance` `tail` de 28 bytes situada en `_io.size - 28` con los tamaños comprimido/descomprimido del
FPG y de hasta dos FNT (`size_fpg_z/size_fpg`, `size_fnt1_z/size_fnt1`, `size_fnt2_z/size_fnt2`) y un
`data_size` total; y una instancia `install_data` situada en `_io.size - tail.data_size` con todos los
textos configurables del instalador (nombre de la app, copyright, directorio por defecto, mensajes de
error/ayuda/disco, etc., todos ASCIIZ) más flags (`create_dir`, `include_setup`, `segundo_font`) y los
propios FPG/FNT comprimidos con zlib embebidos al final.

**Código que lo procesa.** `src/div/divinsta.cpp` es el generador: construye interactivamente
(`Setup0`/`Setup1`/...) los textos (`AppName`, `Copy_Right`, `DefDir`, mensajes `Ierr0..Ierr8`, etc.
— exactamente los campos de `install_data`) y los datos de empaquetado (`dirhead`), y produce el
`.EXE` final concatenando el stub + estos datos, en línea con lo que codifica el `.ksy`. `src/install2`
y `src/install` (con `i.cpp` de más de 2000 líneas) son la contraparte que **lee** ese instalador ya
generado en la máquina del usuario final y ejecuta el proceso de instalación (mensajes, progreso,
extracción de ficheros).

**Rol en el pipeline.** Es el formato del ejecutable de instalación que un desarrollador de DIV puede
generar para distribuir su juego a usuarios finales sin requerir el propio DIV instalado; combina un
stub de instalación (interfaz gráfica genérica) con los recursos concretos (FPG/FNT) y los textos del
producto concreto.

## install_pak — Volumen de instalación multi-disco ("stp")

**Estructura (`formats/install_pak.ksy`).** Cabecera con magia de 6 bytes `"stp",0x1A,0x0D,0x0A,0`
más `not_final_vol` (u1) — 8 bytes en total — seguida de `num_files` (u4) y un array `file_desc` con
el mismo layout que `pak.ksy` (`name[16]`, `offset`, `z_size`, `u_size`), decidiendo compresión zlib
del mismo modo (`z_size < u_size`).

**Código que lo procesa — confirma exactamente el rol de `not_final_vol`:**
- **Escritura** (`src/div/divinsta.cpp`, función de grabación en disquetes, en torno a la línea 1291):
  cuando el instalador se genera para varios volúmenes/discos (`vols`), cada volumen (`NOMBRE.001`,
  `.002`, ...) empieza con `fwrite("stp\x1a\x0d\x0a\x00",1,8,fout)` (8 bytes: magia de 7 + 1 byte que
  arrancará en `0x00`). Si el volumen se llena a mitad de un fichero, el código hace
  `fseek(fout,7,SEEK_SET); fwrite("\x01",1,1,fout);` — es decir, **pone a `1` exactamente el último
  byte de la cabecera** (posición 7, el campo `not_final_vol`) para marcar "quedan más volúmenes".
- **Lectura** (`src/install/i.cpp`, en torno a la línea 1591): al reinstalar, por cada volumen lee 8
  bytes y compara `strcmp(cwork2,"stp\x1a\x0d\x0a")`; concatena el contenido de cada volumen (sin su
  cabecera de 8 bytes) en un fichero temporal `UNPACK.TMP`, y decide si debe pedir el siguiente disco
  mirando el mismo byte: `if (!cwork2[7]) break; // Control del volumen final`. El resultado
  reensamblado (`UNPACK.TMP`) es, en la práctica, un `pak` normal (empieza con `nfiles` + array de
  `HeaderSetup`/`file_desc` + contenidos), que luego se descomprime fichero a fichero con
  `descomprimir_fichero`/`copiar_fichero`.

**Relación con `pak.ksy`.** `install_pak` es, en esencia, `pak` con una magia distinta (`"stp"` en vez
de `"dat"`) y sin los 3 CRC de cabecera, más partido en volúmenes de tamaño limitado por el espacio
libre de cada disco de destino. El propio `.ksy` no declara un `imports: [pak]` explícito, pero
reimplementa localmente los mismos tipos `file_desc`/`compressed_file`/`raw_file` que `pak.ksy` —
duplicación consistente con lo observado en el código, donde ambos "paks" comparten el mismo struct
`HeaderSetup`/`_HeaderSetup` en C.

**Rol en el pipeline.** Es el mecanismo de instalación por discos (históricamente disquetes) cuando el
contenido total no cabe en un único volumen: el generador trocea el paquete de datos en tantos
ficheros `.001`, `.002`, ... como haga falta, cada uno autoidentificado con la cabecera `"stp"` y el
flag de continuidad, y el instalador los reconstituye en orden antes de extraer los ficheros finales.

## wld — Mapa de escenario 3D (motor VPE, "Modo 8")

Es el formato más complejo del conjunto y el único que combina, en un mismo fichero, **dos
representaciones distintas del mismo mapa**: una capa "de edición" (pensada para el editor de mapas
3D dentro del IDE) y una capa "compilada"/runtime (pensada para el motor VPE que ejecuta el juego).

**Estructura de edición (parte principal del `.ksy`, según `seq` de nivel superior).** Cabecera de 12
bytes: magia `"wld",0x1A,0x0D,0x0A,0x01,0` (8 bytes) + `offset_vpe` (u4, desplazamiento al bloque
runtime, ver abajo). Le siguen: `path` (256 bytes) y `name` (16 bytes) del propio mapa, `numero`
(s4), `fpg_path` (256 bytes) y `fpg_name` (16 bytes) — el WLD **no** incluye su propia paleta ni
gráficos: referencia por nombre un `.fpg` externo del que toma las texturas de paredes/suelos/techos
por código numérico —, y luego los arrays de edición: `points` (activo, x, y, links), `walls`
(activo, tipo, p1, p2, región frontal/trasera, textura superior/media/inferior, fade 0-16), `regions`
(activo, tipo, altura de suelo/techo, textura de suelo/techo, fade) y `flags` (activo, x, y, número),
cada uno precedido por su contador de 32 bits. Cierra con un campo `fondo` (s4).

**Estructura runtime (`vpe_map`, instancia en `offset_vpe + 12`).** Empieza con su propia mini-cabecera
`"DAT",0`, y contadores de 16 bits (`num_points`, `num_regions`, `num_walls`, `num_flags`) seguidos de
versiones "compiladas" y más compactas de los mismos elementos (`vpe_point`, `vpe_region`, `vpe_wall`,
`vpe_flag`, con campos de 16/32 bits ya orientados a lectura directa por el motor, más varios campos
marcados en el propio `.ksy` como constantes fijas — `siempre -1`, `siempre 0`, `siempre "NO_NAME"` —
que sugieren metadatos del editor sin usar aún en tiempo de ejecución) y datos generales de la vista:
título, nombre de paleta (`"paleta"`, 9 bytes), textura y ángulo de fondo, y una estructura `force`
(vector de fuerza global x,y,z,t, usada por el motor físico del VPE).

**Código que lo procesa — confirma el esquema de dos capas con precisión:**
- `src/vpe/load.cpp` (`LoadZone`) es la función del motor VPE que carga un `.wld` para jugar. Valida
  el magic de 8 bytes exacto del `.ksy`: `strcmp(&Buffer[Pos],"wld\x1a\x0d\x0a\x01")`. Acto seguido
  **salta toda la capa de edición** con una sola operación:
  `Pos += (8 + 4 + *(int*)&Buffer[8]);` — es decir, 8 (magic) + 4 (el propio campo `offset_vpe`) +
  el valor de `offset_vpe` leído del propio buffer — que es exactamente el cálculo `offset_vpe + 12`
  que describe la instancia `wld_vpe` del `.ksy`.
  A partir de ahí lee directamente los structs runtime (`ZF_Header`, `ZF_Point`, `ZF_Region`,
  `ZF_Wall`, `ZF_Flag`, `ZF_General`) con la macro `READ(ptr,type)`, en el mismo orden que declara
  `vpe_map` en el `.ksy` (puntos, regiones, paredes, banderas, datos generales).
  Confirma también el uso de texturas: `TexAlloc(&new_region->FloorTC, zr->FloorTex, num_fpg_aux)` —
  los códigos de textura de paredes/suelos/techos del WLD son códigos de gráfico dentro del FPG
  asociado (coherente con `fpg_path`/`fpg_name` de la cabecera de edición) — y de paleta:
  `LoadPalette(zgen->Palette)` usa el nombre de paleta embebido en `vpe_map`.
- No se encontró en el código explorado la rutina que **escribe** el `.wld` completo (probablemente
  vive en el editor de mapas 3D del IDE, cuyo código no forma parte de los ficheros revisados en esta
  pasada); esa parte se documenta aquí únicamente a partir del `.ksy` y de la lectura en runtime, sin
  confirmación cruzada de escritura.

**Rol en el pipeline.** WLD es el mapa de un escenario 3D (paredes, suelos/techos por región,
puntos y "banderas"/marcadores de spawn u objetos) para el motor **VPE** (Voxel/Portal Engine o
similar, "Modo 8" en la terminología de DIV, el modo gráfico de renderizado 3D del lenguaje). El hecho
de guardar la capa de edición junto a una capa runtime pre-"compilada" (offsets ya resueltos a
índices, campos ya convertidos a fixed-point donde aplica vía `INT_FIX`) sugiere que el propio editor
de mapas genera ambas representaciones a la vez, evitando que el motor tenga que reconstruir la
topología (enlaces punto-a-punto, qué paredes pertenecen a qué región) en cada carga.

---

## game_exe — Ejecutable de juego compilado (bytecode + stub)

**Estructura (`formats/game_exe.ksy`).** Un `stub` fijo de 602 bytes (el ejecutable base de
DIV/Watcom que sabe cargar el runtime), seguido de un bloque de cabecera de exactamente 40 bytes
interpretado como 10 enteros de 32 bits: `flags` (bitfield `program_flags`: `setup_program`,
`debug_at_start`, `ignore_errors`, `demo`, con varios bits `dummy` sin usar), `imem`, `imem2`,
`max_process`, `reserved`, `iloc1`, `iloc`, `reserved2`, `mem_size`, `len_descomp`. El resto del
fichero (`size-eos: true`) es un bloque **comprimido con zlib** (`process: zlib`) que contiene el
bytecode compilado del programa DIV (código, variables/procesos globales, cadenas de texto, etc.).

**Código que lo procesa — confirma la cabecera campo a campo, con matices.**
`src/div32run/i.cpp` es el intérprete/runtime (el "reproductor" de juegos DIV) y es quien realmente
carga este formato al arrancar un juego compilado:
- `fseek(f,602,SEEK_SET); fread(mimem,4,10,f);` — confirma exactamente el tamaño del `stub` (602
  bytes) y que la cabecera son 10 enteros de 4 bytes (40 bytes) tal como modela el `.ksy`.
- `len = ftell(f) - 602 - 4*10;` calcula el tamaño del bloque final, y ese bloque se pasa a
  `uncompress()` (zlib) usando `mem[9]` (`len_descomp`) como tamaño de salida esperado — confirma que
  el último campo de la cabecera y el resto del fichero son, respectivamente, el tamaño descomprimido
  y los datos comprimidos con zlib, tal como dice el `.ksy`.
- El código usa explícitamente por índice `mimem[3]` (para calcular cuánta memoria de procesos
  reservar, coherente con el nombre `max_process`) y `mimem[8]` (como `imem`, "fin de código/locales/
  textos") y `mimem[5]+mimem[6]` (como longitud combinada de variables locales públicas+privadas).
  **No se encontró uso explícito en el código explorado** de `mimem[0]` como bitfield de flags con
  ese detalle exacto de bits, ni de `mimem[4]`/`mimem[7]` (`reserved`/`reserved2`) — se documentan tal
  cual los nombra el `.ksy`, sin poder confirmar por código su semántica exacta; queda constancia
  explícita de esta limitación en vez de inventar un significado.

**Rol en el pipeline.** Es el resultado final de "compilar" un programa DIV: el compilador (en
`src/div/divc.cpp`/`divbin.cpp`, no explorados en detalle en esta pasada más allá de confirmar que no
contienen las cadenas de estos campos con esos nombres) concatena el stub ejecutable genérico con la
cabecera de 40 bytes y el bytecode comprimido; al ejecutarlo, el propio stub/runtime (`div32run`) lo
detecta, salta los 602 bytes, lee la cabecera y descomprime el resto a memoria para interpretarlo. Los
ficheros de recursos del juego (FPG, FNT, MAP, WLD, PAL) se mantienen como ficheros aparte en el mismo
directorio del `.exe` — no van embebidos dentro de este formato.

---

## Resumen de relaciones entre formatos

- `fpg`/`fnt`/`map`/`pal` comparten literalmente la misma paleta de 256 colores + 16 rangos, y las
  mismas rutinas de detección de cabecera (`cargadac_*` en `src/div/divforma.cpp`).
- `install.ksy` importa (`imports:`) `fpg` y `fnt` tal cual, porque el instalador generado embebe un
  FPG y hasta dos FNT comprimidos con zlib para dibujar su propia interfaz.
- `install_pak.ksy` no declara un `import` formal de `pak.ksy`, pero reimplementa el mismo esquema de
  directorio de ficheros (`file_desc`/`compressed_file`/`raw_file`) que `pak.ksy`, y en el código
  ambos comparten el mismo struct C (`HeaderSetup`/`_HeaderSetup`); la diferencia real es la magia
  (`"dat"` vs `"stp"`), la ausencia de los 3 CRC de cabecera, y que `install_pak` está pensado para
  partirse en varios volúmenes/discos (campo `not_final_vol`).
- `ifs` es la fuente (multiresolución, pre-rasterizada en 5 tamaños) a partir de la cual se genera un
  `fnt` final, según el propio nombre de la cabecera interna que usa la herramienta (`"DIVFNT"`).
- `a3d`/`o3d` son un par (modelo + animación) consumidos por el visor 3D interno (`src/div/visor/`) y
  la utilidad "Generador de sprites" (`divspr.cpp`), para producir sprites 2D (destinados a un `fpg`)
  a partir de personajes 3D — no los usa el runtime de los juegos (`div32run`) directamente.
- `wld` referencia por nombre un `fpg` externo para sus texturas (campos `fpg_path`/`fpg_name`) y
  contiene, dentro del mismo fichero, tanto los datos de edición como el bloque runtime (`vpe_map`,
  magia interna `"DAT"`) que consume el motor VPE en `src/vpe/load.cpp`.
- `pak`/`install_pak` no los lee el juego compilado (`game_exe`) en tiempo de ejecución: sirven solo
  para la distribución/instalación; el instalador extrae todo a ficheros sueltos antes de que el
  juego (que sí sigue el formato `game_exe`) llegue a ejecutarse.

---

## Archivos leídos durante esta investigación

**Especificaciones Kaitai Struct:**
- `formats/README.md`
- `formats/a3d.ksy`
- `formats/fnt.ksy`
- `formats/fpg.ksy`
- `formats/game_exe.ksy`
- `formats/ifs.ksy`
- `formats/install.ksy`
- `formats/install_pak.ksy`
- `formats/map.ksy`
- `formats/o3d.ksy`
- `formats/pak.ksy`
- `formats/pal.ksy`
- `formats/wld.ksy`

**Código fuente cruzado:**
- `src/div/fpgfile.cpp` y `src/div/fpgfile.hpp` (formato FPG, IDE)
- `src/install2/fpg.c` (formato FPG, lector embebido en el instalador generado)
- `src/div/divforma.cpp` (`cargadac_FNT`, `cargadac_PAL`, `cargadac_MAP`, `cargadac_FPG`: detección de
  cabeceras FNT/PAL/MAP/FPG)
- `src/div/divpalet.cpp` (gestión de paleta DAC/DAC4, uso de `cargadac_*`)
- `src/div/ifs.cpp` e implícitamente `src/div/ifs.h` (formato IFS y su relación con FNT)
- `src/div/visor/fileanim.cpp` (formato A3D)
- `src/div/visor/complex.cpp` (formato O3D)
- `src/div/divspr.cpp` (uso de O3D/A3D por el Generador de Sprites)
- `src/div/divinsta.cpp` (formatos PAK e install_pak/"stp", generación del instalador)
- `src/install/i.cpp` (lectura de PAK "stp" multi-volumen durante la instalación, extracción a disco)
- `src/vpe/load.cpp` (formato WLD, carga runtime del motor VPE, `LoadZone`)
- `src/div32run/i.cpp` (formato game_exe: lectura de cabecera de 40 bytes tras el stub de 602 bytes y
  descompresión zlib del bytecode)

No se encontró ni se leyó código que confirme la escritura completa del formato WLD (editor de mapas
3D) ni el detalle exacto de los bits de `flags`/campos `reserved` de `game_exe`; esas partes se
documentan a partir del `.ksy` y quedan señaladas explícitamente como no verificadas por código.
