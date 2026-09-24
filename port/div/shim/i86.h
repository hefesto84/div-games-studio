#ifndef _SHIM_I86_H
#define _SHIM_I86_H

#include <stddef.h>

/* Watcom <i86.h> subset for the DIV32RUN port on MSVC x64.
   Memory-model keywords are meaningless on x64 -> expand to nothing. */

#ifndef far
#define far
#endif
#ifndef _far
#define _far
#endif
#ifndef __far
#define __far
#endif
#ifndef near
#define near
#endif
#ifndef _near
#define _near
#endif
#ifndef __near
#define __near
#endif
#ifndef far16
#define far16
#endif
#ifndef _huge
#define _huge
#endif
#ifndef __huge
#define __huge
#endif
#ifndef _based
#define _based
#endif
#ifndef __based
#define __based
#endif
#ifndef _loadds
#define _loadds
#endif
#ifndef __loadds
#define __loadds
#endif
#ifndef _export
#define _export
#endif
#ifndef __export
#define __export
#endif
#ifndef __exported
#define __exported
#endif
#ifndef __pascal
#define __pascal
#endif
#ifndef __syscall
#define __syscall
#endif
#ifndef __stdcall
#define __stdcall __cdecl
#endif

#ifndef __interrupt
#define __interrupt
#endif
#ifndef _interrupt
#define _interrupt
#endif
#ifndef __intrinsic
#define __intrinsic
#endif

/* Real-mode helpers; not meaningful on x64 but kept as arithmetic macros. */
#ifndef MK_FP
#define MK_FP(seg, off) ((void *)(((unsigned long)(seg) << 4) + (unsigned long)(off)))
#endif
#ifndef FP_SEG
#define FP_SEG(ptr) ((unsigned)(((size_t)(const void *)(ptr)) >> 16))
#endif
#ifndef FP_OFF
#define FP_OFF(ptr) ((unsigned)((size_t)(const void *)(ptr)) & 0xFFFFu)
#endif

#ifndef MK_SEG
#define MK_SEG(sel) ((unsigned long)(sel))
#endif
#ifndef _MK_FP
#define _MK_FP(seg, off) MK_FP(seg, off)
#endif

#ifndef disable
#define disable() (void)0
#endif
#ifndef enable
#define enable() (void)0
#endif
#ifndef _disable
#define _disable() (void)0
#endif
#ifndef _enable
#define _enable() (void)0
#endif
#ifndef nodisable
#define nodisable() (void)0
#endif

/* Port I/O stubs. DIV only ever reads hardware timers/joystick ports here;
   los nombres inp/outp/inpw/outpw/_inp/_outp son intrinsecos reservados
   del compilador de MSVC en modo C (error C2169 si se redefinen
   directamente) -- se implementan con otro nombre y se redirigen por macro,
   asi el codigo del nucleo (que SI los llama con esos nombres) no necesita
   tocarse.

   OJO con el valor de "hardware ausente": en el puerto de joystick (0x201)
   los bits flotan en ALTO cuando no hay nada conectado. Devolver 0 era un
   bug: read_joy() (f.cpp:2472) interpreta bit 4 en 0 como boton 1 PULSADO
   (joy.button1=1 para siempre -> MALVADO saltaba sin parar), y el detector
   de timeout de joy_position() ("espera a que el bit baje") salia al
   instante, asi que el joystick fantasma nunca se desactivaba. Con 0xFF:
   botones en 0 y los ejes agotan TIME_OUT -> joy_status se pone a 0 solo
   (i.cpp:813-818). El resto de puertos (PIT del timer) siguen a 0: sus
   lecturas solo se usan si hay joystick, y nunca lo hay. */
static __inline unsigned div_port_inp(unsigned _p) { return (_p == 0x201) ? 0xFF : 0; }
static __inline void div_port_outp(unsigned _p, unsigned _v) { (void)_p; (void)_v; }

#define inp(p)      div_port_inp(p)
#define inpw(p)     div_port_inp(p)
#define _inp(p)     div_port_inp(p)
#define outp(p, v)  div_port_outp((p), (v))
#define outpw(p, v) div_port_outp((p), (v))
#define _outp(p, v) div_port_outp((p), (v))

#endif /* _SHIM_I86_H */