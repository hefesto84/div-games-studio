#ifndef _SHIM_GRAPH_H
#define _SHIM_GRAPH_H

/* Watcom Graphics Library (<graph.h>). Not used by the ported core:
   v.cpp (mode switching) is replaced by the raylib backend. */

#define _MRES256COLOR 4
#define _MRES16COLOR  3
#define _MAXRESMODE   2
#define _TEXTC80      2
#define _VRES256COLOR 19
#define _DEFAULTMODE  0

#ifdef __cplusplus
extern "C" {
#endif

int _setvideomode(int mode);

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_GRAPH_H */