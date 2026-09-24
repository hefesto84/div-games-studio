# libmikmod 3.3.14 vendored (sezero/mikmod) -- solo el subconjunto que usa
# el port DIV (musica de tracker MOD/S3M/XM), compilado como lib estatica.
#
# Se venden unicamente las fuentes necesarias en vez del arbol completo:
#   - loaders de los 3 formatos que soporta DIV (load_s3m/load_mod/load_xm);
#     NO se usa MikMod_RegisterAllLoaders() (mlreg.c registra los 19) sino
#     MikMod_RegisterLoader() explicito desde port/io/io_audio.c.
#   - driver winmm (drv_win) + drv_nos (mdreg lo registra incondicionalmente).
#   - playercode+mmio completos (playercode/virtch*.c necesitan su .c common)
#     y posix/strcasecmp.c (_mm_strcasecmp, usado por mdriver.c).
#
# Se compila con config.h propio (incluido en include/) + HAVE_CONFIG_H.
# La configuracion "hermosa" del upstream (config.h.cmake/CMake) se sustituye
# por ese config.h fijo para MSVC (ver su cabecera). En MSVC, drv_win.c
# autolinea winmm.lib via #pragma comment(lib,"winmm.lib").

cmake_minimum_required(VERSION 3.16)

project(mikmod C)

add_library(mikmod STATIC
    # drivers
    src/drivers/drv_win.c
    src/drivers/drv_nos.c
    # loaders (solo MOD/S3M/XM)
    src/loaders/load_mod.c
    src/loaders/load_s3m.c
    src/loaders/load_xm.c
    # mmio
    src/mmio/mmio.c
    src/mmio/mmalloc.c
    src/mmio/mmerror.c
    # playercode
    src/playercode/mdreg.c
    src/playercode/mdriver.c
    src/playercode/mdulaw.c
    src/playercode/mloader.c
    src/playercode/mlutil.c
    src/playercode/mplayer.c
    src/playercode/munitrk.c
    src/playercode/mwav.c
    src/playercode/npertab.c
    src/playercode/sloader.c
    src/playercode/virtch.c
    src/playercode/virtch2.c
    src/playercode/virtch_common.c
    # posix (proveedor de _mm_strcasecmp para mdriver.c)
    src/posix/strcasecmp.c
)

target_include_directories(mikmod PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# HAVE_CONFIG_H: hace que los .c y mikmod_internals.h incluyan nuestro
# config.h. MIKMOD_STATIC: mikmod.h NO incluye config.h (solo lo hacen
# mikmod_internals.h y los .c); la API externa del header publico se declara
# __declspec(dllimport) salvo que la macro llegue por aqui, tanto en la lib
# como en los consumidores que compilan con sus headers (io_song.c).
target_compile_definitions(mikmod PUBLIC HAVE_CONFIG_H MIKMOD_STATIC)

if(MSVC)
    # silenciar warnings tipicos de las fuentes (fopen/strcpy)
    target_compile_options(mikmod PRIVATE /wd4996)
    target_compile_definitions(mikmod PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()