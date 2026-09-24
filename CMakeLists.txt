cmake_minimum_required(VERSION 3.16)

# ============================================================================
#  DIV Games Studio 2 - Port a Windows nativo con raylib (Fase 0)
# ============================================================================
#  Este CMakeLists.txt es la raiz del PORT moderno (ver
#  docs/architecture/11-port-windows11-mikedx.md, "Fase 0 - Toolchain y
#  esqueleto de proyecto"). El build original DOS sigue viviendo en el
#  makefile Watcom de la raiz y NO se ve afectado por este proyecto.
#
#  Decidido (fase 0): toolchain MSVC (VS 2022 Build Tools) sobre MinGW
#  (no instalado, y MSVC da mejor integracion con Visual Studio).
# ============================================================================

project(DIV_Port VERSION 0.1.0 LANGUAGES C CXX)

# Ruta a la distribucion precompilada de raylib (headers + .dll + .lib).
# Se configura con -DRAYLIB_PATH=... o desde el cache de CMake.
# El default depende de la arquitectura de build: win64_msvc16 para x64,
# win32_msvc16 para Win32.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_RAYLIB_DEFAULT "C:/Users/Dani/Documents/inter-test/raylib/raylib-5.0_win64_msvc16")
else()
    set(_RAYLIB_DEFAULT "C:/Users/Dani/Documents/inter-test/raylib/raylib-5.0_win32_msvc16")
endif()
set(RAYLIB_PATH "${_RAYLIB_DEFAULT}"
    CACHE PATH "Root de la distribucion precompilada de raylib (con include/ y lib/)")

if(NOT EXISTS "${RAYLIB_PATH}/include/raylib.h")
    message(FATAL_ERROR "raylib no encontrado en ${RAYLIB_PATH}. Pasa -DRAYLIB_PATH=<ruta> apuntando a una distribucion precompilada de raylib.")
endif()

# ---------------------------------------------------------------------------
#  Port: esqueleto + capa de I/O sobre raylib (Fase 1)
# ---------------------------------------------------------------------------
add_subdirectory(3rdparty/mikmod)

add_executable(div_port
    port/main.c
    port/io/div_io.h
    port/io/io_video.c
    port/io/io_audio.c
    port/io/io_song.c
    port/io/io_input.c
    port/io/io_keymap.c
    port/io/io_keyboard.c
    port/io/io_timer.c
)

target_include_directories(div_port PRIVATE
    "${RAYLIB_PATH}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/io"
)
target_link_libraries(div_port PRIVATE "${RAYLIB_PATH}/lib/raylibdll.lib" mikmod)

if(MSVC)
    # Raylib usa formatos de printf de C99 en raylib.h con MSVC
    target_compile_options(div_port PRIVATE /wd4996)
    # IMPORTANTE (checkpoint 11): definir USE_LIBTYPE_SHARED hace que
    # raylib.h declare toda su API como __declspec(dllimport) (linea 98),
    # de modo que las llamadas se resuelven SIEMPRE contra raylib.dll via
    # __imp_<name>. Sin este define, los nombres de raylib que coinciden
    # con simbolos del nucleo DIV (PlaySound/StopSound/UnloadSound en
    # divsound.cpp) se enlazaban contra div_core en vez de contra raylib,
    # y raylib nunca llegaba a reproducir nada. Ver 13-handoff.md.
    target_compile_definitions(div_port PRIVATE USE_LIBTYPE_SHARED HAVE_CONFIG_H MIKMOD_STATIC)
    # HAVE_CONFIG_H/MIKMOD_STATIC: libmikmod -- HAVE_CONFIG_H hace que
    # mikmod_internals.h lea nuestro config.h; MIKMOD_STATIC evita que
    # mikmod.h declare la API como dllimport (__imp_*) y falle el enlace
    # con la lib estatica. Se definen aqui (no solo en el target mikmod)
    # porque io_song.c compila dentro de este ejecutable.
    # Mesa de trabajo limpia (raylib.h no requiere _CRT_SECURE_NO_WARNINGS si se ignora 4996)
endif()

# Copiar raylib.dll junto al ejecutable para que el binario sea autoejecutable
add_custom_command(TARGET div_port POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_PATH}/lib/raylib.dll"
            "$<TARGET_FILE_DIR:div_port>"
    COMMENT "Copiando raylib.dll junto a div_port.exe"
)

# ---------------------------------------------------------------------------
#  Nucleo de DIV32RUN portado (i.cpp/kernel.cpp/f.cpp/divlengu.cpp), sin
#  tocar salvo dos ediciones minimas documentadas (ver
#  docs/architecture/12-port-progreso.md). Todavia SIN enlazar a nada: es
#  una biblioteca objeto para validar que compila via CMake (no solo con
#  cl.exe a mano) mientras se completa la Fase 1. kernel.cpp se compila
#  dentro de i.cpp via #include, no como fuente aparte.
# ---------------------------------------------------------------------------
# zlib 1.2.11 (3rdparty/zlib) no trae ninguna .lib compilada para MSVC/x64
# -- solo las precompiladas para Watcom 386/586 (ver
# docs/architecture/05-dependencias-terceros.md). f.cpp/divc.cpp usan
# compress()/uncompress() (API simple, no streaming). Se compila el
# subconjunto estandar de zlib (sin gz*.c/infback.c, que son para la API
# de fichero .gz, no usada aqui) directamente como fuente propia -- es
# C ANSI portable sin ninguna dependencia de DOS/hardware.
set(ZLIB_SOURCES
    3rdparty/zlib/adler32.c
    3rdparty/zlib/compress.c
    3rdparty/zlib/crc32.c
    3rdparty/zlib/deflate.c
    3rdparty/zlib/inflate.c
    3rdparty/zlib/inftrees.c
    3rdparty/zlib/inffast.c
    3rdparty/zlib/trees.c
    3rdparty/zlib/uncompr.c
    3rdparty/zlib/zutil.c
)

# Motor Modo-8 (VPE) portado -- ver port/vpe/. hard.c/gfx.c/draw_span.c son
# reemplazos en C limpio de las partes que eran DOS/ASM real en el original
# (src/vpe/hard.cpp, gfx.cpp, draw_wa.asm/draw_fa.asm/draw_oa.asm); el resto
# son copias casi literales de src/vpe/ (portable ya en origen) mas
# vpedll.c, la implementacion real de los opcodes Modo-8.
set(VPE_SOURCES
    port/vpe/hard.c
    port/vpe/gfx.c
    port/vpe/mem.c
    port/vpe/fixed.c
    port/vpe/globals.c
    port/vpe/zone.c
    port/vpe/load.c
    port/vpe/object.c
    port/vpe/update.c
    port/vpe/vpe.c
    port/vpe/scan.c
    port/vpe/view.c
    port/vpe/draw_sw.c
    port/vpe/draw_cw.c
    port/vpe/draw_f.c
    port/vpe/draw_o.c
    port/vpe/draw_span.c
    port/vpe/vpedll.c
)

add_library(div_core OBJECT
    port/div/core/i.cpp
    port/div/core/f.cpp
    port/div/core/s.cpp
    port/div/core/v.cpp
    port/div/core/det_vesa.cpp
    port/div/core/mouse.cpp
    port/div/core/divkeybo.cpp
    port/div/core/divlengu.cpp
    port/div/core/divsound.cpp
    port/div/core/divfli.cpp
    port/div/core/port_topflc.c
    port/div/core/ia.c
    port/div/core/port_stubs.c
    port/div/core/port_res_path.c
    port/div/core/port_cierre.c
    port/div/core/port_misc.cpp
    ${VPE_SOURCES}
    ${ZLIB_SOURCES}
)

target_include_directories(div_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/shim"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/vpe"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/judas"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/topflc"
)

if(MSVC)
    # i.cpp/f.cpp/kernel.cpp/divlengu.cpp no usan ninguna construccion real
    # de C++ pese a la extension .cpp; compilarlos como C++ estricto genera
    # decenas de errores de conversion implicita (char*/unsigned char*,
    # void*/puntero-a-funcion) que Watcom permitia sin mas. Ver "3. Receta
    # exacta de compilacion" en docs/architecture/12-port-progreso.md.
    # port_misc.cpp SI es C++ real (usa extern "C"), se excluye del /TC.
    set_source_files_properties(
        port/div/core/i.cpp
        port/div/core/f.cpp
        port/div/core/s.cpp
        port/div/core/v.cpp
        port/div/core/det_vesa.cpp
        port/div/core/mouse.cpp
        port/div/core/divkeybo.cpp
        port/div/core/divlengu.cpp
        port/div/core/divsound.cpp
        port/div/core/divfli.cpp
        PROPERTIES
            LANGUAGE C
            COMPILE_OPTIONS "/TC;/FIport_pre.h"
    )
    # port_stubs.c e ia.c ya son .c (no necesitan /TC) pero SI necesitan el
    # force-include de port_pre.h, igual que el resto del nucleo. Los
    # ficheros de port/vpe/ tambien -- load.c/object.c/hard.c/vpedll.c
    # incluyen "inter.h" y necesitan el mismo orden de cabeceras CRT antes
    # de sus #define de campo (ver el comentario grande en port_pre.h).
    set_source_files_properties(
        port/div/core/port_stubs.c
        port/div/core/ia.c
        ${VPE_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "/FIport_pre.h"
    )
    target_compile_definitions(div_core PRIVATE _CRT_SECURE_NO_WARNINGS USE_LIBTYPE_SHARED)
endif()

# ---------------------------------------------------------------------------
#  Prueba de humo (diagnostico, no forma parte del runtime del port): ver
#  port/div/core/smoke_test.c. Ejercita de verdad svmode/volcado/set_dac/
#  readmouse/tecla/detectar_vesa (ya portadas) sin necesitar el interprete
#  completo ni un fichero de bytecode. Recompila v.cpp/det_vesa.cpp/
#  mouse.cpp/divkeybo.cpp por separado (en vez de reusar los .obj de
#  div_core) para que este target quede autocontenido y no arrastre las
#  dependencias sin resolver de i.cpp/f.cpp (sonido, DLLs, Modo-8, ver
#  docs/architecture/12-port-progreso.md §9.3/§11.3).
# ---------------------------------------------------------------------------
add_executable(div_smoke_test
    port/div/core/smoke_test.c
    port/div/core/v.cpp
    port/div/core/det_vesa.cpp
    port/div/core/mouse.cpp
    port/div/core/divkeybo.cpp
    port/div/core/port_cierre.c
)

target_include_directories(div_smoke_test PRIVATE
    "${RAYLIB_PATH}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/shim"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/judas"
)
target_sources(div_smoke_test PRIVATE
    port/io/io_video.c
    port/io/io_audio.c
    port/io/io_input.c
    port/io/io_keymap.c
    port/io/io_keyboard.c
    port/io/io_timer.c
)
target_link_libraries(div_smoke_test PRIVATE "${RAYLIB_PATH}/lib/raylibdll.lib")

if(MSVC)
    # /FIport_pre.h tambien en smoke_test.c: aunque ya incluye "port_pre.h"
    # el mismo (es idempotente por sus include guards), v.cpp/det_vesa.cpp/
    # mouse.cpp/divkeybo.cpp NO lo incluyen por si mismos (en div_core lo
    # reciben via este mismo /FI) -- sin el, cada uno dispararia de nuevo
    # la colision de macro _Size vs <time.h> ya documentada en
    # docs/architecture/12-port-progreso.md §2.8.
    set_source_files_properties(
        port/div/core/smoke_test.c
        port/div/core/v.cpp
        port/div/core/det_vesa.cpp
        port/div/core/mouse.cpp
        port/div/core/divkeybo.cpp
        TARGET_DIRECTORY div_smoke_test
        PROPERTIES
            LANGUAGE C
            COMPILE_OPTIONS "/TC;/FIport_pre.h"
    )
    target_compile_definitions(div_smoke_test PRIVATE _CRT_SECURE_NO_WARNINGS USE_LIBTYPE_SHARED)
    target_compile_options(div_smoke_test PRIVATE /wd4996)
endif()

add_custom_command(TARGET div_smoke_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_PATH}/lib/raylib.dll"
            "$<TARGET_FILE_DIR:div_smoke_test>"
    COMMENT "Copiando raylib.dll junto a div_smoke_test.exe"
)

# ---------------------------------------------------------------------------
#  div32run_port -- el ejecutable REAL del runtime portado: div_core (i.cpp
#  ya trae su propio main(), no hace falta escribir uno) + port/io. Antes de
#  este checkpoint el enlace se habia hecho siempre a mano con link.exe como
#  diagnostico (ver docs/architecture/12-port-progreso.md §9.3/§11.3); este
#  target lo hace reproducible via "cmake --build . --target div32run_port".
#  Ver checkpoint 8 (§15) en ese mismo documento para la verificacion
#  completa (banner real + bytecode de prueba ejecutado de punta a punta).
# ---------------------------------------------------------------------------
add_executable(div32run_port $<TARGET_OBJECTS:div_core>)

target_sources(div32run_port PRIVATE
    port/io/io_video.c
    port/io/io_audio.c
    port/io/io_song.c
    port/io/io_input.c
    port/io/io_keymap.c
    port/io/io_keyboard.c
    port/io/io_timer.c
)

target_include_directories(div32run_port PRIVATE
    "${RAYLIB_PATH}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/shim"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/judas"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mikmod/include"
)
target_link_libraries(div32run_port PRIVATE "${RAYLIB_PATH}/lib/raylibdll.lib" mikmod)

if(MSVC)
    set_source_files_properties(
        port/io/io_video.c
        port/io/io_audio.c
        port/io/io_song.c
        port/io/io_input.c
        port/io/io_keymap.c
        port/io/io_keyboard.c
        port/io/io_timer.c
        TARGET_DIRECTORY div32run_port
        PROPERTIES COMPILE_OPTIONS "/wd4996"
    )
    target_compile_definitions(div32run_port PRIVATE _CRT_SECURE_NO_WARNINGS USE_LIBTYPE_SHARED HAVE_CONFIG_H MIKMOD_STATIC)
    # HAVE_CONFIG_H/MIKMOD_STATIC: igual que div_port -- evita el dllimport
    # de la API de libmikmod en io_song.c.
endif()

add_custom_command(TARGET div32run_port POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_PATH}/lib/raylib.dll"
            "$<TARGET_FILE_DIR:div32run_port>"
    COMMENT "Copiando raylib.dll junto a div32run_port.exe"
)

# ---------------------------------------------------------------------------
#  Compilador DIV portado (divc.cpp, hito C1): biblioteca OBJECT que valida
#  que el compilador compila con MSVC. divc.cpp/divlengu.cpp son copias de
#  src/div con ediciones minimas documentadas con comentarios "PORT:"; la
#  global.h de este directorio es una copia recortada (sin DOS/judas/map3d)
#  de la del IDE. El driver CLI (divc_port) llega en el hito C2.
# ---------------------------------------------------------------------------
add_library(divc_core OBJECT
    port/div/compiler/divc.cpp
    port/div/compiler/divlengu.cpp
    ${ZLIB_SOURCES}
)

target_include_directories(divc_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/compiler"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib"
)

if(MSVC)
    # Mismo criterio que div_core: los .cpp del codigo original son C de
    # facto; /TC evita los errores de conversion implicita de C++ estricto.
    set_source_files_properties(
        port/div/compiler/divc.cpp
        port/div/compiler/divlengu.cpp
        PROPERTIES
            LANGUAGE C
            COMPILE_OPTIONS "/TC;/FIport_compiler_pre.h"
    )
    target_compile_definitions(divc_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
#  divc_port -- driver CLI del compilador (hito C2): divc_main.c (carga el
#  .prg en source_ptr, llama a comp() y renombra system\EXEC.EXE) + stubs
#  del IDE + los objetos de divc_core. No necesita raylib: el compilador es
#  un programa de consola puro. Hay que ejecutarlo con cwd = raiz del repo
#  (rutas relativas system\ltlex.def, system\EXEC.EXE, ...).
# ---------------------------------------------------------------------------
add_executable(divc_port
    port/div/compiler/divc_main.c
    port/div/compiler/port_compiler_stubs.c
    $<TARGET_OBJECTS:divc_core>
)

target_include_directories(divc_port PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/compiler"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib"
)

if(MSVC)
    set_source_files_properties(
        port/div/compiler/divc_main.c
        port/div/compiler/port_compiler_stubs.c
        PROPERTIES COMPILE_OPTIONS "/FIport_compiler_pre.h"
    )
    target_compile_definitions(divc_port PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
#  IDE DIV portado (E1 en adelante): shell + ventanas + editor .prg y
#  editores de recursos. Se compilan los .cpp originales como C (/TC) con
#  force-include port_ide_pre.h; los headers de port/div/ide tienen prioridad
#  sobre src/div para poder adaptar global.h, div.h y svga.h sin tocar src/.
#  Fase E1: objetivo compilar y enlazar; no tiene por que arrancar aun.
# ---------------------------------------------------------------------------
set(DIV_IDE_SOURCES
    # Nucleo UI de E1 (shell + ventanas + editor de codigo + primitivas)
    src/div/div.cpp
    src/div/divwindo.cpp
    src/div/divhandl.cpp
    src/div/divedit.cpp
    src/div/divbasic.cpp
    src/div/divdsktp.cpp
    src/div/mem.cpp
    src/div/divlengu.cpp
    src/div/divcolor.cpp
    src/div/divgama.cpp
    src/div/divbin.cpp
    src/div/divmouse.cpp
    src/div/divpaint.cpp
    src/div/divpalet.cpp
    # E3: navegador de ficheros (browser0) y editor de fuentes/texto (ShowText)
    src/div/divbrow.cpp
    src/div/divfont.cpp
    # E6a: editor FPG (divfpg) + E/S de ficheros FPG (fpgfile)
    #      + importacion de imagenes PCX/BMP/MAP (divforma)
    src/div/divfpg.cpp
    src/div/fpgfile.cpp
    src/div/divforma.cpp
    # E6b: Sistema -> Modo de video / Configuracion / Memoria / Fondo
    #      (divsetup: Vid_Setup*, Tap_Setup*, MemInfo*, Cfg_Setup*)
    src/div/divsetup.cpp
    # E7 (audio del IDE, WIP): editor de sonido/PCM (divpcm)
    src/div/divpcm.cpp
    # E8 (WIP): sistema de ayuda hipertexto (divhelp)
    src/div/divhelp.cpp
    # E9 (WIP): calculadora (divcalc)
    src/div/divcalc.cpp
    # E10 (WIP): generador de explosiones (diveffec)
    src/div/diveffec.cpp
    # INVESTIGACION Modo-8: divmap3d.cpp (sin commitear, solo para medir errores)
    src/div/divmap3d.cpp
    # Stubs para el resto de modulos (audio, editores de recursos)
    port/div/ide/port_ide_stubs.c
    # E7: shim de compatibilidad JUDAS -> port/io/io_audio.c + io_song.c
    port/div/ide/port_ide_judas.c
    # E2.1: video real (sustituye a divvideo.cpp/det_vesa.cpp), puentes DOS
    # y equivalentes en C de las rutinas ASM de src/a.asm
    port/div/ide/port_ide_video.c
    port/div/ide/port_ide_dos.c
    port/div/ide/port_ide_asm.c
    port/div/ide/port_ide_mem.c
    port/div/ide/port_ide_setup.c
    # E2.3: teclado real (sustituye a divkeybo.cpp, que vive sobre la IRQ 9)
    port/div/ide/port_ide_keybo.c
    # E4: Compilar/Probar -> divc_port.exe / div32run_port.exe
    port/div/ide/port_ide_compila.c
    # GENSPR: generador de sprites 3D (Mapas -> Generador de sprites).
    # src/div/visor/ es el motor 3D propio de DIV (matematica de
    # transformacion/proyeccion + render offline de un solo disparo, sin
    # bucle de eventos ni dependencia de video/VESA) + divspr.cpp/divsprit.cpp
    # (integracion con dialogos del IDE). Del t.asm original (nucleo de
    # mapeado de texturas) solo hacia falta reescribir 2 rutinas -- ver
    # port_ide_genspr_t.c. main.cpp (driver de demo standalone con VESA/
    # conio/i86) no se incluye: huerfano, no lo usa divspr.cpp.
    src/div/visor/animated.cpp
    src/div/visor/complex.cpp
    src/div/visor/fileanim.cpp
    src/div/visor/global.cpp
    src/div/visor/hlrender.cpp
    src/div/visor/llrender.cpp
    src/div/visor/resource.cpp
    src/div/visor/sprite3d.cpp
    src/div/visor/visor.cpp
    port/div/ide/port_ide_genspr_t.c
    src/div/divspr.cpp
    src/div/divsprit.cpp
)

add_executable(div_ide_port ${DIV_IDE_SOURCES})

# Capa de I/O (raylib). Se compila aparte de DIV_IDE_SOURCES porque no debe
# llevar el force-include de port_ide_pre.h (windows.h y raylib.h chocan).
# io_audio.c/io_song.c (audio del IDE, E7): mismo motivo que en div32run_port
# -- io_song.c incluye mikmod.h, que a su vez trae windows.h.
target_sources(div_ide_port PRIVATE
    port/io/io_video.c
    port/io/io_audio.c
    port/io/io_song.c
    port/io/io_input.c
    port/io/io_keymap.c
    port/io/io_keyboard.c
    port/io/io_timer.c
)

target_include_directories(div_ide_port PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/port/div/ide"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/div"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/div/visor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty"
    "${RAYLIB_PATH}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mikmod/include"
)

target_link_libraries(div_ide_port PRIVATE "${RAYLIB_PATH}/lib/raylibdll.lib" mikmod)

# --------------------------------------------------------------------------
#  CRITICO (E2.1): el IDE guarda punteros a funcion y a heap dentro de `int`
#  (`v.paint_handler=(int)func;`, la tabla hash del coloreador en divcolor.cpp,
#  etc.) -- un patron gratis en el DOS 32-bit original y mortal en x64, donde
#  el cast trunca los 32 bits altos (306 avisos C4311).
#
#  En vez de reescribir ~300 puntos de src/div, se fuerza que TODO el proceso
#  viva por debajo de 2 GB, con lo que la truncacion es reversible:
#    /LARGEADDRESSAWARE:NO -> el SO no entrega direcciones por encima de 2 GB
#    /BASE:0x10000000      -> imagen en 0x10000000
#    /DYNAMICBASE:NO /FIXED-> sin ASLR (la base pedida es la real)
#  Verificado: codigo en 0x10001000 y todas las reservas de heap < 2 GB.
# --------------------------------------------------------------------------
if(MSVC)
    target_link_options(div_ide_port PRIVATE
        /LARGEADDRESSAWARE:NO
        /BASE:0x10000000
        /DYNAMICBASE:NO
        /FIXED
    )
endif()

if(MSVC)
    set_source_files_properties(${DIV_IDE_SOURCES}
        PROPERTIES
            LANGUAGE C
            COMPILE_OPTIONS "/TC;/FIport_ide_pre.h"
    )
    # port_ide_stubs.c ya es .c, no necesita /TC pero si el force-include
    set_source_files_properties(
        port/div/ide/port_ide_stubs.c
        port/div/ide/port_ide_video.c
        port/div/ide/port_ide_dos.c
        port/div/ide/port_ide_asm.c
        port/div/ide/port_ide_mem.c
        port/div/ide/port_ide_setup.c
        port/div/ide/port_ide_keybo.c
        TARGET_DIRECTORY div_ide_port
        PROPERTIES COMPILE_OPTIONS "/FIport_ide_pre.h"
    )
    set_source_files_properties(
        port/io/io_video.c
        port/io/io_audio.c
        port/io/io_song.c
        port/io/io_input.c
        port/io/io_keymap.c
        port/io/io_keyboard.c
        port/io/io_timer.c
        TARGET_DIRECTORY div_ide_port
        PROPERTIES COMPILE_OPTIONS "/wd4996"
    )
    # divpaint.cpp define una funcion zoom_porcion() y divbasic.cpp una
    # variable int homonima. En el C++ original convivian (la funcion iba
    # mangled); al compilar como C chocan. Se renombra solo en este TU.
    set_source_files_properties(src/div/divpaint.cpp
        TARGET_DIRECTORY div_ide_port
        PROPERTIES COMPILE_DEFINITIONS "zoom_porcion=zoom_porcion_paint")

    target_compile_definitions(div_ide_port PRIVATE _CRT_SECURE_NO_WARNINGS USE_LIBTYPE_SHARED PORT_IDE HAVE_CONFIG_H MIKMOD_STATIC)
    # HAVE_CONFIG_H/MIKMOD_STATIC: igual que div32run_port -- evita el
    # dllimport de la API de libmikmod en io_song.c.
    target_compile_options(div_ide_port PRIVATE /wd4996)
endif()

add_custom_command(TARGET div_ide_port POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_PATH}/lib/raylib.dll"
            "$<TARGET_FILE_DIR:div_ide_port>"
    COMMENT "Copiando raylib.dll junto a div_ide_port.exe"
)
