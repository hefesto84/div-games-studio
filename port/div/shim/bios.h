#ifndef PORT_DIV_SHIM_CORE_BIOS_H
#define PORT_DIV_SHIM_CORE_BIOS_H
/*
 *  Shim de <bios.h> (Watcom/DOS) para el port del nucleo DIV.
 *
 *  Unico simbolo realmente usado por i.cpp/f.cpp/kernel.cpp (confirmado
 *  por grep, ver docs/architecture/12-port-progreso.md): _bios_timeofday,
 *  llamado como _bios_timeofday(_TIME_GETCLOCK, &ticks) para sembrar el
 *  generador de numeros aleatorios (i.cpp) con el reloj del sistema.
 *
 *  En el original devuelve el numero de ticks del PIT (18.2 Hz) desde
 *  medianoche. Aqui se implementa en port_misc.cpp sobre el reloj
 *  monotono de port/io (io_timer), escalado a la misma cadencia, para
 *  que el valor siga siendo utilizable como semilla.
 *
 *  Nota: las declaraciones especulativas de teclado via BIOS
 *  (bim_key_ready/bim_keybrd/bim_key_shift) que estaban aqui antes se
 *  retiraron por no estar usadas por ningun fichero portado todavia
 *  (divkeybo.cpp no forma parte de este commit) y tener sintaxis rota.
 *  Cuando se porte el input del teclado, se resolvera directamente
 *  contra port/io (io_key_down/io_key_pressed), no via un BIOS falso.
 */
#ifdef __cplusplus
extern "C" {
#endif

#define _TIME_GETCLOCK 0
#define _TIME_SETCLOCK 1

extern int _bios_timeofday(int cmd, long *timerticks);

#ifdef __cplusplus
}
#endif

#endif /* PORT_DIV_SHIM_CORE_BIOS_H */
