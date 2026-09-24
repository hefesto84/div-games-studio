# Estado del árbol (observado en esta revisión)

> Esta sección es una fotografía puntual del estado del repositorio y del entorno de build local. A diferencia del resto de `docs/architecture/`, **no** es documentación estable del proyecto: puede quedar desactualizada rápido. Verificar siempre con `git status`/`git branch -a` antes de confiar en ella.

## Estado de git (verificado 2026-09-17)

- Rama actual: `master`, sin cambios pendientes (`git status --short` no devuelve nada — árbol de trabajo limpio).
- Ramas remotas disponibles: `origin/2.01`, `origin/divc`, `origin/master`, `origin/travis-test`.
- **Nota histórica**: una revisión anterior de `ARCHITECTURE.md` registraba 6 archivos modificados sin commitear en `src/vpe/*.inc` (`oloop`, `omloop`, `otloop`, `wloop`, `wmloop`, `wtloop`), con la advertencia de "no tocar a ciegas" por tratarse posiblemente de diferencias de fin de línea (CRLF/LF) o contenido generado. Esos cambios ya no están presentes en el árbol de trabajo actual (o fueron descartados o commiteados en algún punto). Si reaparecen, revisar el diff real antes de decidir si son ruido o cambios intencionales.

## Estado del toolchain de build local

- **OpenWatcom no está instalado** en este equipo: la variable de entorno `WATCOM` no está definida y `wmake`, `wcc386` no se encuentran en el `PATH`.
- Esto significa que, en el estado actual, **no es posible compilar el proyecto** en esta máquina sin antes instalar OpenWatcom 1.9 (ver [04-build-system.md](04-build-system.md) y [10-hoja-de-ruta.md](10-hoja-de-ruta.md)).
- Las librerías de terceros ya vienen precompiladas para 386 y 586 en `3rdparty/lib/{386,586}/`, por lo que **no hace falta Turbo Assembler (TASM)** para un build normal — solo sería necesario si se quisieran recompilar esas librerías desde cero.

## Cómo revalidar esta sección

```bash
git status --short
git branch -a
# En bash (Git Bash / WSL):
echo "WATCOM=$WATCOM"; command -v wmake; command -v wcc386
```

## Ver también

- [04-build-system.md](04-build-system.md) — qué se necesita instalar para poder compilar.
- [10-hoja-de-ruta.md](10-hoja-de-ruta.md) — próximos pasos sugeridos dado este estado.
