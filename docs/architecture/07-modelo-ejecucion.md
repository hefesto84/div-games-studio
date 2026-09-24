# Modelo de ejecución

Este documento describe, con evidencia extraída directamente del código fuente, el camino completo que recorre un programa escrito en el lenguaje DIV desde que es texto en un `.PRG` hasta que corre como un ejecutable de MS-DOS. Cuando una afirmación no pudo confirmarse con certeza razonable leyendo el código disponible, se indica explícitamente en vez de darla por buena.

## Pipeline general (diagrama ampliado)

```
programa.PRG (texto fuente, lenguaje DIV)
        │
        │  divc.cpp (compilador embebido en el IDE D.EXE)
        │  - lexer dirigido por tablas cargadas de system\ltlex.def
        │  - parser recursivo descendente (sintactico()/sentencia()/expresion())
        │  - tabla de símbolos: array global obj[max_obj] + hash vhash[256]
        │  - generador de código con optimizador peephole (gen()/g1()/g2())
        ▼
mem[] (bytecode EML + tabla de textos, en memoria) + loc[] (plantilla de variables locales)
        │
        │  save_exec_bin() en divc.cpp:
        │  - cabecera de 9 enteros (mem[0..8])
        │  - concatena mem[9..] (textos+código) y loc[] en un solo bloque
        │  - lo comprime con zlib (compress())
        ▼
system\exec.pgm / system\EXEC.EXE (durante desarrollo)
o bien el .EXE final del juego (al "generar ejecutable" / instalar):
        │
        │  formato en disco: [div_stub: 602 bytes] [header: 10 enteros] [bytecode zlib]
        ▼
EXE autónomo (empieza con una cabecera MZ de 602 bytes = div_stub embebido en divc.cpp)
        │  al ejecutarse (en DOS real): el stub localiza y lanza DIV32RUN
        │  pasándole su propia ruta como argv[1]
        ▼
DIV32RUN(.EXE / .386 / .ins) — el runtime/VM
        │  main(argc,argv) en i.cpp:
        │  - abre argv[1], lee cabecera de 10 enteros en el offset 602
        │  - descomprime el resto con zlib uncompress() hacia mem[9..]
        │  - crea los procesos "init" y "main"
        ▼
interprete() → exec_process() → nucleo_exec()
        │  switch gigante por opcode, cuerpo en kernel.cpp (incluido literalmente
        │  dentro del switch de i.cpp)
        │  - opcodes de pila/aritmética/control de flujo
        │  - opcodes de gestión de procesos (lcal, lclo, lfrm, lfrf, lret, lrtf)
        │  - opcode lfun/lfunasp → despacha a function() en f.cpp (2º switch,
        │    la librería de "cientos de funciones" de DIV: gráficos, sprites,
        │    sonido, texto, scroll, modo-7/8, joystick, red...)
        ▼
Juego corriendo: bucle de frames sincronizado por PIT (divtimer.cpp/timer.asm),
procesos DIV cooperando mediante FRAME/FRAME_LOOP, dibujado delegado a un
backend de vídeo cargado como DLL (VPE) vía el mismo mecanismo de DLLs de DIV
```

## El compilador (`divc.cpp`)

`src/div/divc.cpp` tiene 7713 líneas y es, a la vez, el compilador y buena parte de la lógica de "Ejecutar"/"Depurar" del IDE (llama a las mismas rutinas al pulsar F9/F10). Sus fases:

### 1. Carga de tablas del lenguaje (no está hardcodeado en C)

Antes de compilar nada, `analiza_ltlex()` (línea 1275) abre `system\ltlex.def` y construye en memoria:

- Un **trie de caracteres** para símbolos y operadores multi-carácter: cada nodo es un `struct lex_ele { caracter; token; alternativa; siguiente; }` (línea 820), donde `siguiente` es el hijo (siguiente carácter de un símbolo) y `alternativa` es el hermano (otra opción en el mismo nivel). Esto es exactamente un autómata de reconocimiento de símbolos construido a mano, no un `switch` de casos.
- Una **tabla hash de palabras reservadas** (`vhash[256]`, hash calculado con `h=((h<<1)+(h>>7))^c`) que inserta las keywords del lenguaje (`PROGRAM`, `PROCESS`, `FRAME`, etc.) como entradas más en el mismo espacio de nombres que usarán después los identificadores del programa del usuario. Es decir: **las palabras clave del lenguaje DIV son datos externos, no literales en el código del compilador**, lo que sugiere que el mecanismo estaba pensado para poder cambiar de idioma/sintaxis sin recompilar (no se confirmó si esta posibilidad llegó a explotarse realmente; solo se confirmó el mecanismo).
- `precarga_obj()` (línea 1349) hace algo análogo pero para objetos predefinidos, leyendo `ltobj.def` y poblando directamente `obj[]` (comentario en el propio código: "no hay literales en los objetos precargados").

### 2. Lexer (`lexico()`, línea 1664)

Recorre el buffer fuente carácter a carácter usando la tabla `lex_case[256]` (un puntero por carácter ASCII que indica su clase: identificador `l_id`, dígito `l_num`, espacio `l_spc`, error `l_err`, delimitador de literal `l_lit`, o un nodo del trie de símbolos) para producir "piezas" (`pieza`, `pieza_num`) que son el token actual y su valor asociado.

### 3. Parser (`sintactico()`/`sentencia()`/`expresion()`)

Es un descenso recursivo clásico. Cosas confirmadas al leer el código:

- Hay **dos pasadas** sobre el código fuente: primero `psintactico()` (línea 1028, "pre-sintáctico") recorre el programa solo para calcular `longitud_textos` (cuánto espacio ocupará la tabla de textos/strings), y luego `sintactico()` hace la pasada real que genera código. Esto permite reservar el hueco para la tabla de textos en `mem[]` **antes** de emitir el bytecode, de modo que el código quede siempre después de los textos en el mismo buffer lineal.
- `struct exp_ele` (línea 811) es la pila de evaluación de expresiones usada por el parser (`tabexp[max_exp]`), con un campo `tipo` (constante/operador/rango/etc.) y una unión con el valor, el token o un puntero a objeto.

### 4. Tabla de símbolos (`struct objeto`, línea 711)

Es un array global `obj[max_obj]` de structs con un tag (`tipo`) y una **unión gigante** que cubre todos los tipos de símbolo posibles del lenguaje: constante, variable global/local (int/byte/word/string), tabla global/local, struct global/local, puntero a struct, proceso (con puntero al bloque y nº de parámetros), función interna (con código numérico, nº de parámetros y tipos), función externa (DLL). Cada símbolo enlaza con el anterior de mismo nombre (`anterior`, para scoping/shadowing) y con su bloque contenedor (`bloque`, 0 = global, N = privado de un proceso).

### 5. Generación de código y optimizador peephole (`g1()`/`g2()`/`gen()`, línea 7367+)

El código EML se emite con `g1(op)` (1 palabra) y `g2(op,pa)` (2 palabras: opcode + operando) directamente en `mem[imem++]`. Si `optimizar` está activo, en vez de escribir directamente se llama a `gen()`, que implementa un **peephole optimizer** con una ventana de las últimas instrucciones generadas (`code[16]`, comentario "en code[15] debe quedar siempre la última instrucción generada"). Ejemplos reales vistos en el código:
- Varias cargas de constante consecutivas (`lcar`) se funden en `lcar2`/`lcar3`/`lcar4` (cargar 2, 3 o 4 constantes con un solo opcode).
- `lasi`+`lasp` (asignación + descarte) se funde en `lasiasp`.
- `lcar`+`lasi` se funde en `lcarasiasp`.

Esto confirma y concreta lo que ya sugería el bloque de comentarios de `inter.h` ("Instrucciones añadidas para la optimización (DIV 2.0)"): esos opcodes no son instrucciones distintas del lenguaje, son *superinstrucciones* generadas por este optimizador para acortar secuencias comunes.

### 6. Salida (`save_exec_bin`, ver siguiente sección)

Tras compilar, `mem[0]=program_type; mem[1]=imem;` (línea 4055) fija los dos primeros campos de la cabecera, y al final de `compilar()` (línea 1046) se fijan el resto:

```
mem[2]=imem;                 // longitud usada de mem[] hasta el final del código+textos
mem[3]=max_process;          // pista de nº máximo de procesos concurrentes (0 = automático)
mem[4]=0;                    // sin uso actual
mem[5]=iloc_len-iloc;        // longitud de variables privadas
mem[6]=iloc;                 // longitud de variables públicas/locales
mem[7]=0;                    // sin uso actual
mem[8]=imem+iloc;            // tamaño total de la plantilla de proceso
```

## Formato de bytecode y datos generados

El bytecode (llamado "EML" en los comentarios del propio código) es una secuencia lineal de enteros de 32 bits en `mem[]`: cada instrucción es un opcode de 1 byte (`(byte)mem[ip++]` al leerlo, aunque se almacena en una celda de 4 bytes) seguido, según el opcode, de 0, 1 o más operandos también de 32 bits (offsets absolutos en `mem[]`, valores constantes, códigos de función, etc.). No hay separación entre "segmento de código" y "segmento de datos": el buffer `mem[]` contiene, en este orden, la tabla de textos (reservada primero para conocer su tamaño), luego el código bytecode del programa, y estas dos partes conforman lo que el compilador llama simplemente "el programa"; aparte, en un array separado `loc[]`, se genera la **plantilla de variables locales** de un proceso (los valores iniciales que tendrá cualquier instancia nueva de proceso al crearse).

Al guardar a disco (visto en dos sitios idénticos de `divc.cpp`, líneas 1070-1098 para `install\setup.ovl` y 1105-1134 para `system\EXEC.EXE`):

1. Se escriben los 602 bytes de `div_stub` (el array de bytes definido en `src/div/div_stub.h`, que arranca con la firma MZ de un ejecutable DOS: `77,90,...`).
2. Se escriben los primeros 9 enteros de `mem[]` (la cabecera, `mem[0..8]`) sin comprimir: 36 bytes.
3. Se concatenan en un buffer temporal `p`: el resto de `mem[]` (`mem[9..imem-1]`, es decir textos+código) seguido de todo `loc[]` (la plantilla de variables locales/privadas).
4. Ese buffer se comprime con `compress()` de zlib.
5. Se escribe la longitud descomprimida (`n`, 4 bytes) y luego los bytes comprimidos.

Esto da el formato final en disco: **`[602 bytes de stub][36 bytes de cabecera = 9 enteros][4 bytes = tamaño descomprimido][datos comprimidos con zlib]`**, para un total de cabecera fija de `602+40=642` bytes antes del bloque comprimido — cifra que efectivamente coincide con lo que lee `main()` en `i.cpp` (ver más abajo: `fseek(f,602,...)`, lee 10 enteros, y `len=filesize-602-40`).

Nótese que el campo `mem[0]` no es solo `program_type`: más adelante en la compilación se le suman banderas a nivel de bits (`mem[0]+=1024` para builds "SHARE"/demo, `mem[0]+=128` si `ejecutar_programa==3` — modo traza —, `mem[0]+=512` si se ignoran errores). El runtime las vuelve a separar restándolas (ver `i.cpp` línea ~1351).

## El stub y el mecanismo EXE autónomo

Aquí hay un matiz importante que **no coincide exactamente** con la simplificación de "se concatena el bytecode a `wstub.exe`":

- `src/div/div_stub.h` contiene un array de bytes (`div_stub[]`) que es literalmente el binario ya ensamblado de un ejecutable DOS de 602 bytes (cabecera MZ, luego instrucciones de 16 bits con llamadas `int 21h`/`int 10h` reconocibles en los bytes). El compilador **no genera este stub en tiempo de compilación**: lo tiene precompilado y embebido como constante.
- El código fuente correspondiente a ese binario debería ser `src/div32run/wstub/wstub.c`, pero en el estado actual del repositorio ese archivo son literalmente 5 líneas con un `main(void){}` vacío (ligado además con `cpuid.obj` según `wstub.lnk`). Hay una segunda copia, `src/wstub/wstub.c` (169 líneas), pero esa es un programa completamente distinto: es el lanzador del **propio IDE** (`chdir_to_div`, `spawn_process` de `SYSTEM\DOS4GW.EXE`/`SESSION.DIV`/`EXEC.EXE`, detección de si DIV ya está corriendo, etc.), no el stub de 602 bytes que se antepone a un juego compilado.
  **Conclusión honesta:** el código fuente real que generó los 602 bytes de `div_stub` no está identificado con certeza en este árbol; probablemente se perdió o quedó fuera de la reconstrucción del repo, y lo único que sobrevive es el binario ya compilado embebido en `div_stub.h`. No se hizo un desensamblado completo instrucción a instrucción (está fuera del alcance razonable de esta investigación), así que su comportamiento exacto se infiere por el **uso** que hace de él el runtime, no por lectura directa de su código: dado que `i.cpp` espera recibir como `argv[1]` la ruta del propio fichero que contiene el stub+bytecode (ver siguiente sección), y que el propio EXE final es autoejecutable en DOS, el stub tiene que, como mínimo, (a) obtener su propia ruta de invocación, y (b) lanzar el intérprete DIV32RUN pasándole esa ruta como parámetro, probablemente vía `INT 21h, AH=4Bh` (EXEC de DOS) tal como hace el lanzador del IDE en `src/wstub/wstub.c` con `spawnvp`.
- El mismo `div_stub` de 602 bytes se usa tanto para `system\EXEC.EXE` (el ejecutable temporal que usa el IDE para "Ejecutar"/depurar, ver `SESSION=1` más abajo) como para `install\setup.ovl` (usado por el instalador, `program_type==1`). Es decir, **es el mismo stub siempre**; lo único que cambia entre "modo prueba desde el IDE" y "juego instalado" es qué intérprete lo recibe como argumento y con qué banderas.

## Arranque de DIV32RUN

El punto de entrada real está en `void main(int argc, char *argv[])` en `src/div32run/i.cpp` (línea 1260):

1. `argc<2` → error ("Needs a DIV32RUN executable to load"): DIV32RUN **siempre** se invoca con un parámetro, la ruta al fichero que contiene stub+bytecode. Esto confirma que DIV32RUN es un runtime genérico, no el propio juego.
2. Abre `argv[1]`, calcula `len = tamaño_fichero - 602 - 40` y lee, en el offset 602, un bloque de 10 enteros (`mimem[10]`) — el "header" descrito arriba más un décimo entero (la longitud descomprimida, ver formato anterior).
3. Calcula `imem_max` (tamaño del "espacio de memoria" simulado, el array `mem[]` del runtime) a partir de `mimem[3]` (la pista de "número máximo de procesos" fijada por el compilador): si es mayor que 0, dimensiona exactamente para ese número de procesos; si es 0, usa una heurística automática acotada entre 256 KB y 512 KB.
4. Reserva `mem` con `malloc`, copia la cabecera de 10 enteros a `mem[0..9]`, reserva un buffer temporal, lee el resto del fichero y lo **descomprime con `uncompress()` de zlib directamente sobre `&mem[9]`** — es decir, el bytecode y los textos quedan cargados en el mismo array de memoria que usará la VM para ejecutar, sin una copia intermedia a otro formato.
5. Aplica las banderas de `mem[0]` (traza, ignorar errores, demo) y las quita del propio campo.
6. Copia los argumentos de la línea de comandos del juego a una zona alta de `mem[]` (`_argv(n)`, hasta 10 argumentos) para que el programa DIV pueda leerlos.
7. Llama a `kbdInit()` y luego a `interprete()` (el bucle principal, ver más abajo).

### Resolución de DLLs

DIV tiene un mecanismo de "DLL" propio, pero importante: **son DLLs de Windows en formato PE real**, cargadas a mano bajo DOS/extensor mediante un **loader de PE escrito desde cero** (`src/pe_load.c`, con sus propias definiciones de `IMAGE_DOS_HEADER`/`IMAGE_FILE_HEADER`/etc., ya que no hay un loader de PE nativo disponible bajo DOS4G/PMODE-W). La carga real está en `src/divdll.c`:

- `DIV_LoadDll(name)` → `Internal_LoadDll(name, 1)`: lee el PE con `PE_ReadFN`, busca en sus símbolos exportados una función `divmain_`/`_divmain`/`W?divmain` (las tres variantes cubren distintas convenciones de *name mangling* de distintos compiladores) y la ejecuta pasándole dos callbacks, `DIV_import` y `DIV_export`, que son la API con la que la DLL intercambia funciones y variables con el runtime.
- Si además exporta `divlibrary_`, se la llama con `COM_export` — esto es lo que usa `LookForAutoLoadDlls()` (`src/div32run/dll.c`) para autocargar cualquier `*.DLL` presente en el directorio del juego al arrancar, descartando (`DIV_UnLoadDll`) las que no declaren la marca `"Autoload"`.
- El opcode `limp` de la VM (ver `kernel.cpp` línea 247) hace exactamente lo mismo pero para una DLL nombrada explícitamente en el código DIV (`import "nombre.dll"`), guardándola en `pe[nDLL]`.
- El opcode `lext` llama a una función exportada por una DLL ya importada a través de la tabla `ExternDirs[]`, poblada por `CMP_export`.

**Hallazgo no evidente y relevante:** el propio backend gráfico "de serie" de DIV32RUN (fijar modo de vídeo, pintar sprites, gestionar paleta, componer el framebuffer) **no está compilado dentro de `i.cpp`/`kernel.cpp`**, sino que se resuelve como punteros a función (`set_video_mode`, `put_sprite`, `background_to_buffer`, `post_process_scroll`, etc., declarados `GLOBAL void (*...)()` en `inter.h`) que se importan con `DIV_import(...)` la primera vez que se llama a `frame_end()` (`i.cpp`, bloque `if (!dll_loaded)`, línea ~822). Es decir, usa el mismo mecanismo de DLL que las extensiones de usuario. Los fuentes bajo `src/div32run/vpe/` (`draw_cw.cpp`, `gfx.cpp`, `hard.cpp`, etc., compilados en el propio makefile de `div32run`, no como DLL externa real) son casi con certeza la implementación de ese backend ("VPE"), pero **no se investigó en profundidad su código interno**; solo se confirmó que expone su funcionalidad a través del mismo mecanismo `divlibrary`/`DIV_export`/`DIV_import` en vez de ser llamado directamente.

## La VM / el switch de kernel.cpp

`src/div32run/kernel.cpp` (699 líneas) **no es un archivo compilable por sí solo**: es literalmente el cuerpo de un `switch`, incluido con `#include "kernel.cpp"` dentro de dos funciones distintas de `i.cpp` (línea 570 y línea 632), cada `case` corresponde a uno de los opcodes definidos como `#define lXXX N` en `inter.h` (línea 154+, del `0` al `126`, con los códigos `127..255` reservados y sin usar).

Confirmado leyendo `nucleo_exec()` (`i.cpp`, línea 566):

```c
void nucleo_exec() {
    do {
        switch ((byte)mem[ip++]){
            #include "kernel.cpp"
        }
    } while (1);
    next_process1: mem[ide+_Executed]=1;
    next_process2: ;
}
```

Es decir, hay un único punto de despacho (`ip` = puntero de instrucción dentro de `mem[]`) y el `switch` se re-ejecuta en un `do/while(1)` infinito: la única forma de "salir" es con un `goto next_process1`/`next_process2` disparado desde dentro de algunos `case` (los que terminan la ejecución del proceso actual: `lret`, `lfrm`, `lrtf`, `lfrf`, cuando el `id` del siguiente proceso a ejecutar resulta "impar" — ver más abajo el significado de eso). El mismo `kernel.cpp` se reincluye una segunda vez con `#define TRACE` para `nucleo_trace()`, la variante usada por el depurador para ejecutar **una sola instrucción** y devolver el control (paso a paso).

Categorías de opcodes confirmadas leyendo el archivo completo:

- **Aritmética/lógica de pila:** `ladd`, `lsub`, `lmul`, `ldiv`, `lmod`, `lneg`, `lori`, `lxor`, `land`, `lnot`, `lshr`, `lshl`, comparaciones (`ligu`, `ldis`, `lmay`, `lmen`, `lmei`, `lmai`).
- **Acceso a memoria/variables:** `lcar` (carga constante), `lptr`/`lasi` (leer/escribir en una dirección de `mem[]`), variantes especializadas para bytes (`*chr`) y words (`*wor`) que además hacen aritmética de punteros dentro de `memb`/`memw` (los mismos bytes de `mem[]` reinterpretados como `byte*`/`word*` — así se implementan arrays de bytes/words sin duplicar memoria).
- **Cadenas:** un bloque completo de opcodes (`lstrcpy`, `lstrcat`, `lstradd`, `lstrdec`, `lstrsub`, `lstrlen`, comparaciones `lstrigu`/`lstrdis`/`lstrmay`/...) con lógica no trivial: un entero `<256` se trata como un carácter (`"%c"`), y hay una zona de "strings al aire" (`nullstring[4]`, buffer circular de 4 ranuras) para resultados intermedios de concatenación que no tienen una variable donde vivir.
- **Control de flujo:** `ljmp`, `ljpf` (salto condicional), `lcse`/`lcsr` (comparación de `switch`/rango de `switch`).
- **Opcodes fusionados por el optimizador** (ver sección del compilador): `lcar2/3/4`, `lasiasp`, `lcaraid`, `lcarptr`, etc.
- **Gestión de procesos:** ver sección siguiente.
- **Puente a la librería de funciones:** `lfun`/`lfunasp` llaman a `function()` (definida en `f.cpp`), que es un **segundo `switch` gigante** indexado por un código numérico (`v_function=mem[ip++]`, `f.cpp` línea 4218: `case 0: _signal(); case 1: _key(); case 2: load_pal(); ...`). Es en `function()`, no en `kernel.cpp`, donde viven las "cientos de funciones runtime" de gráficos/sprites/sonido/texto/scroll/modo-7/8/joystick/red que menciona la descripción del pipeline: `kernel.cpp` es solo el núcleo de bajo nivel de la VM (pila, aritmética, procesos), la librería de comandos DIV es una capa aparte.
- **Depuración:** `ldbg` comprueba breakpoints y, si `DEBUG` está activo, invoca `deb()`; `lchk` valida que un identificador de proceso siga vivo; varias comprobaciones (`ldiv`/`lmod`/división por cero, `lrng` rango de un `switch`, `lnul` puntero nulo) solo existen bajo `#ifdef DEBUG` — en el build de distribución esas comprobaciones **desaparecen del binario**, no solo se desactivan.

## Modelo de procesos y concurrencia

Cada instancia de un `PROCESS` de DIV (incluyendo el proceso implícito `main`, y un proceso raíz `init` que es padre de todos) es un **bloque de enteros dentro del mismo array `mem[]`**, de longitud fija `iloc_len` (calculada en compilación), con un layout definido por constantes `#define _Campo N` en `inter.h` (línea 420+): `_Id`, `_Status` (0 muerto / 1 recién matado / 2 vivo / 3 dormido / 4 congelado), `_NumPar`/`_Param` (parámetros), `_IP` (instrucción a la que volver), `_SP` (pila propia guardada, ver abajo), `_Frame` (contador de `FRAME(n)`), `_Father`/`_Son`/`_SmallBro`/`_BigBro` (árbol genealógico de procesos, con listas enlazadas de hermanos), `_Priority`, `_X`/`_Y`/`_Z`/`_Graph`/`_Angle`/... (los campos gráficos que el propio lenguaje DIV expone como `x`, `y`, `graph`, etc. de cada proceso).

No hay heap dinámico de "objetos": crear un proceso nuevo es buscar un hueco libre (`_Status==0`) en un rango contiguo `[id_start, id_end]` de `mem[]` (o extender `id_end` si no hay huecos) y copiarle encima la plantilla `loc[]` generada por el compilador. Esto se ve en el opcode `lcal` (`kernel.cpp` línea 80): incrementa un contador global `procesos`, busca el hueco, hace `memcpy` de la plantilla pública de variables (`iloc_pub_len<<2` bytes), enlaza el nuevo proceso en el árbol genealógico de su creador, y decide si es una llamada a función normal (mismo "frame"/turno) o una creación real de proceso — el criterio, algo peculiar, es mirar si la instrucción justo después del salto es un `lnop`: si lo es, se cuenta como llamada a función (`_FCount++`) en vez de como proceso nuevo.

**Scheduler round-robin por prioridad**, confirmado en `exec_process()` (`i.cpp`, línea 516): en cada "turno" recorre circularmente todos los procesos desde `id_old` (el último procesado) buscando el de mayor `_Priority` entre los que están vivos y no ejecutados aún en este frame (`_Status==2 && !_Executed`); ese es el elegido (`ide`). Si su `_Frame` acumulado ya llegó a 100 (ver `FRAME`/`FRAME_LOOP` más abajo) se le marca como ejecutado sin correr ni una instrucción; si no, se restaura su pila (`carga_pila`) y se ejecuta bytecode real (`nucleo_exec()`) hasta que el proceso "cede el turno". Un proceso cede el turno con alguno de estos opcodes:

- `lfrm`/`lfrf` (`FRAME`/`FRAME(%)`): suspende el proceso hasta el próximo frame de pantalla. `lfrf` es la versión "porcentual" (acumula en `_Frame` hasta llegar a 100 antes de ejecutarse de nuevo — así se implementa correr a "medio frame" o a fracciones de la velocidad base).
- `lret`/`lrtf`: terminación (auto-eliminación) del proceso, con o sin valor de retorno.
- `lclo`: clona el proceso actual (crea un hermano nuevo con la misma plantilla completa, no solo las públicas).

El bucle `while(mem[id+_Status] && ...)` de `lcal`/`lclo` para buscar hueco libre, junto con el hecho de que `id_end` solo crece (nunca se compacta), explica por qué DIV tiene un límite práctico de procesos simultáneos a lo largo de la vida del programa relacionado con `imem_max`.

**Multi-pila para funciones**: dado que "funciones" DIV comparten la implementación con "procesos" (mismo mecanismo `lcal`, distinguido solo por el `lnop` posterior), cuando un proceso llama a una función definida como bloque de proceso y esa función hace `FRAME` en medio de su ejecución, hay que "aparcar" la pila de ejecución del llamador. Esto se resuelve con `guarda_pila()`/`carga_pila()`/`actualiza_pila()` (`i.cpp`, línea ~440): cada proceso dormido-por-llamar-a-función puede tener guardado un fragmento de pila en memoria dinámica (`malloc`), independiente del array `pila[]` global compartido por el resto de la ejecución.

## SESSION=1 vs SESSION=0 (debugger)

El nombre "SESSION" del enunciado corresponde en el código real a la variable `SESSION` del makefile moderno `src/div32run/makefile` (líneas 1-49), que a su vez controla si se define o no la macro de preprocesador `DEBUG` (`OPTIONS += -DDEBUG` solo si `SESSION==1`). El propio comentario de cabecera del makefile lo explica sin ambigüedad:

> `SESSION=1` genera el intérprete que incorpora el trazador (el que se ejecuta desde el IDE al arrancar un programa). `SESSION=0` genera la `DIV32RUN.DLL` que se incluirá con el instalador del juego — pero **aún no** es la versión redistribuible, porque el instalador (`src/install`, no investigado en profundidad aquí) le añade después la tabla de textos traducidos.

Diferencias concretas confirmadas:

| | `SESSION=1` (`session.div`/`.386`) | `SESSION=0` (`div32run.ins`/`.386`) |
|---|---|---|
| Macro | `-DDEBUG` definida | no definida |
| Extensor DOS | `dos4g` (DOS/4G) | `pmodew` (PMODE/W, propio, "empaquetado" con la herramienta `pmwlite`) |
| Objeto de debugger | incluye `d.cpp` (`d.obj`) | no se enlaza `d.cpp` |
| Uso | lanzado por el IDE (`SYSTEM\DOS4GW.EXE SYSTEM\SESSION.DIV SYSTEM\EXEC.EXE`, según `src/wstub/wstub.c`) | el que termina, tras el instalador, dentro del juego distribuido |
| Optimización Watcom (scripts legados) | `/3r /fpc` (386, sin optimizar) | `/5r /fp5` (Pentium) |

El efecto de `DEBUG` en el propio código es masivo y transversal (no es solo "activar un módulo"): en `inter.h`, `i.cpp` y `kernel.cpp` hay decenas de bloques `#ifdef DEBUG` que:

- Compilan dentro del ejecutable toda la infraestructura de ventanas del depurador (`tventana`, `t_item`, `t_listbox`, colores, fuentes propias, definidas en `inter.h` línea 839+), que se dibuja usando las mismas primitivas gráficas del propio DIV, superpuesta sobre el juego en ejecución.
- Añaden las comprobaciones de seguridad en tiempo de ejecución (división por cero, rango de `switch`, validez de identificadores de proceso, punteros nulos, desbordamiento de strings) que en el build de distribución **no existen en absoluto** (no es que se salten con un flag: el código ni siquiera está compilado).
- Habilitan el mecanismo de breakpoints (`ldbg`) y el paso a paso (`nucleo_trace()`, activado con `#define TRACE` al reincluir `kernel.cpp`).
- Añaden contabilidad de rendimiento por proceso/función (`process_exec()`, `function_exec()`, usadas por el perfilador integrado del IDE).

`d.cpp` (4759 líneas) implementa, sobre esa infraestructura, los diálogos reales: lista de procesos, inspección de variables, perfilador, ventana de código fuente con la línea actual resaltada, gestión de breakpoints, diálogos de error. El comentario de cabecera del propio archivo lo resume: *"Funciones del debugger y diálogos de error. Ojo: la función `trace_process()` está en `i.cpp`"* — confirmando además que el motor de "un paso" vive junto al scheduler normal, no dentro de `d.cpp`.

## Timing/sincronización

El bucle principal es `interprete()` (`i.cpp`, línea 481): mientras haya procesos vivos y no se haya pulsado Ctrl+Esc, por cada iteración llama a `frame_start()`, ejecuta procesos en un `do { exec_process(); } while(ide)` (mientras el scheduler siga encontrando procesos ejecutables en este frame) y luego `frame_end()`.

`frame_start()` (línea 661) hace, entre otras cosas confirmadas leyendo el código:

- Limpieza de procesos muertos (`_Status==1`) antes de empezar el frame.
- Actualiza contadores `timer(0..9)` (los temporizadores de usuario expuestos al lenguaje DIV) en función de cuánto ha avanzado el reloj real desde el frame anterior.
- Calcula FPS reales con una media móvil (`ffps=(ffps*9+100/(delta))/10`).
- **Control de velocidad por frame** mediante un contador de "reloj virtual" (`freloj`/`ireloj`) leído contra `get_reloj()` (que en última instancia depende de un timer de hardware): si vamos con retraso, permite saltarse hasta `max_saltos` volcados de pantalla (renderizados) sin dejar de ejecutar lógica, para no perder framerate lógico aunque se pierda framerate visual; si vamos bien, hace un **busy-wait** (`do {...} while (get_reloj()<freloj)`) hasta alcanzar el instante exacto del siguiente frame.
- Marca todos los procesos como "no ejecutados todavía este frame" y relee ratón/joystick/teclado.

La fuente de tiempo de hardware real está en `src/div32run/divtimer.cpp` (77 líneas) y `timer.asm` (no abierto en detalle en esta pasada, pero referenciado desde el propio `divtimer.cpp`): `timer_init()` reprograma directamente el **PIT (Programmable Interval Timer) del PC**, escribiendo en los puertos `0x43`/`0x40` (modo de operación y divisor de frecuencia del canal 0) e instalando un manejador de la `IRQ0` (`_dos_setvect(8, ...)`) que reemplaza el vector de interrupción del reloj del sistema. Es decir, DIV **no confía en el timer de 18.2 Hz de BIOS**: reprograma el PIT a la frecuencia que necesite y cuenta sus propios ticks (`timer_count`) en el manejador de interrupción, un patrón típico de motores DOS de la época (aquí implementado sobre la base de la librería de sonido **JUDAS**, cuyo comentario de cabecera dice literalmente "Doesn't officially belong to JUDAS but feel free to use! This is blasphemy-ware too!" — JUDAS es la librería de sonido de terceros integrada en `3rdparty/judas`, y este timer comparte su infraestructura de bloqueo de memoria en modo protegido, `judas_memlock`/`judas_memunlock`, necesaria porque el manejador de interrupción corre con las páginas fijadas para evitar que el mecanismo de paginación del extensor DOS lo deje sin código en memoria en mitad de una interrupción).

`frame_end()` (línea 814) hace el trabajo "de pintado": la primera vez, resuelve mediante `DIV_import()` los punteros a las funciones del backend de vídeo (ver sección de DLLs más arriba); luego, si no se decidió saltar el volcado este frame, restaura el fondo fuera de scroll/modo-7, procesa objetos de modo 8 (`loop_mode8()`), y termina volcando a la pantalla real. No se investigó en detalle el código de rasterización de sprites/scroll/modo-7 en sí (`s.cpp`, `v.cpp`, `src/div32run/vpe/*`); queda fuera del alcance de "modelo de ejecución" propiamente dicho y se documenta, si corresponde, en un capítulo de gráficos aparte.

**Traducción a alto nivel del lenguaje DIV**: un bloque `PROCESS nombre(...) ... END` del código fuente se compila a una secuencia de bytecode direccionable por `lcal`/`lclo`, y cada instancia en ejecución de ese bloque es uno de los bloques de `mem[]` descritos en la sección anterior, no un objeto en el sentido de C++. La sentencia `FRAME` del lenguaje es literalmente el opcode `lfrm` (ceder el turno hasta el próximo frame); `FRAME(n)` con porcentaje es `lfrf`. No hay hilos del sistema operativo ni preempción real: la concurrencia de "procesos" DIV es cooperativa, y toda la ilusión de "varios procesos corriendo a la vez" es el scheduler round-robin de `exec_process()` repartiendo turnos de CPU dentro de un único hilo de DOS antes de cada `frame_end()`.

## Archivos concretos leídos

- `src/div/divc.cpp` (compilador; leídas en detalle las líneas ~640-1145, 700-830, 1275-1345, 1349+, 4040-4060, 7360-7420, y grep completo del archivo)
- `src/div/div_stub.h` (cabecera del stub embebido, primeras líneas)
- `src/div32run/inter.h` (completo, 1066 líneas)
- `src/div32run/kernel.cpp` (completo, 699 líneas)
- `src/div32run/i.cpp` (leídas en detalle líneas 1-120, 151-460, 460-808, 808-930, 1260-1499)
- `src/div32run/d.cpp` (cabecera y primeras ~50 líneas; resto solo localizado/tamaño confirmado)
- `src/div32run/f.cpp` (líneas 4205-4230, dispatcher de `function()`; grep de estructura general)
- `src/div32run/dll.c` (completo)
- `src/div32run/divtimer.cpp` (completo, 77 líneas)
- `src/div32run/makefile` (líneas 1-50, mecanismo `SESSION`)
- `src/div32run/386ins.mak`, `src/div32run/386dbg.mak` (completos, scripts de build legados)
- `src/div32run/586ins.inc`, `src/div32run/586dbg.inc` (completos, definición de `DEBUG`)
- `src/div32run/c.bat`, `c2.bat`, `c3.bat`, `asm.bat`, `td.bat` (completos)
- `src/div32run/wstub/wstub.c`, `src/div32run/wstub/wstub.lnk` (completos)
- `src/wstub/wstub.c` (completo, 169 líneas), `src/wstub.h` (completo)
- `src/divdll.c` (líneas 120-220, carga de PE/DLL)
- `src/pe_load.c` (primeras ~120 líneas, definiciones de formato PE)
- `docs/architecture/01-vision-general.md` y `04-build-system.md` (para contexto y consistencia terminológica con el resto de la documentación)

### Puntos marcados explícitamente como no confirmados / fuera de alcance

- El contenido exacto (desensamblado) del stub de 602 bytes (`div_stub`) no se verificó instrucción a instrucción; su comportamiento se infiere del formato de archivo y de cómo lo consume `DIV32RUN`, no de lectura directa de su ensamblador.
- La discrepancia entre el `wstub.c` "vacío" presente en el repo y el binario real embebido en `div_stub.h` no se pudo resolver con el código disponible: no queda claro si el código fuente original de ese stub simplemente no forma parte de este repositorio, o si hay otro archivo que lo genera y no fue localizado.
- El backend de vídeo "VPE" (`src/div32run/vpe/*`: `draw_cw.cpp`, `gfx.cpp`, `hard.cpp`, `object.cpp`, etc.) no se investigó por dentro; solo se confirmó que se integra en el runtime a través del mismo mecanismo de `DIV_import`/`divlibrary` que las DLL de extensión de usuario.
- El paso final de empaquetado del instalador (`src/install`, que según el comentario del propio makefile añade la tabla de textos traducidos a `div32run.ins` antes de que sea redistribuible) no se investigó.
- El networking (`src/netlib`, `_net_loop()` llamado desde `exec_process()`) y la detección de tarjetas de sonido/CD no se investigaron para este documento.
- `timer.asm` (la mitad en ensamblador del timer, referenciada desde `divtimer.cpp`) no se leyó; la descripción de la reprogramación del PIT se basa en `divtimer.cpp`, que sí se leyó completo.
