# Assessment: port del IDE de DIV Games Studio 2 a Windows 11 nativo

Fecha: 2026-09-18  
Contexto: el runtime (`div32run_port`) y el compilador (`divc_port`) ya están portados y verificados (tags `0.0.6-runtime+api` y `0.0.10-compiler+corpus`). El siguiente bloque grande es el IDE (`div.cpp` y compañía).

## 1. Tamaño real del código fuente

`src/div/*.cpp` suma **~52.000 líneas**, pero gran parte no es crítica para un IDE útil:

| Fichero | Líneas | Notas |
|---|---|---|
| `divc.cpp` | 6.710 | ✅ Ya portado (compilador CLI) |
| `divfrm.cpp` | 6.072 | Muerto (formateador retirado en el proyecto) |
| `divpaint.cpp` | 4.199 | Editor de gráficos grande |
| `divmap3d.cpp` | 3.606 | Editor 3D — aplazable |
| `div.cpp` | 3.376 | Shell principal del IDE |
| `divhandl.cpp` | 3.023 | Manejadores de menús/ventanas |
| `divedit.cpp` | 2.688 | Editor de código `.prg` |
| `divbrow.cpp` | 1.745 | Navegador de ficheros |
| `divpcm.cpp` | 1.734 | Editor de sonido |
| `divfpg.cpp` | 1.671 | Editor FPG |
| `divfont.cpp` | 1.640 | Editor de fuentes |
| `divpalet.cpp` | 1.527 | Editor de paleta |
| `divforma.cpp` | 1.333 | Carga PCX/BMP/JPG/MAP |
| `divhelp.cpp` | 1.171 | Ayuda hipertexto |
| `divspr.cpp` | 1.058 | Generador de sprites |
| `divinsta.cpp` | 1.054 | Creador de instaladores — salteable |
| `divdsktp.cpp` | 936 | Escritorio |
| `divbasic.cpp` | 901 | Primitivas gráficas software |
| `divsetup.cpp` / `ifs.cpp` | ~1.700 | Setup / IFS |
| Puentes hardware (`divvideo`, `divmouse`, `divkeybo`, `divtimer`, `divsb`, `divsound`, `divmixer`...) | ~1.500 | Muy pequeños y concentrados |

**Núcleo IDE útil** (shell + ventanas + editor `.prg` + botón compilar/probar): **~13.000 líneas**.  
**IDE completo menos 3D/instalador/CD**: **~35.000-37.000 líneas**.

## 2. Superficie DOS/hardware: mejor de lo esperado

Conteo de llamadas DOS/BIOS/hardware en todo el IDE:

| Fichero | Sitios |
|---|---|
| `divmixer.cpp` + `divsb.cpp` | 59 |
| `div.cpp` + `divhandl.cpp` | 26 |
| `divvideo.cpp` | 13 |
| `divkeybo.cpp` | 12 |
| `divtimer.cpp` | 9 |
| `divbrow.cpp` | 8 |
| Resto dispersos | ~15 |
| **Total** | **~150** |

- **~60% es audio** (`divmixer`/`divsb` + 97 referencias a `judas_*`). Reemplazable por la capa `port/io` de audio ya usada en el runtime.
- **SciTech/SVGA**: solo 10 referencias. El IDE pinta en un framebuffer indexado a 256 colores propio (`copia`) y luego hace blit a VESA — exactamente el patrón que ya funciona en `v.cpp` del runtime.
- **Mouse/teclado/timer**: 3 ficheros pequeños (~400 líneas) → mapear a `port/io`.

## 3. Estimación por fases

| Fase | Contenido | Esfuerzo estimado |
|---|---|---|
| **E1** | Compila+enlaza el núcleo IDE (shell, ventanas, editor `.prg`) con MSVC usando `/TC`, shims y forward-decls | 1-2 sesiones |
| **E2** | Vídeo+input: escritorio visible, ratón y teclado vivos (blit del framebuffer a raylib) | 1-2 sesiones |
| **E3** | **Editor `.prg` funcional + botón compilar (usa `compilar()` ya portado) + probar en `div32run_port`** | 1-2 sesiones |
| **E4** | Editores de recursos (fpg, paleta, paint, font, sonido, browser, ayuda) | 2-4 sesiones |
| E5 | `divmap3d`, instalador, CD | Aplazable/salteable |

**IDE útil (E1-E3): ~3-5 sesiones de trabajo**.  
**IDE completo menos 3D/instalador: ~2-3× ese esfuerzo**.

## 4. Riesgos

1. **Bugs x64**: el IDE es código más antiguo que `divc.cpp`; se esperan más errores del tipo "puntero en `int`" y K&R. Se cazan con el mismo método ya probado.
2. **Testing interactivo**: la UI no se puede verificar solo con scripts; requerirá clicar manualmente para confirmar hitos.
3. **Efectos de paleta**: ghost, fundidos, gamma. El runtime ya usa blit indexado a 256 colores, pero habrá que asegurar que la gestión de DAC del IDE funcione igual.

## 5. Conclusión

Es más trabajo que el compilador (aproximadamente 2-3×), pero el terreno no es tan hostil como sugieren las 52.000 líneas: las piezas difíciles (vídeo, audio, input, timer) ya están resueltas en el runtime y son reutilizables casi directamente. El camino más valioso es **E1→E3**: tener un editor que escribe, compila y prueba juegos. Los editores de recursos vienen después.
