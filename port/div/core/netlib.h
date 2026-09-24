#ifndef _SHIM_NETLIB_H
#define _SHIM_NETLIB_H

/* Stand-in for the original src/netlib/netlib.h: networking is out of
   scope for the port, so every entry point is a safe stub. */

/* WORD/BYTE: tipos clasicos de 16/8 bits que i.cpp usa en las firmas de
 * MAINSRV_Packet/MAINNOD_Packet (funciones de red, nunca implementadas ni
 * llamadas en este commit -- ver docs/architecture/12-port-progreso.md).
 * En DOS/Watcom los aporta <dos.h>; en Windows normalmente <windows.h>,
 * que no incluimos aqui. Guardados con #ifndef por si en el futuro se
 * llega a incluir windows.h en la misma unidad de compilacion. */
#ifndef _WORD_DEFINED_PORT
#define _WORD_DEFINED_PORT
typedef unsigned short WORD;
typedef unsigned char BYTE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int inicializacion_red;
extern int net_status;

void net_init(void);
void net_end(void);
void _net_loop(void);

int  net_join_game(void);
int  net_get_games(void);

void NET_Start(int id);
void NET_End(void);
int  NET_Recv(int *datos, int tamanyo);
int  NET_Send(int *datos, int tamanyo);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_NETLIB_H */