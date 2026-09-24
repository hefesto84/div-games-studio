# Estructura del repositorio

Árbol verificado contra el estado actual del repositorio (`C:\Users\Dani\Documents\DIV`):

```
DIV/
├── makefile                  # Build maestro (wmake / OpenWatcom)
├── os.mif                    # Detección de SO anfitrión para el build
├── common.mif                # Comandos de copia/borrado según SO
├── 3rdparty.mif              # Include de variables/targets de libs 3ª parte
├── c.lnk / cdbg.lnk          # Scripts del linker Watcom (release/debug)
├── .clang-format             # Estilo de formato C++
├── .travis.yml               # CI (Windows + Linux con OpenWatcom)
├── Vagrantfile / vagrant/    # Entorno reproducible de compilación
│
├── src/                      # Código fuente principal (C/C++ + ASM x86)
│   ├── div/                  #   IDE completo -> D.EXE / D.386
│   │   └── visor/            #     Generador de sprites 3D (Pentium)
│   ├── div32run/              #   Intérprete/virtual machine -> DIV32RUN
│   │   ├── vpe/               #     Motor gráfico "Modo-8" (copia local)
│   │   └── netlib/             #    Red (IPX, copia local)
│   ├── install/                #   Instalador de juegos -> install.ovl
│   ├── install2/               #   Instalador alternativo (C puro)
│   ├── wstub/                  #   Stub 16-bit (cabecera del EXE de DIV)
│   ├── div_stub/                #  Stub alternativo (DESHABILITADO)
│   ├── netlib/                  #  Rutinas de red (build heredado, raíz de src)
│   ├── vpe/                     #  Librería Modo-8 compartida (raíz de src)
│   └── *.c / *.h / *.asm        # Código compartido: divdll, pe_load, pedebug,
│                                 #   vesa, timer, lfbprof, sysdac, cpuid, a.asm
│
├── dll/                       # SDK de DLL para el lenguaje DIV + ejemplos
│   ├── div.h                  #   Cabecera pública del SDK
│   ├── agua.cpp / hboy.cpp / ss1.cpp   # DLLs de ejemplo
│   └── demo0.cpp / demo1.cpp / demo2.cpp  # Demos de uso del SDK
│
├── formats/                   # Especs de formatos de archivo (Kaitai .ksy)
├── 3rdparty/                  # Librerías de terceros (código + .lib precompiladas)
│   ├── jpeglib/                #  JPEG (IJG)
│   ├── judas/                  #  Sonido (JUDAS Apocalyptic Sound System)
│   ├── scitech/                #  SuperVGA Kit
│   ├── topflc/                 #  FLI/FLC playback
│   ├── zlib/                   #  Compresión
│   └── lib/{386,586}/          #  .lib precompiladas por CPU
│
├── pmwlite/                    # Extensor 32-bit PMODE/W (DOS) + pmodew.exe
├── genspr/                     # Datos para generación de sprites
├── help/                       # Datos del sistema de ayuda embebido
├── install/                    # Assets gráficos del instalador
├── resource/                   # Recursos runtime: fnt, fpg, ifs, map, pal, prg, wld
├── setup/                      # Assets del programa de configuración
├── system/                     # Archivos del sistema de DIV
├── docs/                       # Documentación del repo
│   ├── architecture/            #  Esta documentación ampliada
│   └── "DIV dependency graph".graphml / .pdf   # Grafo de dependencias entre módulos
└── tools/                       # Herramientas accesorias (build/test/debug)
    ├── bin2h/                   #  Binario -> cabecera C
    ├── testdll/                 #  Probador de DLLs
    ├── unpak/                   #  Extractos de PAK
    └── wld/                     #  wld2zon / wlddbg (mapas WLD)
```

## Notas sobre la estructura

- **Duplicación deliberada de `netlib/` y `vpe/`**: existen tanto en `src/` (raíz) como dentro de `src/div32run/`. Esto probablemente refleja el historial del proyecto (antes DLLs/librerías separadas, ver comentarios de ARCHITECTURE.md sobre "build heredado") — no confundir ambas copias al editar.
- **`src/div_stub/` está deshabilitado** intencionalmente: cualquier cambio de un solo byte en el stub rompe la compatibilidad binaria con el `DIV32RUN.DLL` ya distribuido con DIV2 comercial. El stub activo es `src/wstub/`.
- **`resource/` vs `genspr/`/`help/`/`setup/`/`system/`/`install/` (carpeta raíz)**: son árboles de *datos* (no código) que se copian al instalar DIV (`wmake install`), organizados por tipo de recurso o por herramienta que los consume.
- **`docs/architecture/`** es la carpeta añadida en esta revisión para alojar el research detallado por sección; el `ARCHITECTURE.md` de la raíz actúa como índice resumido que enlaza aquí.

## Ver también

- [03-componentes-principales.md](03-componentes-principales.md) — qué hace cada binario generado a partir de `src/`.
- [04-build-system.md](04-build-system.md) — cómo el `makefile` raíz recorre estas carpetas.
