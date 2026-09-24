# Hoja de ruta

## Roadmap declarado del proyecto (README / milestones de GitHub)

1. **[Versión 2.01](https://github.com/vii1/DIV/milestone/1)** — Reproducir lo más fielmente posible el DIV 2 comercial tal como salió a la venta en 1999. Este es el hito fundacional: antes de mejorar nada, restaurar el comportamiento original bit a bit.
2. **[Versión 2.02](https://github.com/vii1/DIV/milestone/2)** — Arreglar bugs conocidos (algunos arrastrados desde hace décadas en el binario comercial) y pulir detalles de usabilidad para hacer DIV más cómodo de usar hoy, sin cambiar su identidad.
3. **Después** — Introducir [mejoras](https://github.com/vii1/DIV/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement) (features nuevas), seguir reorganizando y documentando el código.

Este orden de prioridades (fidelidad → estabilidad → mejora) es importante para interpretar cualquier PR o discusión de diseño en el proyecto: cambios que alteren el comportamiento observable de DIV 2.01 sin justificación de bug conocido probablemente no encajan en la fase actual.

## Próximos pasos técnicos sugeridos (dado el estado observado del entorno, ver [09-estado-arbol.md](09-estado-arbol.md))

1. **Instalar OpenWatcom 1.9** (targets DOS 16-bit + DOS 32-bit + host) y verificar que `wmake`, `wcc`, `wcc386` queden en el `PATH` (`OWSETENV.BAT` en DOS/Windows, `owsetenv.sh` en Linux). Alternativa: usar el `Vagrantfile` del repo para un entorno reproducible ya configurado.
2. Probar primero `wmake tools` (compila las herramientas de host: `bin2h`, `testdll`, `unpak`) antes de intentar el build completo — es un subconjunto más rápido de validar y no depende de los compiladores cruzados DOS.
3. Ejecutar `wmake` completo y documentar cualquier error específico de OpenWatcom 1.9 sobre un Windows moderno (rutas largas, permisos, diferencias de `wlink`, etc.), ya que el propio README advierte de incompatibilidades conocidas con OpenWatcom 2.
4. Ejecutar los tests disponibles: `wmake test_dll` (no requiere DOS) y, si hay DOSBox/DOSBox-X instalado, los tests del intérprete bajo emulación.
5. Con el build funcionando, decidir el objetivo de trabajo concreto:
   - **Camino conservador**: compilar el binario DOS y ejecutarlo bajo DOSBox-X, enfocado en paridad con el DIV 2.01 original (alineado con el milestone 2.01).
   - **Camino ambicioso** (fuera del alcance declarado de este fork, pero mencionado como posibilidad futura): un port moderno sustituyendo la capa de bajo nivel (vídeo/input/audio/red descrita en [08-aspectos-tecnicos.md](08-aspectos-tecnicos.md)) por una abstracción portable (p. ej. SDL2/3). Viable en principio porque el intérprete (la VM de `kernel.cpp` + funciones de runtime) es, en teoría, separable de la capa de E/S — a confirmar con el detalle de [07-modelo-ejecucion.md](07-modelo-ejecucion.md) y [08-aspectos-tecnicos.md](08-aspectos-tecnicos.md).

## Ver también

- [09-estado-arbol.md](09-estado-arbol.md) — estado real del entorno local a la fecha.
- [04-build-system.md](04-build-system.md) — detalle del proceso de build a seguir.
- [08-aspectos-tecnicos.md](08-aspectos-tecnicos.md) — desafíos técnicos que condicionan cualquier port.
