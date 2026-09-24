# Visión general

## Qué es DIV Games Studio 2

DIV Games Studio 2 es un **entorno completo de desarrollo de videojuegos** para MS-DOS, publicado en 1999 por el estudio español **Hammer Technologies**, y licenciado por la británica **FastTrak** para su distribución en Europa y Latinoamérica. Integraba en un único programa (`D.EXE`, el "Sistema Operativo DIV™") casi todo lo necesario para hacer un juego:

- Editor gráfico (`divpaint.cpp`) y editor de sprites/FPG (`divsprit.cpp`, `divfpg.cpp`).
- Editor de código fuente (`divedit.cpp`).
- Un lenguaje de programación propio, **DIV**, con su propio compilador (`divc.cpp`).
- Un depurador integrado (`d.cpp`, dentro del intérprete en modo `SESSION=1`).
- Un intérprete/máquina virtual para ejecutar los juegos compilados (`DIV32RUN`).
- Un generador de instaladores para distribuir los juegos terminados (`src/install`).
- Un sistema de ayuda embebido (`divhelp.cpp`, datos en `help/`).

Tras el lanzamiento de DIV 2, Hammer Technologies cerró y el desarrollo oficial se detuvo. En 2016 el código fuente original fue liberado bajo **GPL v3** por MikeDX (ex miembro de FastTrak), dando lugar a distintos forks. Este repositorio ([vii1/DIV](https://github.com/vii1/DIV), fork de [DIVGAMES/DIV-Games-Studio](https://github.com/DIVGAMES/DIV-Games-Studio)) se centra específicamente en **restaurar fielmente la versión original de MS-DOS**, reordenando y documentando el código, en vez de portarlo a plataformas modernas (para eso existe la versión de MikeDX, que sí apunta a Windows/Mac/Linux/consolas/móviles/HTML5).

## Objetivo del proyecto (roadmap declarado)

Según el `README.md` y los milestones del repositorio:

1. **v2.01** — Reproducción fiel del DIV 2 comercial tal como se vendió en 1999 (milestone [2.01](https://github.com/vii1/DIV/milestone/1)).
2. **v2.02** — Corrección de bugs conocidos (algunos arrastrados desde hace décadas) y pulido de detalles de usabilidad (milestone [2.02](https://github.com/vii1/DIV/milestone/2)).
3. **Después** — Mejoras funcionales, reorganización de código y documentación continua.

Ver el detalle ampliado en [10-hoja-de-ruta.md](10-hoja-de-ruta.md).

## Plataforma objetivo

El entorno apunta a **MS-DOS real o emulado**:

- Requiere al menos un **486** (recomendado Pentium), 16 MB de RAM, ratón y tarjeta gráfica **SVGA**.
- Se ejecuta sobre DOS real o emuladores como **DOSBox** / **DOSBox-X**.
- El proyecto contempla explícitamente la posibilidad de extenderse a otros sistemas retro (ej. Amiga), pero **no** persigue un port nativo a sistemas modernos — eso queda fuera de alcance de este fork.

## Por qué importa la separación IDE / intérprete

Un punto de diseño central que atraviesa todo el resto de la documentación: DIV separa el **entorno de autor** (el IDE, `D.EXE`) del **entorno de ejecución** (`DIV32RUN`, la VM/intérprete de bytecode). El IDE se usa solo durante el desarrollo del juego; lo que se distribuye a los jugadores es un ejecutable autocontenido (stub + bytecode) que arrastra consigo una copia del intérprete (`DIV32RUN.DLL`) sin depurador ni herramientas de autor. Este diseño es la razón de ser de casi todas las decisiones de build (`SESSION=0` vs `SESSION=1`, ver [04-build-system.md](04-build-system.md)) y del pipeline de ejecución completo (ver [07-modelo-ejecucion.md](07-modelo-ejecucion.md)).

## Ver también

- [02-estructura-repositorio.md](02-estructura-repositorio.md) — organización de carpetas y archivos.
- [03-componentes-principales.md](03-componentes-principales.md) — detalle de cada binario/módulo.
- [09-estado-arbol.md](09-estado-arbol.md) — estado observado del repositorio a la fecha de esta revisión.
