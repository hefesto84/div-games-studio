# Plan de lo que queda del port (post tag `1.1.0+mode8`)

> Foto fija a 2026-09-22, tras cerrar el motor de renderizado Modo-8 en
> tiempo real (§54 de `12-port-progreso.md`, tag `1.1.0+mode8`). Este
> documento no repite el estado ya conseguido (ver la tabla completa en
> `13-handoff.md`) — es la lista priorizada de lo que falta y en qué
> orden abordarlo.

## Dónde estamos

El núcleo del intérprete, el compilador, y el runtime para el DIV 2.01
"típico" (2D, Modo-7, Modo-8, vídeo FLI, sonido de efectos y tracker,
colisiones, pathfinding) están portados y **verificados con contenido
real** (juegos y programas de prueba reales, no solo "compila"). El IDE
tiene su ciclo completo cerrado: escribir código, compilar, probar con
recursos reales, y todos sus editores de recursos (FPG, mapas 2D y 3D,
paleta, sonido/PCM, ayuda, calculadora, generador de explosiones,
**generador de sprites**) funcionan.

Lo que queda es, en su mayoría, **trabajo grande y explícitamente
aparcado por decisión del usuario**, no huecos por sorpresa — con la
excepción de un puñado de flecos cosméticos/de endurecimiento menores.

## Plan priorizado

### 1. (Opcional, bajo esfuerzo) Cerrar flecos cosméticos sueltos

Ninguno bloquea nada; se pueden hacer en cualquier momento, aislados,
sin decisión de diseño previa:

- Tamaño de letra grande en la ventana de ayuda del IDE (§48.4 de
  `12-port-progreso.md`, hipótesis de doble escalado `big2` en
  `vuelca_help()` ya apuntada, sin confirmar).
- Doble-clic sobre el mapa del escritorio no entra en modo edición (hay
  que usar el menú `Mapas → Editar mapa`).
- Confirmar Modo-8 con un nivel "clásico" (interior con
  paredes/pasillos en vez de un curso de esquí acuático al aire libre) y
  de paso comprobar si el suelo/techo sale rotado 90° (indexado
  fila-mayor de `DrawFSpan`, ver §54.5).
- `ascii`/`scan_code` en el **runtime** (no el IDE) se quedan siempre a
  0 — no afecta a `key()` (lo más común en juegos), sí a la entrada de
  texto desde bytecode. La pieza a reutilizar si hace falta ya existe
  (`port/io/io_keyboard.c`, la misma cola de eventos que usa el IDE).
- ALT GR físico / acentos / `ñ` en el IDE sin confirmar con un teclado
  español real (el filtro está implementado, ver §29.6, pero solo
  probado con teclas sintéticas).
- Warnings `C4028` en `v.cpp` (inconsistencia `char*`/`byte*`
  preexistente en el original, documentada como inocua) y verificar que
  los builds `-DDEBUG` siguen enlazando tras `USE_LIBTYPE_SHARED`.

**Estimación**: horas por ítem, ninguno requiere diseño nuevo.

### 2. (Medio, sin decisión de diseño) Endurecimiento del compilador

`divc_port` compila el corpus completo de tutoriales (11/11) y juegos
reales grandes (STEROID, WSPORTX, TOKENKAI...), pero queda un pulido
opcional documentado en el backlog original: `PRIVATE`/`STRUCT`,
punteros y strings-dato extensivos, y siempre queda la superficie de
"algún juego real de 1999 usa una construcción rara del lenguaje que
nadie ha ejercitado todavía". No es una tarea con final claro — es
vigilancia continua a medida que se prueben más juegos reales.

### 3. Generador de sprites (GENSPR) — PORTADO Y VERIFICADO (2026-09-22)

`divspr.cpp` + `src/div/visor/` (~5.500 líneas, motor 3D propio con
ensamblador `t.asm`). Era el único hueco *funcional* grande que le
quedaba al IDE — sin él, un juego que dependa de sprites vectoriales
generados o retocados dentro del propio DIV no se podía editar (sí se
podía *jugar*, si ya trae sus gráficos precompilados en el `.fpg`).

**Cerrado en la misma sesión en que se investigó** (el usuario pidió
retomarlo justo después de la investigación de abajo), y **rematado del
todo el 2026-09-23** tras cerrar un doble `free()` real que crasheaba
al clicar la vista 3D (§55.7 de `12-port-progreso.md`). Detalle técnico
completo en `12-port-progreso.md` §55. Resumen: `Mapas → Generador de
sprites` abre el diálogo, renderiza el modelo 3D (textura + sombreado
Gouraud correctos), y aguanta clics/arrastres repetidos sobre la vista
3D además de aceptar/cancelar — verificado en caliente con capturas
(`tools/drive_ide.ps1` + `DIV_IDE_SHOT_EVERY`) y confirmado por el
usuario en el IDE real. Limitación heredada, no introducida por el port:
el `genspr\textura.pcx` que trae el repo es en realidad un JPEG con
extensión `.pcx` (mismo dato en las 3 copias del repo), y JPG sigue
stub en este port (decisión de E6a) — así que con el contenido
*tal cual viene*, el generador siempre da "No se reconoce el tipo de
fichero" al cargar la textura. Verificado que el resto del pipeline
(render 3D, texturizado, diálogo) funciona sustituyendo temporalmente
esa textura por un PCX real.

**Investigación 2026-09-22 (revisión a ojo, sin empezar el port):**
resulta más abordable de lo que parecía:

- `t.asm` (788 líneas) son en realidad solo 2 rutinas (con/sin máscara)
  desenrolladas a mano 6 veces, una por tamaño de textura de potencia de
  dos (8/16/32/64/128/256). Es un DDA de punto fijo de mapeado de
  texturas afín, mismo patrón que Modo-7/Modo-8 ya portado — trivial de
  reescribir en C como una única función parametrizada por `shift`.
- Los `_asm` inline de `llrender.cpp` (blending traslúcido/Gouraud)
  están **todos dentro de bloques `/* ... */` ya comentados** en el
  original — código muerto, no hay que portarlo.
- El resto (~5.200 líneas en `src/div/visor/` + `divspr.cpp`/
  `divsprit.cpp`) es C/C++ de la época sin sorpresas: matemática 3D
  estándar (rotación/proyección) en `complex.cpp`/`sprite3d.cpp`,
  parseo binario simple de los formatos propios `.o3d`/`.a3d`, y
  la integración GUI (`divspr.cpp`) usa el mismo estilo procedural de
  diálogos que ya se portó en otros editores del IDE (no hay
  MFC/OWL). `main.cpp` (con `<conio.h>`/`<i86.h>`/VESA) es un driver de
  demo standalone huérfano, no usado por la integración real del IDE —
  no hace falta tocarlo.

Conclusión confirmada al portarlo: no era "reescribir un motor 3D desde
cero", fue un port mecánico de tamaño medio-grande, de la misma
naturaleza que Modo-8, con el único fragmento de asm real siendo
trivial de traducir.

### 4. (Grande, fuera de alcance declarado) Loader de DLLs/plugins real

El pool de símbolos (`DIV_export`/`DIV_import`) ya es una implementación
real; falta un *loader* PE de verdad (`DIV_LoadDll` siempre falla limpio
hoy). Esto es Fase 4 del plan original del port
(`11-port-windows11-mikedx.md`) y nunca ha sido prioridad — la mayoría
de juegos DIV sueltos no dependen de DLLs de terceros. Retomar solo si
aparece un juego real que las necesite.

### 5. (Fuera de alcance, decisión ya tomada) CD-audio y red

Stubs inertes a propósito (`11-port-windows11-mikedx.md` §0.2). No hay
plan de retomarlos salvo cambio explícito de alcance.

### 6. (Pendiente, aplazado) Repo de release curado

Preparar un repo nuevo con solo lo necesario para "clonar y compilar"
(ver `project_div_release_goal` en memoria) — se dejó a medias el
análisis de qué ficheros mover tras el tag `1.0.0`. Puramente de
empaquetado/distribución, no afecta al estado funcional del port.

## Recomendación de orden

1. Los flecos cosméticos (§1) son gratis — hacerlos según vayan
   estorbando, no como bloque dedicado.
2. GENSPR (§3) **ya está cerrado** — era la pieza que de verdad movía
   la aguja de "¿está DIV Games Studio 2 completo?".
3. DLLs (§4), CD/red (§5) y el repo de release (§6) quedan pendientes
   salvo que un caso de uso concreto (un juego real que los necesite, o
   querer publicar el port) los adelante.

## Ver también

- [12-port-progreso.md](12-port-progreso.md) — bitácora completa,
  cualquier `§N` referenciado aquí vive ahí.
- [13-handoff.md](13-handoff.md) — tabla de estado pieza por pieza y
  punto de entrada para retomar trabajo.
