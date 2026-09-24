# Aspectos técnicos relevantes y desafíos de portabilidad

Este documento profundiza, con evidencia extraída directamente del código fuente, en los aspectos técnicos que hacen de DIV Games Studio 2 un proyecto poco convencional de compilar y potencialmente muy costoso de portar a plataformas modernas. Cada sección cita archivos y líneas concretas del repositorio.

## 1. Compilación: OpenWatcom 1.9 y triple target

El `README.md` (línea 40) es explícito:

> "OpenWatcom 1.9 instalado y funcionando. Posiblemente también funcione Watcom 10 o superior, pero no lo hemos probado. OJO: necesitas instalar los compiladores para *DOS 16 bits*, *DOS 32 bits* y también para la plataforma desde la que estés compilando (ya que algunas herramientas, como PMWLITE, se compilan y ejecutan durante el proceso de compilación de DIV). Actualmente hay incompatibilidades en el código con OpenWatcom 2."

Esto se confirma en `.travis/install_watcom.sh`, que descarga explícitamente `open-watcom-c-linux-1.9` u `open-watcom-c-win32-1.9.exe` (no OW2) para el CI.

El *triple target* no es solo una preferencia: es un requisito funcional del propio proceso de build.

- **Target 1 — DOS 16 bits**: usado para `src/div_stub` (el stub que se antepone a todo `.EXE` generado por DIV, ver sección 2) y para `src/wstub`.
- **Target 2 — DOS 32 bits (extendido)**: usado para el núcleo del IDE (`src/div` → `D.EXE`/`D.386`) y del intérprete (`src/div32run` → `DIV32RUN.DLL`/`.386`), que corren sobre el extensor DOS `PMODE/W` (carpeta `pmwlite/`, ver sección 2) como alternativa a DOS/4GW. El propio `README.md` (línea 72) lo indica: *"pmwlite: Extensor de 32 bits para DOS alternativo a DOS/4GW, que se usa para la DIV32RUN.DLL y el instalador."*
- **Target 3 — Host nativo**: porque, según el makefile raíz, "algunas herramientas, como PMWLITE, se compilan y ejecutan durante el proceso de compilación de DIV" — es decir, el build necesita ejecutar herramientas nativas (empaquetadoras, generadores de datos, etc.) en la máquina donde se compila, no solo producir binarios DOS.

El `makefile` raíz (`ROOT/makefile`) usa `wmake` (Watcom Make) con macros condicionales por sistema operativo (`os.mif`) y permite elegir el ensamblador (`ASM = WASM` por defecto, con soporte opcional para `TASM`), documentando explícitamente que el fork prefiere WASM porque TASM es privativo y difícil de conseguir, aunque advierte de posibles diferencias de comportamiento entre ensambladores.

**No determinado sin más esfuerzo**: no se investigó en detalle *qué* incompatibilidades concretas rompen la compilación bajo OpenWatcom 2 (el README lo menciona pero no se profundizó línea a línea en los headers de OW2 para aislar la causa exacta).

## 2. Código 16/32 bits mezclado y llamadas DOS/BIOS/DPMI directas

### El stub de arranque es genuinamente 16 bits, en ensamblador real-mode

`src/div_stub/asm/exec.asm` es el código que antecede a cualquier ejecutable generado por DIV (el "EXE stub"). Es `.model small`, `.186`, y su rutina principal (`exec proc far`) hace, en orden:

1. `setfree`: libera la memoria DOS no usada por el stub, restando el segmento de `SS` menos el `PSP` (usa `int 21h, AH=4Ah`, "modificar tamaño de bloque de memoria").
2. `get_cpuid`: detección de CPU **sin CPUID**, mediante el truco clásico de flags (bits 12-15 de `FLAGS` en 8086, bit AC en 486, bit ID en Pentium) — código realmente de la era pre-Pentium.
3. Si la CPU no es al menos un Pentium (`cpu_type >= 5`), aborta con un mensaje de error usando `int 21h, AH=09h` (imprimir cadena).
4. `exeprg`: reconstruye la línea de comandos desde el PSP y usa `int 21h, AH=4Bh` (**DOS EXEC**) para cargar y ejecutar `DIV32RUN.DLL` — que es, pese a la extensión `.DLL`, un ejecutable DOS extendido de 32 bits cargado por el extensor **PMODE/W** (ver `pmwlite/file_id.diz`: *"PMODE/W v1.33 DOS Extender ... Replaces DOS4GW.EXE"*).

Es decir: **el arranque real de un juego DIV es un programa de 16 bits en modo real que hace un `EXEC` de DOS sobre un binario de 32 bits en modo protegido**. Esta frontera de 16/32 bits está incrustada en el propio mecanismo de carga, no es un artefacto incidental del compilador.

### El núcleo usa DPMI, BIOS e interrupciones DOS directamente

Búsqueda de `int86`, `int386`, `_dos_*`, `i86.h`/`dos.h` en `src/div` y `src/div32run` (y módulos compartidos) arroja 55 archivos con coincidencias. Ejemplos concretos:

**a) Lectura de puertos de joystick y del PIT (timer) — `src/div32run/f.cpp` (líneas 2433-2463)**, dentro de la implementación de la sentencia DIV para leer el joystick:
```c
outp(TIMER_PORT+3,0);
start=inp(TIMER_PORT); start+=inp(TIMER_PORT)<<8;
outp(GAME_PORT,0);
for(i=0;i<TIME_OUT;i++) if((inp(GAME_PORT)&mask)==0) break;
```
Esto mide directamente, por I/O de puertos, cuánto tarda en descargarse el condensador RC del puerto de joystick (`GAME_PORT`, el clásico 0x201) usando el PIT (`TIMER_PORT`) como reloj de referencia — la técnica estándar de lectura de joystick analógico en PC de los 90, sin ninguna capa de abstracción del sistema operativo.

**b) Llamadas DOS de sistema de archivos — `src/div32run/f.cpp` e `i.cpp`**: uso extensivo de `_dos_findfirst`/`_dos_findnext` (búsqueda de ficheros), `_dos_setdrive`/`_dos_getdrive` (cambiar/leer unidad activa), `_dos_setfileattr` — usadas para implementar las sentencias del propio lenguaje DIV que manipulan ficheros y unidades (p. ej. `f.cpp:3062`, `f.cpp:3130`, `i.cpp:1213`).

**c) DPMI real-mode callback (INT 31h, AX=0x300) — `src/vpe/hard.cpp`**, función `int386Extend`:
```c
int int386Extend(short _inter,RMREGS *rmregs)
{
  ...
  regs.x.eax = 0x300;   // DPMI: Simulate Real Mode Interrupt
  regs.x.ebx = _inter;
  regs.x.edi = FP_OFF(rmregs);
  sregs.es   = FP_SEG(rmregs);
  int386x(0x31,&regs,&regs,&sregs);
  ...
}
```
Esto permite que código corriendo en modo protegido (32 bits) invoque una interrupción real-mode (típicamente `INT 7Ah`, la API de IPX — ver sección 4) simulándola vía DPMI, y `allocate_dos_memory`/`free_dos_memory` en el mismo archivo usan `int386(0x31, ...)` con `AX=0x100`/`0x101` (asignar/liberar memoria DOS convencional real-mode) — necesario porque los drivers TSR de DOS (IPX, ratón) solo entienden direcciones reales de memoria baja.

**d) Cambios de modo de vídeo BIOS — `int 10h`** en `src/vesa.asm` (`vbeSetMode_`, `vbeSetScanWidth_`, `vbeSetStart_`, `vbeSetVGAMode_`, etc., ver sección 3) y en `exec.asm` (`mov ax,3 / int 10h` para restaurar modo texto al salir).

**Conclusión de esta sección**: no es solo "algo" de código 16/32 bits mezclado — es una arquitectura de tres capas (stub real-mode → extensor DOS de 32 bits → llamadas DPMI hacia servicios real-mode) que atraviesa el núcleo entero, no un módulo aislado.

## 3. Ensamblador x86: inventario de rutinas de bajo nivel

Se localizaron y revisaron los siguientes ficheros `.asm` (sintaxis Intel/MASM, compilables con WASM o TASM):

| Fichero | Líneas | Contenido |
|---|---|---|
| `src/a.asm` | 70 | `memcpyb_` (memcpy vía `REP MOVSB`), `call_` (salto indirecto a una dirección arbitraria, `JMP NEAR PTR EAX`), `get_t_`/`get_t2` (lee el **timestamp counter** de CPU con el opcode crudo `db 0Fh,032h` = `RDMSR`/lectura de contador — nota: el comentario del propio código y el opcode sugieren lectura de un contador de ciclos, ejecutado a través de un `CALL FAR` calculado dinámicamente para cruzar de 32 a 16 bits (`kk1`/`kk2` construyen un puntero far calculado a partir de `CS`)). |
| `src/timer.asm` | 56 | `timer_handler_`: manejador de la **interrupción de timer (IRQ0/INT 8)** en ensamblador puro. Guarda registros (`pushad`), reprograma `DS`/`ES`, hace `sti`, envía el **EOI (End Of Interrupt) al PIC** con `mov AL,20h / out 20h,AL`, incrementa un contador (`_timer_count`), llama a una función de usuario (`_timer_function`) y decide si encadena con el vector de interrupción original (`_timer_oldvect`) según un contador de frecuencia (`_timer_systemcount`) — el patrón clásico de "chaining" de IRQ0 para generar un timer de alta frecuencia sin perder el tick de 18.2 Hz del reloj del sistema. |
| `src/div/visor/t.asm` | 788 | Rutinas `nucleo8_8_`, `nucleo8_16_`, `nucleo8_32_`, ... `nucleo8_256_` y sus variantes `mask_nucleo8_*_`: son **núcleos de blitting desenrollados** (unrolled loops) para copiar franjas de 8, 16, 32, 64, 128 y 256 píxeles con y sin máscara de transparencia — optimización manual de rendimiento para el renderizado de sprites a 8 bits por píxel. |
| `src/vesa.asm` | 936 | Biblioteca **"MaLiCe VeSa LiBrARy"**: define a mano las estructuras `VBEINFOBLOCK`/`MODEINFOBLOCK` del estándar VESA BIOS Extensions y expone procedimientos como `vbeInit_`, `vbeGetModeInfo_`, `vbeSetMode_`, `vbeSetVirtual_`/`vbeFreeVirtual_` (memoria virtual VESA vía DPMI), `vbeFlip_`, `vbePutPixel_`, `vbeClearScreen_`, `vbeSetScanWidth_`/`vbeGetScanWidth_`, `vbeSetStart_`/`vbeGetStart_` (scroll de página VESA), `vbeSetVGAMode_`/`vbeGetVGAMode_`, `vbeWR_`. Usa `int 31h` (DPMI) para reservar/liberar memoria lineal y simular interrupciones reales, e `int 10h` (BIOS de vídeo) para invocar las funciones VESA/VGA. |
| `src/vpe/draw_fa.asm` | 59 | `DrawFSpan_`: dibuja un **span horizontal de suelo** (floor) en un renderizador tipo *mode7*/raycaster, usando una tabla de saltos (`FLoopOffset`) hacia un bucle desenrollado (`FLoopStart`/macro `FLoop`) según el número de píxeles a dibujar. |
| `src/vpe/draw_oa.asm` | 182 | `DrawOSpan_`, `DrawMaskOSpan_`, `DrawTransOSpan_`: variantes normal, con máscara y con transparencia para dibujar **objetos/sprites** dentro del motor VPE (probablemente "voxel/wall/object" — visor 3D tipo Doom/Wolfenstein). |
| `src/vpe/draw_wa.asm` | 155 | `DrawWSpan_`, `DrawMaskWSpan_`, `DrawTransWSpan_`: mismas variantes para **paredes** (walls). |
| `src/div_stub/asm/exec.asm` | 403 | Ver sección 2: stub 16-bit de arranque, detección de CPU y `EXEC` de DOS. |
| `src/div_stub/asm/cstrt086.asm` | (no leído en detalle) | Localizado por la búsqueda; por el nombre, es el *C startup* de 16 bits (runtime de arranque de Watcom para el modelo de memoria del stub). No se analizó línea a línea. |
| `src/cpuid.asm` | (no leído en detalle) | Detección de CPU vía instrucción `CPUID` (complementaria al `get_cpuid` manual de `exec.asm`, que es pre-Pentium). No se analizó línea a línea. |

En conjunto, el ensamblador cubre: arranque de programa (stub), manejo de IRQ0 (timer), acceso a VESA/VGA a bajo nivel, y los núcleos críticos de rendimiento de dos motores de render distintos (el blitter de sprites 2D de `t.asm` y el renderizador pseudo-3D de VPE con sus `draw_*a.asm`). Todo ello asume arquitectura x86 de 32 bits en modo protegido plano (`.386`/`.386P`, `.MODEL FLAT`), salvo el stub que es 16-bit real-mode.

## 4. Dependencias DOS/hardware "hardcodeadas"

### Temporizador / PIC (8259) — `src/timer.asm` + `src/div/divtimer.cpp`

`timer_init()` en `divtimer.cpp` (líneas 36-55) programa directamente el **PIT 8253/8254** por I/O de puertos:
```c
timer_oldvect = _dos_getvect(8);
_disable();
_dos_setvect(8, timer_newvect);
outp(0x43, 0x34);           // modo de operación del canal 0 del PIT
outp(0x40, frequency);      // byte bajo del divisor de frecuencia
outp(0x40, frequency >> 8); // byte alto
_enable();
```
Y el handler en ensamblador (`timer.asm`, ver sección 3) envía el EOI al **PIC 8259** con `out 20h, AL`. Esto es reprogramación directa de hardware de interrupciones del PC, algo que en un SO moderno ni siquiera es posible desde espacio de usuario.

### VESA/VGA — `src/vesa.asm` + `src/div/det_vesa.cpp` / `src/div32run/det_vesa.cpp`

`detectar_vesa()` (`src/div/det_vesa.cpp`, líneas 32-77) llama a `vbeInit()` (implementado en `vesa.asm`) y recorre `VbeInfoBlock.VideoModePtr` probando cada modo con `vbeGetModeInfo()`, filtrando por `BitsPerPixel==8` para construir la lista de resoluciones soportadas por la tarjeta gráfica. Toda la comunicación con la tarjeta pasa por `int 10h` (BIOS de vídeo/VESA) e `int 31h` (DPMI, para asignar el buffer real-mode donde la BIOS deposita su respuesta) — ver sección 3.

### Sound Blaster "bare-metal" — `src/div/divsb.cpp`

Es un driver de Sound Blaster escrito directamente contra el hardware (no usa ningún API de sonido de DOS):
- Tablas de puertos DMA por canal: `dma_adr[8]`, `dma_cnt[8]`, `dma_page[8]`, `dma_mask[8]`, `dma_mode[8]`, `dma_clr[8]` (líneas 23-28) — los registros del controlador DMA 8237 para los 8 canales posibles.
- `dspwrite`/`dspread` (líneas 31-41): esperan por polling (`while(inp(...)&0x80)`) el bit de "listo" del DSP de la tarjeta antes de escribir/leer un byte por el puerto base configurado (`judascfg_port`).
- `sbinit()` (líneas 43-72): hace un **reset por hardware del DSP** (secuencia `outp(port+6,1)` seguido de espera y `outp(port+6,0)`) y comprueba la respuesta `0xAA` característica del chip.
- `sbmalloc()` (líneas 74-109): reserva memoria DOS convencional alineada a un límite de 64 KB mediante DPMI (`int386(0x31,...)`, `AX=0x0100`) — necesario porque el DMA del Sound Blaster requiere buffers que no crucen fronteras de 64 KB en memoria física real.
- `sbrec()` (líneas 146-169): programa una transferencia DMA completa a mano (máscara del canal, modo, dirección, longitud, página) y arranca el DSP con el comando de grabación (`0x24`).

Este código pertenece a JUDAS ("blasphemy-ware", según su propio comentario de cabecera), una librería de audio de terceros integrada directamente en el árbol de DIV.

### Red IPX — `src/netlib/ipxlib.c` (+ `src/vpe/hard.cpp` para el soporte DPMI)

`IPX_CheckIfLoad()` (línea 58) usa `int386Extend(0x2F, &mregs)` — es decir, simula la **interrupción multiplex del DOS (INT 2Fh)** para preguntar si el TSR de IPX está cargado. El resto de operaciones (`IPX_OpenSocket`, `IPX_CloseSocket`, `IPX_GetInternalAddress`, `IPX_SendPacketAddress`, `IPX_ListendPacket`) usan `int386Extend(0x07A, &mregs)`, es decir, la **API real-mode de Novell IPX en INT 7Ah**, simulada desde modo protegido vía DPMI (`int386Extend`, ver sección 2c). El código reserva explícitamente memoria DOS convencional para los ECB (Event Control Blocks) y paquetes (`allocate_dos_memory`), porque el driver IPX real-mode solo puede escribir en memoria baja direccionable en modo real.

### Ratón — mencionado en el contexto de partida

No se localizó un fichero dedicado único al driver de ratón (aparece disperso, p. ej. `src/install2/mouse.c`, y llamadas a interrupciones de ratón vía `int86`/`int33h` implícitas en el uso de `dos.h`/`i86.h` en varios módulos). **No se profundizó** con la misma exhaustividad que timer/VESA/SB/IPX; se confirma la dependencia pero no se documentaron aquí los detalles de `INT 33h` función por función.

## 5. Loader PE propio (`pe_load.c` + `divdll.c`)

### Qué implementa `pe_load.c`

Define a mano (con `#pragma pack(push,1)`) las estructuras del formato PE de Windows: `IMAGE_DOS_HEADER`, `IMAGE_FILE_HEADER`, `IMAGE_OPTIONAL_HEADER` (224 bytes, formato PE32 clásico), `IMAGE_NT_HEADERS`, `IMAGE_SECTION_HEADER`, `IMAGE_EXPORT_DIRECTORY` y `IMAGE_BASE_RELOCATION`. El flujo de `PE_ReadFP()` (líneas 530-628):

1. Lee la cabecera DOS y comprueba la firma `MZ` (0x5A4D).
2. Salta a `e_lfanew` y comprueba la firma `PE\0\0`.
3. Lee `IMAGE_NT_HEADERS` completo y valida que `Machine == IMAGE_FILE_MACHINE_I386` y que el tamaño del *optional header* coincide exactamente con 224 bytes — es decir, **exige el formato PE32 de 32 bits tal y como lo emitía el linker de Watcom de los 90**, sin tolerancia a variantes.
4. Lee la tabla de secciones y, para cada una (`PE_LoadSection`, líneas 348-410), reserva memoria con `malloc()` del tamaño alineado a `SectionAlignment`, la rellena con ceros si es BSS (`IMAGE_SCN_CNT_UNINITIALIZED_DATA`) o copia los bytes crudos desde el fichero (`fseek`+`fread`).
5. Aplica **relocaciones base** (`PE_Relocate`/`PE_ApplyReloc`, líneas 415-517): recorre los bloques de `IMAGE_DIRECTORY_ENTRY_BASERELOC` y, para cada entrada de 16 bits, soporta únicamente `IMAGE_REL_BASED_ABSOLUTE`, `IMAGE_REL_BASED_HIGH` y `IMAGE_REL_BASED_HIGHLOW` — cualquier otro tipo (`LOW`, `HIGHADJ`, tipos MIPS/i860) hace fallar la carga con `dll_error = "Unsupported relocation type"`.
6. Libera las secciones marcadas como `IMAGE_SCN_MEM_DISCARDABLE` tras la carga.
7. `PE_ImportFnc()` (líneas 661-711) busca una función por nombre recorriendo a mano la **tabla de exportación** (`IMAGE_EXPORT_DIRECTORY`: `AddressOfNames`, `AddressOfNameOrdinals`, `AddressOfFunctions`), con un tratamiento especial para *name mangling* de C++ de Watcom (si el segundo carácter del nombre exportado es `?`, recorta el prefijo y trunca en el primer `$`, sustituyéndolo por `_`).

### Qué **no** implementa (hallazgo relevante)

**El loader no procesa en absoluto la tabla de importación (`IMAGE_DIRECTORY_ENTRY_IMPORT`)**: no hay ninguna referencia a esa entrada del directorio de datos en todo `pe_load.c`, ni resolución de DLLs importadas al estilo Windows (no hay IAT walking, no hay `LoadLibrary`/`GetProcAddress` equivalentes por nombre de DLL). Tampoco se procesan: recursos (`IMAGE_DIRECTORY_ENTRY_RESOURCE`), TLS, tabla de excepciones, ni depuración.

En su lugar, `divdll.c` implementa un **mecanismo de enlace dinámico propio y paralelo**, completamente ajeno al estándar PE:
- `DIV_export(name, obj)` añade una entrada a una lista enlazada global (`pool`) de pares nombre→puntero.
- `DIV_import(name)` busca en esa misma lista.
- Al cargar una DLL de DIV (`Internal_LoadDll`), tras aplicar relocaciones, se localizan por **exportación PE** (vía `PE_ImportFnc`, comprobando alternativamente los nombres decorados `divmain_`, `_divmain`, `W?divmain`, y lo mismo para `divlibrary_`/`divend_`) tres puntos de entrada fijos: `divmain` (punto de entrada del intérprete, recibe `DIV_import`/`DIV_export` como *callbacks*), `divlibrary` (registra funciones exportadas por la DLL para que el compilador las use vía `COM_export`) y `divend` (des-inicialización).

Es decir: **el único mecanismo real de "importación" cruzada entre módulos es este pool global de símbolos por nombre, resuelto en tiempo de carga mediante llamadas explícitas dentro de `divmain`/`divlibrary`** — la tabla de importación estándar de PE simplemente no se usa ni se necesita, porque las DLLs de DIV no dependen de otras DLLs de Windows, solo del propio ejecutable anfitrión (D.EXE o DIV32RUN) a través de este pool.

### Riesgos y limitaciones frente a un loader de SO real

- **Sin ASLR ni protecciones de memoria**: las secciones se cargan con `malloc()` normal; no hay separación de permisos R/W/X por página (el propio comentario en `PE_LoadSection`, línea 395, dice *"Watcom 10 pone las variables inicializadas a 0 en BSS !!!!"*, evidenciando que ya en su día hubo sorpresas de comportamiento del linker que tuvieron que parchearse a mano).
- **Sin validación robusta de límites**: no se ve comprobación exhaustiva de que `PointerToRawData`/`SizeOfRawData` estén dentro del tamaño real del fichero antes de `fseek`/`fread`; un PE corrupto o malicioso podría provocar lecturas fuera de rango o corrupción de memoria (el propósito original —cargar DLLs de compilador/intérprete propias de DIV, no ejecutables arbitrarios de terceros no confiables— probablemente hizo que esto nunca se considerase un problema de seguridad).
- **Tipos de relocación muy limitados**: si algún día se generase una DLL con un linker distinto a Watcom que emitiera relocaciones `LOW` o `HIGHADJ`, la carga fallaría explícitamente.
- **Acoplado al ABI de Watcom**: el *name-mangling* especial (detección de `?`/`$`) es específico del compilador de C++ de Watcom; un PE producido por otro toolchain (MSVC, GCC/MinGW) no se resolvería igual.
- **No hay manejo de dependencias entre DLLs de terceros**: al no leerse la tabla de importación, cualquier DLL que sí dependiera de símbolos de otra DLL externa (no del pool `DIV_export`/`DIV_import`) simplemente no funcionaría — el diseño asume un universo cerrado de dos-o-tres binarios (D.EXE/DIV32RUN + DLLs de DIV) que se conocen entre sí por convención de nombres fijos.

En resumen: es un loader **deliberadamente mínimo**, suficiente para el caso de uso exacto de DIV (cargar sus propias DLLs de intérprete/compilador en modo protegido DOS, donde no existe ningún loader de SO al que delegar), pero muy alejado en robustez y superficie de un loader de sistema operativo real.

## Hallazgos adicionales

Búsqueda de `TODO`, `FIXME`, `HACK`, `XXX`, `BUG` en `src/` (ver comando `Grep` ejecutado sobre todo el árbol):

- `src/wstub/wstub.c:27`: `// TODO: sacar esto a un asm o buscar una alternativa en C`
- `src/wstub/wstub.c:43`: `// TODO: Buscar otra forma menos chapucera de hacer esto` (el propio autor califica la solución actual de "chapucera").
- `src/install2/main.c:239`: `// TODO: considerar un handler que sirva para algo`
- `src/install2/main.c:245`: `// TODO: obtener discos duros y espacio libre`
- `src/div/divedit.cpp:2701`: comentario `// TODO` en un `case` del editor (sin más contexto).
- `src/div/divc.cpp:74`: comentario en español sobre una función de `"capar()"` direcciones en modo DEBUG *"para evitar en lo posible los page fault"* — indica que en su momento hubo problemas de acceso a memoria inválida que se mitigaron recortando direcciones en vez de corregir la causa raíz.
- `src/div/divc.cpp:153`: `// SOLUCION 3: PASAR TODOS LOS OFFSET A BYTE` — comentario que sugiere iteración sobre varias soluciones a un mismo problema (probablemente de límites de direccionamiento), quedándose con la tercera.

No se hallaron ocurrencias de `HACK`, `XXX` ni `BUG` como palabra propiamente dicha (sí aparece "BUG" implícito en el discurso del roadmap del proyecto, ver `01-vision-general.md`, pero no como marcador en comentarios de código). La gran mayoría de coincidencias de la búsqueda amplia correspondían a la macro de compilación `#ifdef DEBUG`/`_DEBUG` (cientos de apariciones en `src/div32run/i.cpp`, `f.cpp`, `s.cpp`, `kernel.cpp`, etc.), que no son TODOs sino código de depuración condicional del intérprete — mencionadas aquí solo para dejar constancia de que la búsqueda fue exhaustiva y de que esa fue la señal dominante, no bugs marcados explícitamente.

**Nota sobre severidad**: los TODOs encontrados son puntuales y de alcance acotado (limpieza de un stub, gestión de discos en el instalador); no se encontró evidencia de TODOs/FIXMEs marcando problemas estructurales grandes. Los comentarios de `divc.cpp` sí son más reveladores: apuntan a que el manejo de punteros/offsets en el compilador tuvo, históricamente, problemas de acceso a memoria fuera de rango que se resolvieron con mitigaciones (capado de direcciones, truncado a byte) en vez de un rediseño.

### Sobre el grafo de dependencias (`docs/DIV dependency graph.graphml`)

El fichero es procesable como XML (formato yEd/GraphML, 3379 líneas, generado con yEd 3.19.1.1): contiene 135 nodos y 56 aristas. Una extracción rápida de las etiquetas de nodo confirma que documenta relaciones de compilación entre los `.c`/`.cpp`/`.asm` de `D.EXE` y `DIV32RUN` (agrupados en "D sources" y "DIV32RUN sources") y los artefactos de build (`D.EXE`, `D.386`, `DIV32RUN.INS`, `DIV32RUN.386`, ficheros `.MIF` de Watcom, etc.) — es decir, es un mapa de qué fuente alimenta a qué target de compilación, útil para entender el makefile, no un grafo de arquitectura en tiempo de ejecución. **No se extrajo la lista completa de las 56 aristas** (qué nodo depende de cuál exactamente) por no ser necesario para este documento; si se requiere ese detalle exacto, habría que parsear los elementos `<edge source="..." target="...">` del GraphML, lo cual es viable pero no se hizo aquí.

## Implicaciones para un port moderno

Basado en la evidencia anterior, un port de DIV Games Studio 2 a una plataforma moderna (Windows/Linux/macOS nativos, sin DOS ni DOSBox) tendría que sustituir por completo cuatro capas de I/O, y podría reaprovechar con distinto grado de esfuerzo el resto:

### Capas a reemplazar por completo

1. **Vídeo**: todo `vesa.asm` (VESA/BIOS `int 10h`) y las escrituras directas a registros VGA en `src/div32run/v.cpp` (constantes `CRTC_INDEX 0x3d4`, `SC_INDEX 0x3c4`, `MISC_OUTPUT 0x3c2`, acceso a `vga = (byte*)0xA0000`) tendrían que sustituirse por una capa moderna (SDL2/framebuffer/OpenGL) que exponga un buffer lineal de 8 bits con paleta, ya que el motor asume nativamente **256 colores indexados** en toda su cadena de render (paletas DAC, `t.asm`, `draw_*a.asm`).
2. **Audio**: `divsb.cpp` (Sound Blaster por DMA/puertos crudos) es 100% no portable; requeriría una capa de audio moderna (SDL_mixer/miniaudio) reimplementando el mezclador (`divmixer`/`divsound`) sobre un backend de audio real, conservando el formato de datos (muestras PCM) pero no el driver.
3. **Red**: `ipxlib.c` está atado a IPX real-mode vía DPMI (`INT 7Ah`/`INT 2Fh`); IPX lleva décadas obsoleto y esta capa se sustituiría enteramente por sockets TCP/UDP, reimplementando la semántica de sesión (`IPX_Users`, `IPX_TimeOut`, broadcast) sobre el nuevo transporte.
4. **Timer/reloj e interrupciones**: `timer.asm`/`divtimer.cpp` (reprogramación de PIT+PIC) no tiene sentido en un SO moderno; se sustituiría por temporizadores del SO (hilos, `timerfd`, `SetTimer`, etc.).
5. **Entrada (joystick/ratón)**: la lectura de joystick por RC/puerto de juego (`f.cpp`) y el manejo de ratón vía interrupciones DOS necesitan reemplazarse por APIs de input modernas (SDL2 `GameController`/`Mouse`).
6. **Carga de "DLLs"**: aunque `pe_load.c` es un loader PE, es demasiado mínimo y específico del ABI de Watcom/DOS de 32 bits (no procesa importaciones, asume `Machine==I386` y cabecera opcional de exactamente 224 bytes) como para reutilizarse tal cual sobre un PE32/PE32+ o ELF modernos; lo más razonable sería sustituirlo por el mecanismo de carga dinámica nativo del SO (`LoadLibrary`/`dlopen`) recompilando las DLLs de DIV para la plataforma destino, conservando solo la *idea* del pool `DIV_export`/`DIV_import` como capa de indirección de símbolos entre el host y las DLLs.

### Partes relativamente aislables (con matices confirmados en el código)

El contexto de partida planteaba la hipótesis de que la VM del intérprete podría estar separada de la I/O si `f.cpp`/`s.cpp`/`v.cpp` estuvieran bien encapsulados. La revisión del código la confirma **solo parcialmente**:

- **`s.cpp`** (funciones de sprites, texto y scroll): **0 coincidencias** de `outp`/`inp`/`int86`/`_dos_*`/`int386` en todo el fichero. Es rasterización de software pura (blitting, escaneo de sprites, `pinta_modo7`) que opera sobre buffers en memoria y delega el volcado final a otras rutinas — este módulo **sí parece razonablemente aislable** de la capa de hardware.
- **`v.cpp`** (funciones de vídeo): **30 coincidencias**. Contrario a lo que su nombre podría sugerir como "solo lógica de vídeo", este fichero escribe directamente a los registros del controlador CRTC/Sequencer de la VGA (`CRTC_INDEX`, `SC_INDEX`, `MISC_OUTPUT`) para programar modos de vídeo personalizados (320x240, 320x400, 360x240, 360x360, etc., no estándar de BIOS) — **no está aislado del hardware**, es la capa que directamente lo maneja.
- **`f.cpp`** (funciones internas del intérprete — el mayor módulo, con cientos de sentencias del lenguaje DIV): **31 coincidencias**, correspondientes a lectura de joystick por puertos, llamadas DOS de fichero/unidad (`_dos_findfirst`, `_dos_setdrive`, etc.) y una llamada DPMI (`int386x(0x31,...)`) para operaciones de disco. Esto significa que **las primitivas del lenguaje que tocan hardware/DOS están mezcladas en el mismo fichero que el resto de las ~centenares de funciones "puras" del lenguaje** (matemáticas, control de flujo, manipulación de datos) — no hay una frontera de módulo entre "opcode puro" y "opcode con I/O" dentro de `f.cpp`, aunque sí están relativamente concentradas en secciones identificables del fichero (bloque de joystick en torno a la línea 2400-2463; bloque de ficheros/unidades disperso).
- **`i.cpp`** (bucle principal del intérprete/dispatch de bytecode): solo **11 coincidencias**, y todas ellas relacionadas con búsqueda del path de instalación de DIV y cambio de unidad activa (`_dos_setdrive`, `_dos_findfirst` para localizar el ejecutable/recursos) — **no** en el bucle de despacho de opcodes en sí. Esto sugiere que el núcleo de ejecución de bytecode (el "intérprete" propiamente dicho, distinto de sus funciones nativas) sí está razonablemente desacoplado del hardware, y que el acoplamiento real está concentrado en las funciones nativas invocadas desde `f.cpp`.
- **`kernel.cpp`, `ia.cpp`, `c.cpp`**: **0 coincidencias** — buenos candidatos a lógica portable sin cambios (aunque no se leyó su contenido completo, solo se verificó ausencia de las llamadas buscadas; podrían tener dependencias indirectas vía cabeceras que sí las tengan).

**Conclusión práctica para un port**: la hipótesis de encapsulamiento limpio se cumple a nivel de *bucle de intérprete* (`i.cpp` dispatch) y de *rasterización de software* (`s.cpp`), pero **no** en `v.cpp` (que es en sí mismo la capa de hardware de vídeo, no algo que se pueda dejar tal cual) ni completamente en `f.cpp` (donde la I/O de hardware/DOS está entremezclada con la implementación de cientos de funciones nativas del lenguaje, aunque agrupada en secciones localizables). Un port realista debería: (a) dejar el dispatcher de `i.cpp` prácticamente intacto, (b) reescribir `v.cpp` desde cero contra una API gráfica moderna manteniendo su mismo contrato de buffer indexado de 8 bits, (c) revisar función por función `f.cpp` para extraer las ~10-20 funciones nativas que tocan hardware/DOS (joystick, disco) y sustituirlas por sus equivalentes modernos sin tocar el resto, y (d) reutilizar `s.cpp` casi sin cambios al ser software puro sobre buffers.

## Archivos leídos

- `README.md`
- `.travis/install_watcom.sh`
- `makefile` (raíz)
- `.clang-format`
- `src/a.asm`
- `src/timer.asm`
- `src/vesa.asm` (parcial, primeras ~150 líneas + grep de procedimientos)
- `src/div/visor/t.asm` (grep de etiquetas/procedimientos)
- `src/vpe/draw_fa.asm`
- `src/vpe/draw_oa.asm` (grep de etiquetas/procedimientos)
- `src/vpe/draw_wa.asm` (grep de etiquetas/procedimientos)
- `src/div/divtimer.cpp`
- `src/div/det_vesa.cpp`
- `src/div/divsb.cpp`
- `src/pe_load.c`
- `src/divdll.c`
- `src/netlib/ipxlib.c` (parcial, primeras ~310 líneas)
- `src/netlib/dpmi_net.c`
- `src/vpe/hard.cpp`
- `src/div_stub/asm/exec.asm`
- `src/div32run/f.cpp` (grep dirigido + fragmentos)
- `src/div32run/v.cpp` (fragmento inicial)
- `src/div32run/s.cpp` (fragmento inicial)
- `src/div32run/i.cpp` (grep dirigido)
- `docs/DIV dependency graph.graphml` (cabecera + conteo de nodos/aristas + extracción de etiquetas, no parseado en profundidad)
- `docs/architecture/01-vision-general.md`
- `pmwlite/file_id.diz`

**Ficheros localizados pero no leídos en profundidad** (mencionados por completitud): `src/cpuid.asm`, `src/div_stub/asm/cstrt086.asm`, `src/install2/mouse.c`, `src/div32run/kernel.cpp`, `src/div32run/ia.cpp`, `src/div32run/c.cpp`, `src/div32run/d.cpp`, `docs/DIV dependency graph.pdf`.
