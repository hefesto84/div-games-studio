/*
 * ia.c -- modulo de IA / busqueda de caminos del runtime (path_find,
 * path_line, path_free).
 *
 * PORT: estas tres funciones eran stubs vacios en port_stubs.c (ver
 * docs/architecture/12-port-progreso.md) y TOKENKAI (como casi todo juego
 * de scroll) las necesita para mover al protagonista con el raton. Esta
 * implementacion es un PORT FIEL del modulo original del runtime DIV 2
 * (src/shared/run/ia.c del repo MikeDX/DIV-Games-Studio, el mismo del que
 * se restauraron i.cpp/f.cpp/kernel.cpp), con tres unicas diferencias,
 * todas documentadas aqui mismo:
 *
 *   1. Todas las variables de estado del modulo se declaran `static`: en
 *      el original eran globales con linkage externo compartidas con otros
 *      TUs, aqui nadie mas las referencia y asi no chocan con los globals
 *      del resto del port.
 *   2. La rama `#ifdef DIV2` de path_find (comprobacion de limites del
 *      buffer destino con capar()) se usa SIEMPRE: es la unica que encaja
 *      con este port (f.cpp ya usa capar() en todos los builtins).
 *   3. find_status se deja con linkage externo a proposito: i.cpp lo
 *      declara `extern int find_status;` y lo resetea en su init.
 *
 * Semantica (manual DIV 2):
 *   path_find(modo, fichero, grafico, tamanioCasilla, x, y, OFFSET, SIZEOF)
 *     Busca una ruta desde las coordenadas actuales del proceso hasta
 *     (x,y) sorteando los obstaculos del "mapa de busqueda" (el grafico
 *     indicado, donde negro=paleta 0 es zona libre y blanco es obstaculo).
 *     Devuelve el numero de puntos de la ruta (0 si no hay ruta) y deja
 *     los puntos en la estructura pasada (struct {x,y}).
 *     modo 0 = busqueda rapida (A* con heuristica euclidea a la mita),
 *     modo 1 = busqueda exacta (Dijkstra puro sobre la lista ordenada).
 *   path_line(file, code, tile, x, y)
 *     1 si se puede ir en linea recta desde el proceso hasta (x,y).
 *   path_free(file, code, tile, x, y)
 *     1 si la casilla (x,y) del mapa de busqueda esta libre.
 */

#include "inter.h"

/* ==========================================================================
 *  Datos del modulo
 * ========================================================================== */

#define max_map_size 254 // 128

int find_status;    // Indica si han sido inicializadas las estructuras

static int modo;    // 0 Rapido, 1 Exacto

static word * distancias;   // Tabla con las distancias del origen a cada casilla
static word * distancias2;  // Tabla con las distancias del destino a cada casilla
static word * siguientes;   // Tabla con la lista enlazada de casillas

#define dis(x)  (*(distancias+(x)))
#define dis2(x) (*(distancias2+(x)))
#define sig(x)  (*(siguientes+(x)))

static byte *map;   // Puntero al mapa de durezas (bytes, 0 si libre)
static int an,al;   // Ancho y alto del mapa

#define m(x,y) (*(map+(x)+((y)*an)))

static word cx,cy,c;    // Casilla actualmente en exploracion (x,y y numero)
static int ax,ay,bx,by; // Casillas inicial y final
static word b;          // Casilla final (numero)
static int fin;         // Indica si se alcanzo ya el destino
static int tile;        // Tilesize del mapa (entero, de 1 en adelante)
static int choque_linea;    // Para determinar si se puede ir de un punto a otro

static int cas_inicial; // Si la primera casilla esta ocupada .. la desocupa

/* ==========================================================================
 *  path_find(modo,file,code,tilesize,x,y,offset tabla,sizeof tabla)
 * ========================================================================== */

static int init_find(void);
static int calcula_vertices(int * ptr, int max_ver, int x0, int y0, int x1, int y1);
static void puede_ir(int x0,int y0,int x1,int y1);
static void expand(void);
static void expand2(void);
static void add(int x, int y, word bnew, word step);
static void add2(word bnew, word step);

void path_find(void) {
  int file,code,x,y,offset,size;  // Parametros de entrada
  int *ptr;                       // Puntero al registro del mapa

  size=pila[sp--];
  offset=pila[sp--];
  y=pila[sp--];
  x=pila[sp--];
  tile=pila[sp--];
  code=pila[sp--];
  file=pila[sp--];
  modo=pila[sp];

  pila[sp]=0; // Por defecto, y hasta que se demuestre lo contrario

  // Comprueba limites de offset y size ...

  if (!capar(offset) || !capar(offset+size)) { e(122); return; }

  // Limites tilesize

  if (tile<1 || tile>256) { e(151); return; }

  // Comprueba limites de file y code

  if (file>max_fpgs || file<0) { e(109); return; }
  if (file) max_grf=1000; else max_grf=2000;
  if (code<=0 || code>=max_grf) { e(110); return; }
  if (g[file].grf==NULL) { e(111); return; }
  if ((ptr=g[file].grf[code])==NULL) { e(121); return; }

  // Toma puntero al mapa, ancho y alto

  an=ptr[13]; al=ptr[14]; map=(byte*)ptr+64+ptr[15]*4;

  if (an<1 || al<1 || an>max_map_size || al>max_map_size) { e(152); return; }

  // Comprueba limites de coordenadas (si estan fuera del mapa retorna 0)

  if (x<0 || y<0 || x>=an*tile || y>=al*tile) return;
  ax=mem[id+_X]; ay=mem[id+_Y];
  if (ax<0 || ay<0 || ax>=an*tile || ay>=al*tile) return;

  // Calcula las casillas inicial y final: m(ax,ay) y m(bx,by)

  ax/=tile; ay/=tile; bx=x/tile; by=y/tile;

  if (m(bx,by)) { // Caso en el que la casilla final esta ocupada

    x=0; // Si la casilla final no esta libre, prueba con una adyacente

    if (ax<bx) {
      if (ay<by) {
        if (bx) if (!m(bx-1,by)) x=1;
        if (!x && by) if (!m(bx,by-1)) x=2;
        if (!x && bx<an-1) if (!m(bx+1,by)) x=3;
        if (!x && by<al-1) if (!m(bx,by+1)) x=4;
      } else {
        if (bx) if (!m(bx-1,by)) x=1;
        if (!x && by<al-1) if (!m(bx,by+1)) x=4;
        if (!x && by) if (!m(bx,by-1)) x=2;
        if (!x && bx<an-1) if (!m(bx+1,by)) x=3;
      }
    } else {
      if (ay<by) {
        if (bx<an-1) if (!m(bx+1,by)) x=3;
        if (!x && by) if (!m(bx,by-1)) x=2;
        if (!x && by<al-1) if (!m(bx,by+1)) x=4;
        if (!x && bx) if (!m(bx-1,by)) x=1;
      } else {
        if (bx<an-1) if (!m(bx+1,by)) x=3;
        if (!x && by<al-1) if (!m(bx,by+1)) x=4;
        if (!x && bx) if (!m(bx-1,by)) x=1;
        if (!x && by) if (!m(bx,by-1)) x=2;
      }
    }

    if (!x) return;

    switch(x) {
      case 1: bx--; x=bx*tile+tile-1; y=by*tile+tile/2; break;
      case 2: by--; x=bx*tile+tile/2; y=by*tile+tile-1; break;
      case 3: bx++; x=bx*tile;        y=by*tile+tile/2; break;
      case 4: by++; x=bx*tile+tile/2; y=by*tile;        break;
    }
  }

  // Si las casillas inicial y final son identicas ...

  if (ax==bx && ay==by) {
    if (size<2) return;
    mem[offset]=x; mem[offset+1]=y; // Va directamente hasta (x,y)
    pila[sp]=1; return;
  }

  // Inicializa el sistema de busqueda, si esta es la primera busqueda

  if (!find_status) if (init_find()) { e(100); return; }

  cas_inicial=m(ax,ay); m(ax,ay)=0;

  // Prepara (limpia) los buffer ...

  memset(distancias,0,max_map_size*max_map_size*2); // Distancias si hace falta limpiarlo, para ver
                          // que casillas estan ya visitadas en el "fill"

  cx=ax; cy=ay; c=cx+cy*max_map_size; b=bx+by*max_map_size;

  sig(c)=65535; // No hay una siguiente
  dis(c)=1;     // No hay una distancia (1 para marcarla como visitada)
  fin=0;        // No se encontro un camino

  if (!modo) do {
    expand();
    c=sig(c);
    if (c==65535) break;
    cy=c/max_map_size; cx=c%max_map_size; // Hace falta por comprobar los limites (y el mapa)
  } while (!fin);
  else do {
    expand2();
    c=sig(c);
    if (c==65535) break;
    cy=c/max_map_size; cx=c%max_map_size; // Hace falta por comprobar los limites (y el mapa)
  } while (!fin);

  // Si (fin==1) se alcanzo la casilla (bx,by), y el camino se saca de dis()

  if (!fin) { //  No se encontro (no hay) un camino hasta bx,by
    m(ax,ay)=cas_inicial; return;
  }

  // Hay un camino, ahora obtiene los vertices en la tabla pasada como parametro

  pila[sp]=calcula_vertices(&mem[offset],size/2,mem[id+_X],mem[id+_Y],x,y);

  m(ax,ay)=cas_inicial;

}

/* ==========================================================================
 *  Expande una casilla
 * ========================================================================== */

static void expand(void) { // Version rapida
  int n=0;

  // Examina los limites del mapa y anade los cuatro laterales

  if (cx)      if (!m(cx-1,cy)) { n|=1; if (!dis(c-1))   add(cx-1,cy,c-1,10); }
  if (cx<an-1) if (!m(cx+1,cy)) { n|=2; if (!dis(c+1))   add(cx+1,cy,c+1,10); }
  if (cy)      if (!m(cx,cy-1)) { n|=4; if (!dis(c-max_map_size)) add(cx,cy-1,c-max_map_size,10); }
  if (cy<al-1) if (!m(cx,cy+1)) { n|=8; if (!dis(c+max_map_size)) add(cx,cy+1,c+max_map_size,10); }

  // Ahora anade las cuatro diagonales (no se si sera necesario ...)

  if ((n&5)==5)   if (!dis(c-1-max_map_size)) if (!m(cx-1,cy-1)) add(cx-1,cy-1,c-1-max_map_size,14);
  if ((n&9)==9)   if (!dis(c-1+max_map_size)) if (!m(cx-1,cy+1)) add(cx-1,cy+1,c-1+max_map_size,14);
  if ((n&6)==6)   if (!dis(c+1-max_map_size)) if (!m(cx+1,cy-1)) add(cx+1,cy-1,c+1-max_map_size,14);
  if ((n&10)==10) if (!dis(c+1+max_map_size)) if (!m(cx+1,cy+1)) add(cx+1,cy+1,c+1+max_map_size,14);

}

static void expand2(void) { // Version exacta
  int n=0;

  // Examina los limites del mapa y anade los cuatro laterales

  if (cx)      if (!m(cx-1,cy)) { n|=1; if (!dis(c-1))   add2(c-1,10); }
  if (cx<an-1) if (!m(cx+1,cy)) { n|=2; if (!dis(c+1))   add2(c+1,10); }
  if (cy)      if (!m(cx,cy-1)) { n|=4; if (!dis(c-max_map_size)) add2(c-max_map_size,10); }
  if (cy<al-1) if (!m(cx,cy+1)) { n|=8; if (!dis(c+max_map_size)) add2(c+max_map_size,10); }

  // Ahora anade las cuatro diagonales (no se si sera necesario ...)

  if ((n&5)==5)   if (!dis(c-1-max_map_size)) if (!m(cx-1,cy-1)) add2(c-1-max_map_size,14);
  if ((n&9)==9)   if (!dis(c-1+max_map_size)) if (!m(cx-1,cy+1)) add2(c-1+max_map_size,14);
  if ((n&6)==6)   if (!dis(c+1-max_map_size)) if (!m(cx+1,cy-1)) add2(c+1-max_map_size,14);
  if ((n&10)==10) if (!dis(c+1+max_map_size)) if (!m(cx+1,cy+1)) add2(c+1+max_map_size,14);

}

/* ==========================================================================
 *  Anade una nueva casilla
 * ========================================================================== */

static void add(int x, int y, word bnew, word step) { // Version rapida
  word cdis;  // Distancia
  word i,a;   // Siguiente y anterior

  dis(bnew)=dis(c)+step; // Guarda su distancia

  i=abs(x-bx); a=abs(y-by); cdis=dis2(bnew)=(i<a)?(i>>2)+a:i+(a>>2);

  a=c; do { // Busca el lugar para esta nueva casilla
    i=a; a=sig(a);
    if (a==65535) break;
  } while (cdis>dis2(a));

  sig(i)=bnew;
  sig(bnew)=a;

  if (bnew==b) fin=1;
}

static void add2(word bnew, word step) { // Version exacta
  word i,a;   // Siguiente y anterior

  dis(bnew)=dis(c)+step; // Guarda su distancia

  a=c; do { // Busca el lugar para esta nueva casilla
    i=a; a=sig(a);
    if (a==65535) break;
  } while (dis(bnew)>dis(a));

  sig(i)=bnew;
  sig(bnew)=a;

  if (bnew==b) fin=1;
}

/* ==========================================================================
 *  Inicializa las estructuras de busqueda - Retorna 1 si fallo algun alloc
 * ========================================================================== */

static int init_find(void) {

  find_status=1;

  distancias=(word*)malloc(max_map_size*max_map_size*2);
  if (distancias==NULL) return(1);
  distancias2=(word*)malloc(max_map_size*max_map_size*2);
  if (distancias2==NULL) { free(distancias); return(1); }
  siguientes=(word*)malloc(max_map_size*max_map_size*2);
  if (siguientes==NULL) { free(distancias2); free(distancias); return(1); }

  return(0);
}

/* ==========================================================================
 *  Calcula todos los vertices por los que debe pasar, de (x1,y1) a (x0,y0)
 *  Devuelve el numero de vertices o 0 si salen demasiados vertices ...
 * ========================================================================== */

static int calcula_vertices(int * ptr, int max_ver, int x0, int y0, int x1, int y1) {
  int * p=ptr+max_ver*2;  // Del ultimo al primero, y luego memmove
  int num=max_ver;        // Para contar los vertices que lleva metidos en *ptr
  int d;                  // Distancia de la casilla actual al inicio
  int x,y;                // Siguiente punto
  int xx,yy;              // Punto actual (hasta el que SI puede ir seguro)
  int newdir;             // Temporal (para calcular la siguiente casilla)
  int dir;                // Direcciones en las que puede ir ...
  int cas;                // Casilla actual (bx+by*max_map_size)
  int n;                  // Un simple contador
  int nextx[8],nexty[8];  // Las proximas ocho casillas (ix,iy desde bx,by)

  if (num<1) return(0);
  *(--p)=y1; *(--p)=x1; num--; // Mete el ultimo vertice

  x=bx*tile+tile/2; y=by*tile+tile/2; cas=bx+by*max_map_size; fin=0;

  do {

    do {

      d=dis(cas); // Obtiene la siguiente (bx,by) y (x,y)

      if (d>14*8) {

        xx=bx; yy=by;

        for (n=0;n<8;n++) { dir=0;
          if (xx)      if (dis(cas-1)  ) { dir|=1; if(dis(cas-1)<=d  ) { d=dis(cas-1);   newdir=1; } }
          if (xx<an-1) if (dis(cas+1)  ) { dir|=2; if(dis(cas+1)<=d  ) { d=dis(cas+1);   newdir=2; } }
          if (yy)      if (dis(cas-max_map_size)) { dir|=4; if(dis(cas-max_map_size)<=d) { d=dis(cas-max_map_size); newdir=3; } }
          if (yy<al-1) if (dis(cas+max_map_size)) { dir|=8; if(dis(cas+max_map_size)<=d) { d=dis(cas+max_map_size); newdir=4; } }

          if ((dir&5)==5)   if(dis(cas-1-max_map_size)) if(dis(cas-1-max_map_size)<=d) { d=dis(cas-1-max_map_size); newdir=5; }
          if ((dir&9)==9)   if(dis(cas-1+max_map_size)) if(dis(cas-1+max_map_size)<=d) { d=dis(cas-1+max_map_size); newdir=6; }
          if ((dir&6)==6)   if(dis(cas+1-max_map_size)) if(dis(cas+1-max_map_size)<=d) { d=dis(cas+1-max_map_size); newdir=7; }
          if ((dir&10)==10) if(dis(cas+1+max_map_size)) if(dis(cas+1+max_map_size)<=d) { d=dis(cas+1+max_map_size); newdir=8; }

          switch(newdir) {
            case 1: xx--; cas+=-1;           break;
            case 2: xx++; cas+=1;            break;
            case 3: yy--; cas+=-max_map_size;         break;
            case 4: yy++; cas+=max_map_size;          break;
            case 5: xx--; yy--; cas+=-1-max_map_size; break;
            case 6: xx--; yy++; cas+=-1+max_map_size; break;
            case 7: xx++; yy--; cas+=1-max_map_size;  break;
            case 8: xx++; yy++; cas+=1+max_map_size;  break;
          }

          nextx[n]=xx-bx; nexty[n]=yy-by;
        }

        puede_ir(x1,y1,x+nextx[7]*tile,y+nexty[7]*tile);
        if (!choque_linea) {
          bx+=nextx[7]; by+=nexty[7];
        } else {
          puede_ir(x1,y1,x+nextx[3]*tile,y+nexty[3]*tile);
          if (!choque_linea) {
            bx+=nextx[3]; by+=nexty[3];
          } else {
            puede_ir(x1,y1,x+nextx[1]*tile,y+nexty[1]*tile);
            if (!choque_linea) {
              bx+=nextx[1]; by+=nexty[1];
            } else {
              puede_ir(x1,y1,x+nextx[0]*tile,y+nexty[0]*tile);
              bx+=nextx[0]; by+=nexty[0];
            }
          }
        }

        xx=x; yy=y; // desde x1,y1 hasta xx,yy "SI" puede ir SIEMPRE
        x=bx*tile+tile/2; y=by*tile+tile/2; cas=bx+by*max_map_size;

      } else {

        xx=x; yy=y; dir=0; // desde x1,y1 hasta xx,yy "SI" puede ir SIEMPRE

        if (bx)      if (dis(cas-1)  ) { dir|=1; if(dis(cas-1)<=d  ) { d=dis(cas-1);   newdir=1; } }
        if (bx<an-1) if (dis(cas+1)  ) { dir|=2; if(dis(cas+1)<=d  ) { d=dis(cas+1);   newdir=2; } }
        if (by)      if (dis(cas-max_map_size)) { dir|=4; if(dis(cas-max_map_size)<=d) { d=dis(cas-max_map_size); newdir=3; } }
        if (by<al-1) if (dis(cas+max_map_size)) { dir|=8; if(dis(cas+max_map_size)<=d) { d=dis(cas+max_map_size); newdir=4; } }

        if ((dir&5)==5)   if(dis(cas-1-max_map_size)) if(dis(cas-1-max_map_size)<=d) { d=dis(cas-1-max_map_size); newdir=5; }
        if ((dir&9)==9)   if(dis(cas-1+max_map_size)) if(dis(cas-1+max_map_size)<=d) { d=dis(cas-1+max_map_size); newdir=6; }
        if ((dir&6)==6)   if(dis(cas+1-max_map_size)) if(dis(cas+1-max_map_size)<=d) { d=dis(cas+1-max_map_size); newdir=7; }
        if ((dir&10)==10) if(dis(cas+1+max_map_size)) if(dis(cas+1+max_map_size)<=d) { d=dis(cas+1+max_map_size); newdir=8; }

        switch(newdir) {
          case 1: bx--; cas+=-1; x-=tile; break;
          case 2: bx++; cas+=1; x+=tile; break;
          case 3: by--; cas+=-max_map_size; y-=tile; break;
          case 4: by++; cas+=max_map_size; y+=tile; break;
          case 5: bx--; by--; cas+=-1-max_map_size; x-=tile; y-=tile; break;
          case 6: bx--; by++; cas+=-1+max_map_size; x-=tile; y+=tile; break;
          case 7: bx++; by--; cas+=1-max_map_size; x+=tile; y-=tile; break;
          case 8: bx++; by++; cas+=1+max_map_size; x+=tile; y+=tile; break;
        }

        puede_ir(x1,y1,x,y); // devuelve choque_linea=0/1

      }

      if (bx==ax && by==ay) { fin=1; break; }

    } while (!choque_linea);

    if (choque_linea) { // Anade un nuevo vertice a la ruta
      if (!num) return(0);
      *(--p)=yy; *(--p)=xx; num--;
      x1=xx; y1=yy;
    }

  } while (!fin);

  if (x!=x0 || y!=y0) { // es necesario ir al centro de la primera casilla?
    puede_ir(x1,y1,x0,y0);
    if (choque_linea) {
      if (!num) return(0);
      *(--p)=y; *(--p)=x; num--; // Un caso poco probable, pero bueno ...
    }
  }

  if (num) {
    memmove((byte*)ptr,(byte*)p,(max_ver-num)*8);
  } return(max_ver-num);
}

/* ==========================================================================
 *  Determina si puede ir a un punto en linea recta (pathline/calcula_vertices)
 * ========================================================================== */

static void puede_ir(int x0,int y0,int x1,int y1) {
  int tilesize;
  int dx,dy,a,b,d,x,y;

  choque_linea=0;
/*
  if (tile>3) { // Tamano del tile ... si es muy grande lo divide entre 2
    x0=x0/2; y0=y0/2;
    x1=x1/2; y1=y1/2;
    tilesize=tile/2;
  } else tilesize=tile;
*/  tilesize=tile;

  if (x0>x1) { x=x1; dx=x0-x1; } else { x=x0; dx=x1-x0; }
  if (y0>y1) { y=y1; dy=y0-y1; } else { y=y0; dy=y1-y0; }

  if (dx || dy) {
    if (dy<=dx) {
      if (x0>x1) {
        if (m(x1/tilesize,y1/tilesize)) choque_linea=1;
        x0--; swap(x0,x1); swap(y0,y1);
      }
      d=2*dy-dx; a=2*dy; b=2*(dy-dx); x=x0; y=y0;
      if (y0<=y1) while (x<x1) {
        if (d<=0) { d+=a; x++; } else { d+=b; x++; y++; }
        if (m(x/tilesize,y/tilesize)) choque_linea=1;
      } else while (x<x1) {
        if (d<=0) { d+=a; x++; } else { d+=b; x++; y--; }
        if (m(x/tilesize,y/tilesize)) choque_linea=1;
      }
    } else  {
      if (y0>y1) {
        if (m(x1/tilesize,y1/tilesize)) choque_linea=1;
        y0--; swap(x0,x1); swap(y0,y1);
      }
      d=2*dx-dy; a=2*dx; b=2*(dx-dy); x=x0; y=y0;
      if (x0<=x1) while (y<y1) {
        if (d<=0) { d+=a; y++; } else { d+=b; y++; x++; }
        if (m(x/tilesize,y/tilesize)) choque_linea=1;
      } else while (y<y1) {
        if (d<=0) { d+=a; y++; } else { d+=b; y++; x--; }
        if (m(x/tilesize,y/tilesize)) choque_linea=1;
      }
    }
  }
}

/* ==========================================================================
 *  path_line(file,code,tilesize,x,y)
 * ========================================================================== */

void path_line(void) {
  int file,code,x,y;
  int *ptr;

  y=pila[sp--];
  x=pila[sp--];
  tile=pila[sp--];
  code=pila[sp--];
  file=pila[sp];

  pila[sp]=0; // Por defecto, y hasta que se demuestre lo contrario

  if (tile<1 || tile>256) { e(151); return; } // Limites tilesize

  // Comprueba limites de file y code

  if (file>max_fpgs || file<0) { e(109); return; }
  if (file) max_grf=1000; else max_grf=2000;
  if (code<=0 || code>=max_grf) { e(110); return; }
  if (g[file].grf==NULL) { e(111); return; }
  if ((ptr=g[file].grf[code])==NULL) { e(121); return; }

  // Toma puntero al mapa, ancho y alto

  an=ptr[13]; al=ptr[14]; map=(byte*)ptr+64+ptr[15]*4;
  if (an<1 || al<1 || an>max_map_size || al>max_map_size) { e(152); return; }

  // Comprueba limites de coordenadas (si estan fuera del mapa retorna 0)

  if (x<0 || y<0 || x>=an*tile || y>=al*tile) return;
  ax=mem[id+_X]; ay=mem[id+_Y];
  if (ax<0 || ay<0 || ax>=an*tile || ay>=al*tile) return;

  // Determina si puede ir desde (ax,ay) hasta (x,y)

  puede_ir(ax,ay,x,y); if (!choque_linea) pila[sp]=1;
}

/* ==========================================================================
 *  path_free(file,code,tilesize,x,y)
 * ========================================================================== */

void path_free(void) {
  int file,code,x,y;
  int *ptr;

  y=pila[sp--];
  x=pila[sp--];
  tile=pila[sp--];
  code=pila[sp--];
  file=pila[sp];

  pila[sp]=0; // Por defecto, y hasta que se demuestre lo contrario

  if (tile<1 || tile>256) { e(151); return; } // Limites tilesize

  // Comprueba limites de file y code

  if (file>max_fpgs || file<0) { e(109); return; }
  if (file) max_grf=1000; else max_grf=2000;
  if (code<=0 || code>=max_grf) { e(110); return; }
  if (g[file].grf==NULL) { e(111); return; }
  if ((ptr=(int *)g[file].grf[code])==NULL) { e(121); return; }

  // Toma puntero al mapa, ancho y alto

  an=ptr[13]; al=ptr[14]; map=(byte*)ptr+64+ptr[15]*4;
  if (an<1 || al<1 || an>max_map_size || al>max_map_size) { e(152); return; }

  // Comprueba limites de coordenadas (si estan fuera del mapa retorna 0)

  if (x<0 || y<0 || x>=an*tile || y>=al*tile) return;

  // Determina si la casilla destino esta libre

  if (!m(x/tile,y/tile)) pila[sp]=1;
}