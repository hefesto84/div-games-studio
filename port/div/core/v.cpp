
//�����������������������������������������������������������������������������
// Funciones de video
//�����������������������������������������������������������������������������

/*
 * PORT: este fichero SI se reescribe de verdad (a diferencia de i.cpp/f.cpp/
 * s.cpp/kernel.cpp/divlengu.cpp, portados sin tocar salvo ediciones minimas
 * documentadas). v.cpp es precisamente el unico modulo de div32run que toca
 * hardware de video real (registros VGA/CRTC, DAC, VESA/SVGA por el kit de
 * SciTech) -- ver docs/architecture/08-aspectos-tecnicos.md y
 * docs/architecture/12-port-progreso.md secciones sobre v.cpp. Se elimino
 * todo lo que dependia de <svga.h>/"vesa.h" (SciTech SuperVGA Kit, no
 * vendorizado ni tiene sentido para este port) y de Modo-X (registros de
 * secuenciador para 320x240/360x240/etc a 4 planos), sustituyendolo por la
 * capa port/io (raylib). Las funciones "puras" (sin hardware: composicion
 * de fondo por scan[], tabla ghost de mezcla de colores, snapshot a PCX)
 * se dejan exactamente como en el original.
 */

#include "inter.h"
#include "div_io.h"

//�����������������������������������������������������������������������������
//	Declaraciones y datos a nivel de m�dulo
//�����������������������������������������������������������������������������

byte * vga = NULL; // PORT: antes apuntaba al segmento fisico 0xA0000; ahora
                    // es el framebuffer de 8 bits indexados que devuelve
                    // io_video_init()/io_framebuffer() (ver svmode()).

/* PORT: multiplo de tamano de ventana sobre la resolucion logica del juego
 * (igual que port/main.c). Sin configurar todavia desde fuera; valor fijo
 * razonable para desarrollo. */
#define DIV_PORT_WINDOW_SCALE 3

#define MAX_YRES 2048

short scan[MAX_YRES*4]; // Por scan [x,an,x,an] se definen hasta 2 segmentos a volcar

//�����������������������������������������������������������������������������
//      Tabla ghost
//�����������������������������������������������������������������������������

struct t_tpuntos { // Para la creaci�n de la tabla ghost
  int r,g,b;
  struct t_tpuntos * next;
} tpuntos[256];

struct t_tpuntos * vcubos[512]; // Para la creaci�n de la tabla ghost

extern int fli_palette_update;

//����������������������������������������������������������������������������
// Funci�n para poner la paleta
//����������������������������������������������������������������������������

byte color_oscuro;

void set_paleta (void) {
  word n;

  n=abs(dacout_speed); // if (n>64) n=64;

  if (now_dacout_r<dacout_r) {
    if (now_dacout_r+n<dacout_r) now_dacout_r+=n; else now_dacout_r=dacout_r;
  } else if (now_dacout_r>dacout_r) {
    if (now_dacout_r-n>dacout_r) now_dacout_r-=n; else now_dacout_r=dacout_r;
  }

  if (now_dacout_g<dacout_g) {
    if (now_dacout_g+n<dacout_g) now_dacout_g+=n; else now_dacout_g=dacout_g;
  } else if (now_dacout_g>dacout_g) {
    if (now_dacout_g-n>dacout_g) now_dacout_g-=n; else now_dacout_g=dacout_g;
  }

  if (now_dacout_b<dacout_b) {
    if (now_dacout_b+n<dacout_b) now_dacout_b+=n; else now_dacout_b=dacout_b;
  } else if (now_dacout_b>dacout_b) {
    if (now_dacout_b-n>dacout_b) now_dacout_b-=n; else now_dacout_b=dacout_b;
  }

  n=0; do {
    if (now_dacout_r>paleta[n]) dac[n]=0; else dac[n]=paleta[n]-now_dacout_r;
    if (dac[n]>63) dac[n]=63; n++;
    if (now_dacout_g>paleta[n]) dac[n]=0; else dac[n]=paleta[n]-now_dacout_g;
    if (dac[n]>63) dac[n]=63; n++;
    if (now_dacout_b>paleta[n]) dac[n]=0; else dac[n]=paleta[n]-now_dacout_b;
    if (dac[n]>63) dac[n]=63; n++;
  } while (n<768);

  color_oscuro=0;

  if (process_active_palette!=NULL) process_active_palette();
}

/* PORT: set_dac()/set_dac2() escribian dac[] (paleta VGA de 6 bits/canal,
 * 0..63) directamente al DAC por los puertos 0x3c8/0x3c9, y set_dac()
 * ademas fijaba el "color de borde" con una llamada BIOS (int386 0x10).
 * Ahora: se convierte a RGB de 8 bits (mismo *4 que ya usa el resto del
 * codigo para esta conversion, ver dac4[] en init_ghost) y se sube via
 * io_palette_set(). El color de borde no tiene equivalente (ni falta que
 * hace) en una ventana moderna, se omite sin mas. */
static void port_push_dac(void) {
  int n;
  uint8_t rgb[768];
  for (n=0;n<768;n++) rgb[n]=(uint8_t)(dac[n]*4);
  io_palette_set(rgb);
}

void set_dac (void) {
  if (fli_palette_update) return;
  port_push_dac();
}

void set_dac2 (void) {
  port_push_dac();
}

/* PORT: esperaba el retrazo vertical de la VGA por puerto (0x3da). No
 * aplica con presentacion via GPU/raylib; no-op. Se deja la funcion (y sus
 * llamadas en volcado()) para no tocar el resto del flujo original. */
void retrazo (void) {
}

//�����������������������������������������������������������������������������
//      Set Video Mode (vga_an y vga_al se definen en shared.h)
//�����������������������������������������������������������������������������

extern float m_x,m_y;

/* PORT: el original detectaba y programaba modos VESA/SVGA reales (kit de
 * SciTech) o, si no habia VESA, un modo-X de VGA pura (svmodex, eliminado).
 * Sustituido por una unica llamada a io_video_init(), que crea la ventana
 * raylib y el framebuffer de 8 bits indexados a la resolucion logica que
 * el juego haya pedido (vga_an x vga_al).
 *
 * Un programa DIV elige su resolucion con set_mode() y puede cambiarla
 * varias veces (tipico: 320x200 en la presentacion y 640x480 al entrar al
 * juego), asi que svmode() se llama mas de una vez. Mientras io_video_*
 * no supo redimensionar, la segunda llamada se ignoraba y el nucleo
 * escribia imagenes de la nueva anchura en un framebuffer con la anchura
 * vieja: la pantalla salia desdoblada (stride corto) o desplazada. Ahora
 * la primera vez se crea la ventana y las siguientes se redimensiona. */
void svmode(void) {
  if (!vga) {
    vga = io_video_init(vga_an, vga_al, DIV_PORT_WINDOW_SCALE, "DIV Port");
  } else {
    byte * nuevo = io_video_resize(vga_an, vga_al);
    if (nuevo) vga = nuevo; /* el framebuffer anterior ya no es valido */
  }

  m_x=(float)vga_an/2.0f;
  m_y=(float)vga_al/2.0f;

  if (demo) {
    texto[max_textos].tipo=0;
    texto[max_textos].centro=4;
    texto[max_textos].y=vga_al/2;
    texto[max_textos].x=vga_an/2;
    texto[max_textos].font=(byte*)fonts[0];
  } else texto[max_textos].font=0;
}

//�����������������������������������������������������������������������������
//      Reset Video Mode
//�����������������������������������������������������������������������������

/* PORT: restauraba el modo de texto de DOS al salir (SV_restoreMode +
 * _setvideomode(3)). El cierre de la ventana raylib lo gestiona
 * io_video_close() desde el main() del port; no-op aqui. */
void rvmode(void) {
}

//�����������������������������������������������������������������������������
//      Volcado de un buffer a vga
//�����������������������������������������������������������������������������

/* PORT: el original despachaba a una de 6 rutinas distintas segun
 * resolucion/modo (SVGA con banking, Mode-X con escritura por planos, VGA
 * lineal 320x200) -- volcadop320200/volcadoc320200/volcadopsvga/
 * volcadocsvga/volcadopx/volcadocx/vgacpy, todas eliminadas de este
 * fichero por depender de hardware/SciTech SVGA Kit inexistente aqui. Con
 * port/io el framebuffer es siempre un buffer lineal de 8 bits
 * indexados sea cual sea la resolucion, asi que un unico memcpy sirve
 * para todos los casos; la distincion "volcado completo vs parcial"
 * (volcado_completo) ya no cambia el coste real (copiar de mas es
 * insignificante en hardware moderno), asi que se ignora sin mas --
 * init_volcado()/volcado_parcial() se dejan igual porque otros ficheros
 * los siguen usando para decidir cuando llamar a volcado(). */
void volcado(byte *p) {

  if ((shift_status&4) && (shift_status&8) && key(_P)) {
    snapshot(p);
    do {} while(key(_P));
  }

  if (fli_palette_update) retrazo();

  if (vga) memcpy(vga, p, (size_t)vga_an*vga_al);

  if (fli_palette_update) { fli_palette_update=0; set_dac2(); }
  init_volcado();

  io_present();

  /* PORT: en DOS no habia ventana que cerrar; aqui el usuario puede pulsar
   * la X o ALT+F4 y el programa debe terminar ordenadamente en vez de
   * quedarse inmortal. Ver port_cierre.c. */
  port_check_cierre();
}

//�����������������������������������������������������������������������������
//      Snapshot
//�����������������������������������������������������������������������������

void snapshot(byte *p) {
  FILE * f;
  int n=0;
  char cwork[128];

  do {
    sprintf(cwork,"SNAP%04d.PCX",n++);
    if ((f=fopen(cwork,"rb"))!=NULL) fclose(f);
  } while (f!=NULL);

  f=fopen(cwork,"wb");
  graba_PCX(p,vga_an,vga_al,f);
  fclose(f);
}

typedef struct _pcx_header {
  char manufacturer;
  char version;
  char encoding;
  char bits_per_pixel;
  short  xmin,ymin;
  short  xmax,ymax;
  short  hres;
  short  vres;
  char   palette16[48];
  char   reserved;
  char   color_planes;
  short  bytes_per_line;
  short  palette_type;
  short  Hresol;
  short  Vresol;
  char  filler[54];
}pcx_header;

struct pcx_struct {
  pcx_header header;
  unsigned char far *cimage;
  unsigned char palette[3*256];
  unsigned char far *image;
  int clength;
};

int graba_PCX(byte *mapa,int an,int al,FILE *f) {
  byte p[768];
  int x;
  byte *cbuffer;
  struct pcx_struct pcx;
  int ptr=0;
  int cptr=0;
  int Desborde=0;
  char ActPixel;
  char cntPixel=0;
  char Paletilla=12;

        pcx.header.manufacturer=10;
        pcx.header.version=5;
        pcx.header.encoding=1;
        pcx.header.bits_per_pixel=8;
        pcx.header.xmin=0;
        pcx.header.ymin=0;
        pcx.header.xmax=an-1;
        pcx.header.ymax=al-1;
        pcx.header.hres=an;
        pcx.header.vres=al;
        pcx.header.color_planes=1;
        pcx.header.bytes_per_line=an;
        pcx.header.palette_type=0;
        pcx.header.Hresol=an;
        pcx.header.Vresol=al;

        if ((cbuffer=(unsigned char *)malloc(an*al*2))==NULL) return(1);

        ActPixel=mapa[ptr];
        while (ptr < an*al)
        {
                while((mapa[ptr]==ActPixel) && (ptr<an*al))
                {
                        cntPixel++;
                        Desborde++;
                        ptr++;
                        if(Desborde==an)
                        {
                                Desborde=0;
                                break;
                        }
                        if(cntPixel==63)
                                break;
                }
                if(cntPixel==1)
                {
                        if(ActPixel>63)
                                cbuffer[cptr++] = 193;
                        cbuffer[cptr++] = ActPixel;
                }
                else
                {
                        cbuffer[cptr++] = 192+cntPixel;
                        cbuffer[cptr++] = ActPixel;
                }
                ActPixel=mapa[ptr];
                cntPixel=0;
        }

        fwrite(&pcx.header,1,sizeof(pcx_header),f);
        fwrite(cbuffer,1,cptr,f);
        fwrite(&Paletilla,1,1,f);
        for (x=0;x<768;x++) p[x]=paleta[x]*4;
        fwrite(p,1,768,f);
        free(cbuffer);
        return(0);
}

int graba_MAP (byte * mapa, int an, int al, FILE * f) {
  int y;
  char cwork[32]="";
  char reglas[576];

  fwrite("map\x1a\x0d\x0a\x00\x00",8,1,f);      // +000 Cabecera y version
  fwrite(&an,2,1,f);                   // +008 Ancho
  fwrite(&al,2,1,f);                   // +010 Alto
  y=1; fwrite(&y,4,1,f);// +012 C�digo

  fwrite(cwork,32,1,f);// +016 Descripcion
  fwrite(paleta,768,1,f);                          // +048 Paleta

  for (y=0;y<16;y++) {
    reglas[y*36]=16;
    reglas[y*36+1]=0;
    reglas[y*36+2]=0;
    reglas[y*36+3]=0;
    memset(&reglas[y*36+4],y*16,32);
  } fwrite(reglas,1,sizeof(reglas),f);            // +816 Reglas de color

  y=0; fwrite(&y,2,1,f);                     // +1392 Numero de puntos
  fwrite(mapa,an*al,1,f);
  return(0);
}

//�����������������������������������������������������������������������������
//      Restauraci�n parcial del fondo a la copia
//�����������������������������������������������������������������������������

void restore(byte *q, byte *p) {
  int y=0,n=0;
  if (vga_an<640 && vga_al>200) { // Modo-X
    while (y<vga_al) {
      n=y*4;
      if (scan[n+1]) memcpy(q+scan[n]*4,p+scan[n]*4,scan[n+1]*4);
      if (scan[n+3]) memcpy(q+scan[n+2]*4,p+scan[n+2]*4,scan[n+3]*4);
      q+=vga_an; p+=vga_an; y++;
    }
  } else {
    while (y<vga_al) {
      n=y*4;
      if (scan[n+1]) memcpy(q+scan[n],p+scan[n],scan[n+1]);
      if (scan[n+3]) memcpy(q+scan[n+2],p+scan[n+2],scan[n+3]);
      q+=vga_an; p+=vga_an; y++;
    }
  }
}

//�����������������������������������������������������������������������������
//      Selecciona una ventana para su posterior volcado
//�����������������������������������������������������������������������������

void init_volcado(void) { memset(&scan[0],0,MAX_YRES*8); volcado_completo=0; }

void volcado_parcial(int x,int y,int an,int al) {
  int ymax,xmax,n,d1,d2,x2;

  if (an==vga_an && al==vga_al && x==0 && y==0) { volcado_completo=1; return; }

  if (an>0 && al>0 && x<vga_an && y<vga_al) {
    if (x<0) { an+=x; x=0; } if (y<0) { al+=y; y=0; }
    if (x+an>vga_an) an=vga_an-x; if (y+al>vga_al) al=vga_al-y;
    if (an<=0 || al<=0) return;
    xmax=x+an-1; ymax=y+al-1;

    while (y<=ymax) { n=y*4;
      if (scan[n+1]==0) {         // Caso 1, el scan estaba vac�o ...
        scan[n]=x; scan[n+1]=an;
      } else if (scan[n+3]==0) {  // Caso 2, ya hay un scan definido ...
        if (x>scan[n]+scan[n+1] || x+an<scan[n]) { // ... hueco entre medias
          if (x>scan[n]) {
            scan[n+2]=x; scan[n+3]=an;
          } else {
            scan[n+2]=scan[n]; scan[n+3]=scan[n+1];
            scan[n]=x; scan[n+1]=an;
          }
        } else { // ... no hay hueco, amplia el primer scan
          if (x<(x2=scan[n])) scan[n]=x;
          if (x+an>x2+scan[n+1]) scan[n+1]=x+an-scan[n];
          else scan[n+1]=x2+scan[n+1]-scan[n];
        }
      } else {                    // Caso 3, hay 2 scanes definidos ...
        if (x<=scan[n]+scan[n+1] && x+an>=scan[n+2]) {
          // Caso 3.1, se tapa el hueco anterior -> queda un solo scan
          if (x<scan[n]) scan[n]=x;
          if (x+an>scan[n+2]+scan[n+3]) scan[n+1]=x+an-scan[n]; else scan[n+1]=scan[n+2]+scan[n+3]-scan[n];
          scan[n+2]=0; scan[n+3]=0;
        } else {
          if (x>scan[n]+scan[n+1] || x+an<scan[n]) { // No choca con 1�
            if (x>scan[n+2]+scan[n+3] || x+an<scan[n+2]) { // No choca con 2�
              // Caso 3.4, el nuevo no colisiona con ninguno, se calcula el espacio
              // hasta ambos, y se fusiona con el m�s cercano
              if (x+an<scan[n]) d1=scan[n]-(x+an); else d1=x-(scan[n]+scan[n+1]);
              if (x+an<scan[n+2]) d2=scan[n+2]-(x+an); else d2=x-(scan[n+2]+scan[n+3]);
              if (d1<=d2) {
                // Caso 3.4.1 se fusiona con el primero
                if (x<(x2=scan[n])) scan[n]=x;
                if (x+an>x2+scan[n+1]) scan[n+1]=x+an-scan[n];
                else scan[n+1]=x2+scan[n+1]-scan[n];
              } else {
                // Caso 3.4.2 se fusiona con el segundo
                if (x<(x2=scan[n+2])) scan[n+2]=x;
                if (x+an>x2+scan[n+3]) scan[n+3]=x+an-scan[n+2];
                else scan[n+3]=x2+scan[n+3]-scan[n+2];
              }
            } else {
              // Caso 3.3, el nuevo colisiona con el 2�, se fusionan
              if (x<(x2=scan[n+2])) scan[n+2]=x;
              if (x+an>x2+scan[n+3]) scan[n+3]=x+an-scan[n+2];
              else scan[n+3]=x2+scan[n+3]-scan[n+2];
            }
          } else {
            // Caso 3.2, el nuevo colisiona con el 1�, se fusionan
            if (x<(x2=scan[n])) scan[n]=x;
            if (x+an>x2+scan[n+1]) scan[n+1]=x+an-scan[n];
            else scan[n+1]=x2+scan[n+1]-scan[n];
          }
        }
      } y++;
    }
  }
}

//�����������������������������������������������������������������������������
//      Funciones para la creaci�n de la tabla ghost
//�����������������������������������������������������������������������������

void init_ghost(void) {

  int n,m;
  byte * d=paleta;

  for (n=0;n<768;n++) dac4[n]=paleta[n]*4;

  for (n=0;n<512;n++) vcubos[n]=NULL;

  for (n=0;n<256;n++) {
    tpuntos[n].r=*d++*4; tpuntos[n].g=*d++*4; tpuntos[n].b=*d++*4;
    m=(((int)tpuntos[n].r&224)<<1)+(((int)tpuntos[n].g&224)>>2)+((int)tpuntos[n].b>>5);

    if (vcubos[m]==NULL) {
      vcubos[m]=&tpuntos[n]; tpuntos[n].next=NULL;
    } else {
      tpuntos[n].next=vcubos[m]; vcubos[m]=&tpuntos[n];
    }
  }
}

//�����������������������������������������������������������������������������
//      Funci�n para la creaci�n de la tabla ghost
//�����������������������������������������������������������������������������

int rr,gg,bb;
int num_puntos;

void crear_ghost(void) {

  int n,m;
  int r3,g3,b3,vcubo;
  byte * ptr;

  n=255; do {
    ptr=paleta+n*3; _r=*ptr; _g=*(ptr+1); _b=*(ptr+2); ptr=paleta;
    m=0; do {
      rr=((int)(*ptr+_r)<<7)&0x3f00;
      gg=((int)(*(ptr+1)+_g)<<7)&0x3f00;
      bb=((int)(*(ptr+2)+_b)<<7)&0x3f00;
      ptr+=3;

      r3=(rr&0x3800)>>5; g3=(gg&0x3800)>>8; b3=(bb&0x3800)>>11;
      vcubo=r3+g3+b3;

      find_min=65536;
      num_puntos=0;

      // Cubos de distancia sqr(0) ��������������������������������������������

      crear_ghost_vc(vcubo);

      if (num_puntos>1) goto fast_ghost;

      // Cubos de distancia sqr(1) ��������������������������������������������

      if (r3>0) crear_ghost_vc(vcubo-64);
      if (r3<7*64) crear_ghost_vc(vcubo+64);
      if (g3>0) crear_ghost_vc(vcubo-8);
      if (g3<7*8) crear_ghost_vc(vcubo+8);
      if (b3>0) crear_ghost_vc(vcubo-1);
      if (b3<7) crear_ghost_vc(vcubo+1);

      if (num_puntos>2) goto fast_ghost;

      // Cubos de distancia sqr(2) ��������������������������������������������

      if (r3>0) {
        if (g3>0) crear_ghost_vc(vcubo-64-8);
        else { if (g3<7*8) crear_ghost_vc(vcubo-64+8); }
        if (b3>0) crear_ghost_vc(vcubo-64-1);
        else { if (b3<7) crear_ghost_vc(vcubo-64+1); }
      } else if (r3<7*64) {
        if (g3>0) crear_ghost_vc(vcubo+64-8);
        else { if (g3<7*8) crear_ghost_vc(vcubo+64+8); }
        if (b3>0) crear_ghost_vc(vcubo+64-1);
        else { if (b3<7) crear_ghost_vc(vcubo+64+1); }
      }
      if (g3>0) if (b3>0) crear_ghost_vc(vcubo-8-1);
                else { if (b3<7) crear_ghost_vc(vcubo-8+1); }
      else if (g3<7*8) if (b3>0) crear_ghost_vc(vcubo+8-1);
                else { if (b3<7) crear_ghost_vc(vcubo+8+1); }

      if (find_min==65536) crear_ghost_slow();

      fast_ghost: *(ghost+n*256+m)=find_col;
                  *(ghost+m*256+n)=find_col;

      // if ((punto++&2047)==0) cprintf(".");

    } while (++m<n);
  } while (--n);

  do { *(ghost+n*256+n)=n; } while(++n<256);

  // if (puntos) cprintf(".\r\n");

  memcpy(ghost_inicial,ghost,256);

  n=0; ptr=ghost;
  do {
    *ptr++=n++;
  } while (n<256);

}

void crear_ghost_vc(int m) {

  int dif;
  struct t_tpuntos * p;

  if ((p=vcubos[m])!=NULL) do { num_puntos++;
    dif=*(int*)(cuad+rr+(*p).r);
    dif+=*(int*)(cuad+gg+(*p).g);
    dif+=*(int*)(cuad+bb+(*p).b);
    if (dif<find_min) { find_min=dif;
        find_col=((byte*)p-(byte*)tpuntos)/sizeof(struct t_tpuntos); }
  } while ((p=(*p).next)!=NULL);
}

void crear_ghost_slow (void) {

  int dmin,dif;
  byte *pal,*endpal,*color;

  pal=dac4; endpal=dac4+768; dmin=65536;
  do {
    dif=*(int*)(cuad+rr+*pal); pal++;
    dif+=*(int*)(cuad+gg+*pal); pal++;
    dif+=*(int*)(cuad+bb+*pal); pal+=4;
    if (dif<dmin) { dmin=dif; color=pal-6; }
  } while (pal<endpal);
  find_col=(color-dac4)/3;
}

void find_color(int r,int g,int b) { // Encuentra un color (que no sea el 0)

  int dmin,dif;
  byte *pal,*endpal,*color;

  pal=paleta+3; endpal=paleta+768; dmin=65536;
  do {
    if (((pal-paleta)/3)==last_c1) pal+=3;
    dif=(int)(r-*pal)*(int)(r-*pal); pal++;
    dif+=(int)(g-*pal)*(int)(g-*pal); pal++;
    dif+=(int)(b-*pal)*(int)(b-*pal); pal++;
    if (dif<dmin) { dmin=dif; color=pal-3; }
  } while (pal<endpal);
  find_col=(color-paleta)/3;
}

byte media(byte a,byte b) {
  find_color(
      (paleta[a*3]+paleta[b*3])/2,
      (paleta[a*3+1]+paleta[b*3+1])/2,
      (paleta[a*3+2]+paleta[b*3+2])/2
    );
  return(find_col);
}

//����������������������������������������������������������������������������
