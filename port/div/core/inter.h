
#ifdef DEFINIR_AQUI
#define GLOBAL
#else
#define GLOBAL extern
#endif

#include "include.div"

#define INTERPRETE

//�����������������������������������������������������������������������������
// Includes
//�����������������������������������������������������������������������������


#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <direct.h>
#include <signal.h>

#include <bios.h>
#include <dos.h>
#include <i86.h>
#include <graph.h>
#include "math_.h"
#include "divdll.h"
#include "dll.h"

#include "divkeybo.h"
#include "divfli.h"
#include <zlib.h>

#include "judas/judas.h"
#include "judas/timer.h"
#include "divsound.h"

//�����������������������������������������������������������������������������
// Definiciones
//�����������������������������������������������������������������������������

#pragma check_stack(off)

#define DPMI_INT 0x31

#define uchar unsigned char
#define byte unsigned char
#define ushort unsigned short
#define word unsigned short
#define ulong unsigned int
#define dword unsigned int

#define pi 180000
#define radian 57295.77951

//�����������������������������������������������������������������������������
//  Prototipos
//�����������������������������������������������������������������������������

// I-nt�rprete

void error(word);
void exer(int);
void mouse_window(void);

// V-�deo

void set_paleta (void);
void set_dac(void);
void retrazo(void);
void svmode(void);
void rvmode(void);
void init_volcado(void);
void volcado_parcial(int,int,int,int);
void volcado(char *);
void restore(char *,char *);
void init_ghost(void);
void crear_ghost(void);
//void find_color(byte,byte,byte);

// S-prites

void scroll_simple(void);
void scroll_parallax(void);
void put_sprite(int file, int graph, int x, int y, int size, int angle, int flags, int reg,byte *,int,int);
void pinta_sprite(void);
void sp_normal(byte * p, int x, int y, int an, int al, int flags);
void sp_cortado(byte * p, int x, int y, int an, int al, int flags);
void sp_escalado(byte*,int,int,int,int,int,int,int,int);
void sp_rotado(byte*,int,int,int,int,int,int,int,int,int);
void sp_scan(byte * p,short n,byte * si,int an,int x0,int y0,int x1,int y1);
void pinta_textos(int n);
void pinta_drawings(void);
void init_sin_cos(void);
void pinta_m7(int);
int get_distx(int a,int d);
int get_disty(int a,int d);

// F-unciones

void function(void);
void nueva_paleta(void);
void init_rnd(int);

// C-olisiones

void out_region(void);
void graphic_info(void);
void collision(void);
void sp_size_scaled( int *x, int *y, int *xx, int *yy, int xg, int yg, int size, int flags);
void put_collision(byte * buffer, int * ptr, int x, int y, int xg, int yg, int angle, int size, int flags);
void sp_size( int *x, int *y, int *xx, int *yy, int xg, int yg, int ang, int size, int flags);

// D-ebug

#ifdef DEBUG
void debug(void);
extern int debug_active;
#endif

// A-ssembler

void memcpyb(byte * di, byte * si, int n);
void call(int);

// M-ode 8

void loop_mode8(void);

//�����������������������������������������������������������������������������
//      Funciones exportadas por DIVLENGU
//�����������������������������������������������������������������������������

void inicializa_textos(byte * fichero);
void finaliza_textos(void);

//�����������������������������������������������������������������������������
// Constantes
//�����������������������������������������������������������������������������

#define max_exp 512	  // M�ximo n�mero de elementos en una expresi�n
#define long_pila 2048	  // Longitud de la pila en ejecuci�n

#define swap(a,b) {a^=b;b^=a;a^=b;}

//�����������������������������������������������������������������������������
// Mnem�nico/C�digo/Operandos (Generaci�n de c�digo EML, "*" � "a�n no usado")
//�����������������������������������������������������������������������������

#define lnop  0 // *            No operaci�n
#define lcar  1 // valor        Carga una constante en pila
#define lasi  2 //              Saca valor, offset y mete el valor en [offset]
#define lori  3 //              Or l�gico
#define lxor  4 //              Xor, or exclusivo
#define land  5 //              And l�gico, operador sobre condiciones
#define ligu  6 //              Igual, operador logico de comparaci�n
#define ldis  7 //              Distinto, true si los 2 valores son diferentes
#define lmay  8 //              Mayor, comparaci�n con signo
#define lmen  9 //              Menor, idem
#define lmei 10 //              Menor o igual
#define lmai 11 //              Mayor o igual
#define ladd 12 //              Suma dos constantes
#define lsub 13 //              Resta, operaci�n binaria
#define lmul 14 //              Multiplicaci�n
#define ldiv 15 //              Divisi�n de enteros
#define lmod 16 //              M�dulo, resto de la divisi�n
#define lneg 17 //              Negaci�n, cambia de signo una constante
#define lptr 18 //              Pointer, saca offset y mete [offset]
#define lnot 19 //              Negaci�n binaria, bit a bit
#define laid 20 //              Suma id a la constante de la pila
#define lcid 21 //              Carga id en la pila
#define lrng 22 // offset, len  Realiza una comparaci�n de rango
#define ljmp 23 // offset       Salta a una direcci�n de mem[]
#define ljpf 24 // offset       Salta si un valor es falso a una direcci�n
#define lfun 25 // c�digo       Llamada a un proceso interno, ej. signal()
#define lcal 26 // offset       Crea un nuevo proceso en el programa
#define lret 27 // num_par      Auto-eliminaci�n del proceso
#define lasp 28 //              Desecha un valor apilado
#define lfrm 29 // num_par      Detiene por este frame la ejecuci�n del proceso
#define lcbp 30 // num_par      Inicializa el puntero a los par�metros locales
#define lcpa 31 //              Saca offset, lee par�metro [offset] y bp++
#define ltyp 32 // bloque       Define el tipo de proceso actual (colisiones)
#define lpri 33 // offset       Salta a la direcci�n, y carga var. privadas
#define lcse 34 // offset       Si switch <> expresi�n, salta al offfset
#define lcsr 35 // offset       Si switch no esta en el rango, salta al offset
#define lshr 36 //              Shift right (binario)
#define lshl 37 //              Shift left (binario)
#define lipt 38 //              Incremento y pointer
#define lpti 39 //              Pointer e incremento
#define ldpt 40 //              Decremento y pointer
#define lptd 41 //              Pointer y decremento
#define lada 42 //              Add-asignaci�n
#define lsua 43 //              Sub-asignaci�n
#define lmua 44 //              Mul-asignaci�n
#define ldia 45 //              Div-asignaci�n
#define lmoa 46 //              Mod-asignaci�n
#define lana 47 //              And-asignaci�n
#define lora 48 //              Or-asignaci�n
#define lxoa 49 //              Xor-asignaci�n
#define lsra 50 //              Shr-asignaci�n
#define lsla 51 //              Shl-asignaci�n
#define lpar 52 // num_par_pri  Define el n�mero de par�metros privados
#define lrtf 53 // num_par      Auto-eliminaci�n del proceso, devuelve un valor
#define lclo 54 // offset       Crea un clon del proceso actual
#define lfrf 55 // num_par      Pseudo-Frame (frame a un porcentaje, frame(100)==frame)
#define limp 56 // offset text  Importa una DLL externa
#define lext 57 // c�digo       Llama a una funci�n externa
#define lchk 58 //              Comprueba la validez de un identificador
#define ldbg 59 //              Invoca al debugger

// Instrucciones a�adidas para la optimizaci�n (DIV 2.0)

#define lcar2 60
#define lcar3 61
#define lcar4 62
#define lasiasp 63
#define lcaraid 64
#define lcarptr 65
#define laidptr 66
#define lcaraidptr 67
#define lcaraidcpa 68
#define laddptr 69
#define lfunasp 70
#define lcaradd 71
#define lcaraddptr 72
#define lcarmul 73
#define lcarmuladd 74
#define lcarasiasp 75
#define lcarsub 76
#define lcardiv 77

// Instrucciones a�adidas para el manejo de cadenas

#define lptrchr 78 // Pointer, saca (index, offset) y mete [offset+byte index]
#define lasichr 79 // Saca (valor, index, offset) y mete el valor en [offset+byte index]
#define liptchr 80 // Incremento y pointer
#define lptichr 81 // Pointer e incremento
#define ldptchr 82 // Decremento y pointer
#define lptdchr 83 // Pointer y decremento
#define ladachr 84 // Add-asignaci�n
#define lsuachr 85 // Sub-asignaci�n
#define lmuachr 86 // Mul-asignaci�n
#define ldiachr 87 // Div-asignaci�n
#define lmoachr 88 // Mod-asignaci�n
#define lanachr 89 // And-asignaci�n
#define lorachr 90 // Or-asignaci�n
#define lxoachr 91 // Xor-asignaci�n
#define lsrachr 92 // Shr-asignaci�n
#define lslachr 93 // Shl-asignaci�n
#define lcpachr 94 // Saca offset, lee par�metro [offset] y bp++

#define lstrcpy 95 // Saca si, di, y hace strcpy(mem[di],[si]) (deja di en pila)
#define lstrfix 96 // Amplia una cadena antes de meter un char en ella
#define lstrcat 97 // Concatena dos cadenas (opera como strcpy)
#define lstradd 98 // Suma dos strings "en el aire" y deja en pila el puntero al aire
#define lstrdec 99  // A�ade o quita caracteres a una cadena
#define lstrsub 100 // Quita caracteres a una cadena (-=)
#define lstrlen 101 // Sustituye una cadena por su longitud
#define lstrigu 102 // Comparacion de igualdad de dos cadenas
#define lstrdis 103 // Cadenas distintas
#define lstrmay 104 // Cadena mayor
#define lstrmen 105 // Cadena menor
#define lstrmei 106 // Cadena mayor o igual
#define lstrmai 107 // Cadena menor o igual
#define lcpastr 108 // Carga un par�metro en una cadena

// Instrucciones a�adidas para el manejo de Words

#define lptrwor 109 // Pointer, saca (index, offset) y mete [offset+byte index]
#define lasiwor 110 // Saca (valor, index, offset) y mete el valor en [offset+byte index]
#define liptwor 111 // Incremento y pointer
#define lptiwor 112 // Pointer e incremento
#define ldptwor 113 // Decremento y pointer
#define lptdwor 114 // Pointer y decremento
#define ladawor 115 // Add-asignaci�n
#define lsuawor 116 // Sub-asignaci�n
#define lmuawor 117 // Mul-asignaci�n
#define ldiawor 118 // Div-asignaci�n
#define lmoawor 119 // Mod-asignaci�n
#define lanawor 120 // And-asignaci�n
#define lorawor 121 // Or-asignaci�n
#define lxoawor 122 // Xor-asignaci�n
#define lsrawor 123 // Shr-asignaci�n
#define lslawor 124 // Shl-asignaci�n
#define lcpawor 125 // Saca offset, lee par�metro [offset] y bp++

// Miscel�nea

#define lnul    126 // Comprueba que un puntero no sea NULL

//����������������������������������������������������������������������������� // Variables globales de los programas //�����������������������������������������������������������������������������

#define long_header 9    // Longitud de la cabecera al inicio de los programas

GLOBAL int imem_max;     // Memoria principal de la m�quina destino

struct _mouse { // x1
  int x,y,z,file,graph,angle,size,flags,region,left,middle,right,cursor,speed;
};

GLOBAL struct _mouse * mouse;

struct _scroll { // x10
  int z,camera,ratio,speed,region1,region2,x0,y0,x1,y1;
};

GLOBAL struct _scroll * scroll;

struct _m7 { // x10
  int z,camera,height,distance,horizon,focus,color;
};

GLOBAL struct _m7 * m7;

struct _joy { // x1
  int button1,button2,button3,button4;
  int left,right,up,down;
};

GLOBAL struct _joy * joy;

struct _setup { // x1
  int card,port,irq,dma,dma2;
  int master,sound_fx,cd_audio;
  int mixer,mixrate,mixmode;
};

GLOBAL struct _setup * setup;

struct _net { // x1
  int dispositivo,com,velocidad;
  int telefono,cadena_inicio;
  int tonos,servidor,num_players;
  int act_players;
};

GLOBAL struct _net * net;

struct _m8 { // x10
  int z,camera,height,angle;
};

GLOBAL struct _m8 * m8;

struct _dirinfo { // x1
  int files;
  int name[1025];
};

GLOBAL char * filenames;

GLOBAL struct _dirinfo * dirinfo;

struct _fileinfo { // x1
  int fullpath_fix;
  char fullpath[256];  // Nombre completo
  int  drive;          // Unidad de disco
  int dir_fix;
  char dir[256];       // Directorio
  int name_fix;
  char name[12];       // Nombre
  int ext_fix;
  char ext[8];         // Extension
  int  size;           // Tama�o (en bytes)
  int  day;            // Dia
  int  month;          // Mes
  int  year;           // A�o
  int  hour;           // Hora
  int  min;            // Minuto
  int  sec;            // Segundo
  int  attrib;         // Atributos
};

GLOBAL struct _fileinfo * fileinfo;

struct _video_modes { // x100
  int ancho;          // Ancho del modo
  int alto;           // alto del modo
  int modo;           // Numero del modo
};

GLOBAL struct _video_modes * video_modes;

// *** OJO *** Indicar adem�s de aqu� en la inicializaci�n de i.cpp y el div.h ***

#define end_struct long_header+14+10*10+10*7+8+11+9+10*4+1026+146+32*3

#define timer(x) mem[end_struct+x]
#define text_z mem[end_struct+10]
#define fading mem[end_struct+11]
#define shift_status mem[end_struct+12]
#define ascii mem[end_struct+13]
#define scan_code mem[end_struct+14]
#define joy_filter mem[end_struct+15]
#define joy_status mem[end_struct+16]
#define restore_type mem[end_struct+17]
#define dump_type mem[end_struct+18]
#define max_process_time mem[end_struct+19]
#define fps mem[end_struct+20]
#define _argc mem[end_struct+21]
#define _argv(x) mem[end_struct+22+x]
#define channel(x) mem[end_struct+32+x]
#define vsync mem[end_struct+64]
#define draw_z mem[end_struct+65]
#define num_video_modes mem[end_struct+66]
#define unit_size mem[end_struct+67]

GLOBAL int joy_timeout;

void read_joy(void);

//�����������������������������������������������������������������������������
// Variables locales del sistema de sprites (las primeras no son p�blicas)
//�����������������������������������������������������������������������������

#define _Id         0  //Para comprobar validez de accesos externos
#define _IdScan     1  //Recorrido del resto de los procesos (p.ej.colisiones)
#define _Bloque     2  //Identificador del tipo de proceso (para colisiones)
#define _BlScan     3  //Ultimo tipo de proceso scaneado en el �ltimo recorrido
#define _Status     4  //Estado (0 dead, 1 killed, 2 alive, 3 sleept, 4 freezed)
#define _NumPar     5  //N�mero de par�metros del proceso
#define _Param      6  //Puntero a los par�metros pasados al proceso (en pila)
#define _IP         7  //Puntero de ejecuci�n (la siguiente al frame anterior)
#define _SP         8  //Puntero de pila (stack pointer del proceso)
#define _Executed   9  //Indica para cada frame si el proceso ya se ejecut�
#define _Painted    10 //Indica si el proceso ya ha sido pintado

// Las siguientes 2 variables son duales, segun el proceso sea de m7 o m8

#define _Dist1      11 //Distancia 1, para el modo 7
#define _Dist2      12 //Distancia 2, para el modo 7

#define _M8_Object  11 //Objeto dentro del mundo m8
#define _Old_Ctype  12 //Antiguo _Ctype

#define _Frame      13 //Cuanto frame lleva el proceso (frame(n))
#define _x0         14 //Caja ocupada por el sprite cada
#define _y0         15 // vez que se pinta para realizar
#define _x1         16 // volcado y restauraci�n de fondo
#define _y1         17 // parcial (dump_type==0 y restore_background==0)
#define _FCount     18 //Cuenta de llamadas a funcion (para saltarse retornos en frame)
#define _Caller     19   //ID del proceso o funcion llamador (0 si ha sido el kernel)

#ifdef _X
#undef _X
#endif
#ifdef _Z
#undef _Z
#endif

#define _Father     20 //Id del padre del proceso (0 si no existe)
#define _Son        21 //Id del �ltimo hijo que ha creado (0 sne)
#define _SmallBro   22 //Id del hermano menor del proceso (0 sne)
#define _BigBro     23 //Id del hermanos mayor (m�s viejo) del proceso (0 sne)
#define _Priority   24 //Prioridad de proceso (positivo o negativo)
#define _Ctype      25 //Indica si es relativo a pantalla, parallax o mode 7
#define _X          26 //Coordenada x (del centro gravitatorio del gr�fico)
#define _Y          27 //Coordenada y (idem)
#define _Z          28 //Coordenada z (Prioridad para la impresi�n)
#define _Graph      29 //C�digo del gr�fico (se corresponde con los ficheros)
#define _Flags      30 //Define espejados horizontales y verticales
#define _Size       31 //Tama�o (%) del gr�fico
#define _Angle      32 //Angulo de rotaci�n del gr�fico (0 gr�fico normal)
#define _Region     33 //Regi�n con la que hacer el clipping del gr�fico
#define _File       34 //FPG que contiene los gr�ficos del proceso
#define _XGraph     35 //Puntero a tabla: n�graficos,graf_angulo_0,...
#define _Height     36 //Altura de los procesos en el modo 7 (pix/4)
#define _Cnumber    37 //Indica en que scroll o m7 se ver� el gr�fico
#define _Resolution 38 //Resoluci�n de las coordenadas x,y para este proceso
#define _Radius     39 //Radio del objeto en m8
#define _M8_Wall    40 //Pared con la que colisiona
#define _M8_Sector  41 //Sector en el que esta
#define _M8_NextSector 42 //Sector que esta detras de la pared con la que colisiona
#define _M8_Step    43 //Lo que puede subir el sprite en m8 (altura escalon)

//�����������������������������������������������������������������������������
//  Memoria de la m�quina destino
//�����������������������������������������������������������������������������

GLOBAL int pila[long_pila+max_exp+64]; // c�lculo de expresiones (compilaci�n y ejecuci�n)

GLOBAL int * mem, imem, iloc, iloc_pub_len, iloc_len;
GLOBAL byte * memb;
GLOBAL word * memw;

//�����������������������������������������������������������������������������
// Variables globales para la interpretaci�n - VARIABLES DE PROCESO
//�����������������������������������������������������������������������������

GLOBAL int inicio_privadas; // Inicio de variables privadas (proceso en ejecuci�n)

GLOBAL int ip;        // Puntero de programa

GLOBAL int sp;          // Puntero de pila

GLOBAL int bp;          // Puntero auxiliar de pila

GLOBAL int id_init;     // Inicio del proceso init (padre de todos)

GLOBAL int id_start;    // Inicio del primer proceso (sus locales y privadas)

GLOBAL int id_end;      // Inicio del �ltimo proceso hasta el momento

GLOBAL int id_old;      // Para saber por donde se est� procesando

GLOBAL int procesos;    // N�mero de procesos vivos en el programa

GLOBAL int ide,id;      // Proceso en proceso

GLOBAL int id2;         // Identificador extra para las llamadas a procesos (cal)

//�����������������������������������������������������������������������������
// Variables globales para el control de handles
//�����������������������������������������������������������������������������

GLOBAL int numfiles;     // Numero de ficheros abiertos al comenzar el interprete

/* PORT: era "int tabfiles[32]". tabfiles guarda el FILE* real devuelto por
 * fopen() (ver f.cpp: _fopen/_fclose/_fread/_fwrite/_fseek/_ftell/
 * div_filelength), truncado a 32 bits -- en un proceso x64 un FILE* real
 * casi nunca cabe en un int, asi que la version original corrompia el
 * puntero antes incluso de guardarlo. tabfiles es una tabla puramente
 * nativa (nunca la toca el bytecode DIV, solo estas funciones C), asi
 * que ampliar su tipo es seguro y no cambia el formato de bytecode ni el
 * "handle" de 6 bits que SI ve el lenguaje DIV (x*2+1, ver _fopen).
 * Ver docs/architecture/12-port-progreso.md. */
GLOBAL intptr_t tabfiles[32]; // Tabla con los handles abiertos (a 0 los libres)

//�����������������������������������������������������������������������������
// Variables globales para la interpretaci�n - VARIABLES GRAFICAS
//�����������������������������������������������������������������������������

GLOBAL int vga_an,vga_al; // Dimensiones de la pantalla f�sica

GLOBAL byte *copia;     // Copia virtual de pantalla

GLOBAL byte *copia2;    // Segunda copia, fondo de sprites fuera del scroll

#ifdef DEBUG
GLOBAL byte *copia_debug;       // Tercera copia, solo para el debug (dialogos)
#endif

GLOBAL byte paleta[768]; // Paleta actual del programa

GLOBAL int palcrc;      // CRC de la paleta actual del programa

GLOBAL int adaptar_paleta; // Autoadaptar archivos cargados a la paleta activa

GLOBAL byte dac[768];   // Paleta real activa en pantalla

GLOBAL byte dac4[768];  // Paleta multiplicada por 4

GLOBAL int dacout_r,dacout_g,dacout_b,dacout_speed; // Fade, que restar y a que veloc.

GLOBAL int now_dacout_r,now_dacout_g,now_dacout_b; // Situaci�n actual de dac[]

GLOBAL int paleta_cargada; // Indica si ya se ha cargado alguna paleta

GLOBAL int activar_paleta; // Indica si ya se ha cargado alguna paleta

GLOBAL byte * cuad;

//�����������������������������������������������������������������������������
// Textos de salida, en formato traducible
//�����������������������������������������������������������������������������

#define max_textos_sistema 256         // N� m�x. de textos permitidos (lenguaje.div)

GLOBAL byte *text[max_textos_sistema];
GLOBAL int  num_error;

//�����������������������������������������������������������������������������
// Ficheros de gr�ficos (*.FPG de DIV)
//�����������������������������������������������������������������������������

typedef struct _t_g { // Estructura para un fpg
  int * * fpg; // Fichero cargado en memoria
  int * * grf; // Punteros a los gr�ficos (g[n].grf[000..999])
}t_g;

// El primer fpg puede contener hasta 2000 gr�ficos, a partir de 1000 son los
// gr�ficos cargados con load_map (sus c�digos 1000..1999)

GLOBAL int next_map_code,max_grf;

#define max_fpgs 64     // Numero m�ximo de fpgs cargados

GLOBAL t_g g[max_fpgs]; // Array de los fpg

//�����������������������������������������������������������������������������
// Variables gen�ricas usadas por varias funciones
//�����������������������������������������������������������������������������

GLOBAL FILE * es;       // Lectura de ficheros en la interpretaci�n (fpg, voc, ...)

GLOBAL int file_len;    // Lectura de ficheros en la interpretaci�n

GLOBAL word * ptr;      // Puntero general para un malloc en ejecuci�n

GLOBAL int x,y;         // Coordenadas gen�ricas para su uso en funciones internas

GLOBAL float angulo;    // Angulo gen�rico para su uso en funciones internas

//�����������������������������������������������������������������������������
// Sistema de regiones de visualizaci�n
//�����������������������������������������������������������������������������

#define max_region 32   // N�mero m�ximo de regiones definidas

typedef struct _t_region { // Zonas de clipping, referidas a pantalla
  int x0,x1;
  int y0,y1;
}t_region;

GLOBAL t_region region[max_region]; // Array de regiones

GLOBAL int clipx0,clipx1,clipy0,clipy1; // Regi�n de clipping

//�����������������������������������������������������������������������������
// Sistema de font (*.FNT generados con DIV)
//�����������������������������������������������������������������������������

typedef struct _TABLAFNT{
    int ancho;
    int alto;
    int incY;
    int offset;
}TABLAFNT;

typedef struct _fnt_info{
  int ancho;            // Ancho medio del font
  int espacio;          // Longitud en pixels del espacio en blanco
  int espaciado;        // Espaciado entre car�cteres (adem�s del propio ancho)
  int alto;             // Altura m�xima del font
  int fonpal;           // CRC de su paleta
  int syspal;           // CRC de la paleta a la que est� adaptado
  int len;              // Longitud del archivo FNT
  char name[80];        // Nombre del archivo FNT
} fnt_info;

#define max_fonts 32    // N�mero m�ximo de fonts en ejecuci�n

GLOBAL byte * fonts[max_fonts]; // Fonts cargados en ejecuci�n (0-no cargado)

GLOBAL TABLAFNT * fnt;

GLOBAL fnt_info f_i[max_fonts];

//�����������������������������������������������������������������������������
// Sistema de impresi�n de textos
//�����������������������������������������������������������������������������

#define max_textos 256  // N�mero m�ximo de textos en ejecuci�n

typedef struct _t_texto {
  int tipo;     // Tipo de texto 0-normal, 1-&variable
  byte * font;  // Puntero al font (byte h,car[256].an/.dir,data...)
  int x,y;      // Coordenadas del texto
  int ptr;      // Texto
  int centro;   // Tipo de centrado 0-normal (decha), 1-centrado horiz, ...
  int region;   // Regi�n de clipping
  int x0,y0;    // Region ocupada por el texto
  int an,al;    // para los volcados parciales
}t_texto;

GLOBAL t_texto texto[max_textos+1];

//�����������������������������������������������������������������������������
// Sistema de impresi�n de primitivas gr�ficas
//�����������������������������������������������������������������������������

#define max_drawings 256 // N�mero m�ximo de primitivas en ejecuci�n

typedef struct _t_drawing {
  int tipo;     // Tipo de primitiva 0-n/a, 1-linea, ...
  int color;    // color de la primitiva
  int porcentaje; // 0 M�nimo ... 15 Opaco
  int region;   // Regi�n de clipping
  int x0,y0;    // Coordenada sup/izqd de la primitiva
  int x1,y1;    // Coordenada inf/dcha de la primitiva
} t_drawing;

GLOBAL t_drawing drawing[max_drawings];

//�����������������������������������������������������������������������������
// Sistema de volcados parciales (juegos sin scroll) - A�n no implementado
//�����������������������������������������������������������������������������

GLOBAL int volcado_completo; // Indica si se ha modificado toda la copia de vga
                             // Por ahora se mantiene siempre a 1

// Ya se ha implementado, la variable que controla ahora el tipo de
// volcado es la global dump_type, accesible por los programas

//�����������������������������������������������������������������������������
// Sistema de modo 7 - Struct interno
//�����������������������������������������������������������������������������

struct _im7 {
  int on,painted;
  int x,y,an,al;
  byte *map,*ext;
  int map_an,map_al;
  int ext_an,ext_al;
};

GLOBAL struct _im7 im7[10];

//�����������������������������������������������������������������������������
// Sistema de scroll parallax - Struct interno
//�����������������������������������������������������������������������������

#define max_inc 32

typedef struct _tfast { // Tabla de incrementos para el primer plano
  int nt;               // 0..max_inc-1 N� de tramos, >=max_inc Desbordamiento
  short inc[max_inc];   // Salto,datos,salto,datos,...
} tfast;                // Hasta 1024x768

struct _iscroll { // x10
  int on,painted;
  int x,y,an,al;
  byte *_sscr1,*sscr1;
  byte *_sscr2,*sscr2;
  byte *map1,*map2;
  int map1_an,map1_al,map2_an,map2_al;
  int map_flags;
  int map1_x,map1_y,map2_x,map2_y;
  int block1,block2;
  int iscan;
  tfast * fast;
};

// Uso de la tabla fast:
//   iscroll[snum].fast=(tfast*)malloc(iscroll[snum].al*sizeof(tfast));
//   fast=iscroll[snum].fast;
//   fast[n].nt; // iscroll[snum].iscan;

GLOBAL struct _iscroll iscroll[10];

GLOBAL int snum; // Variable para indicar sobre que scroll se trata

//GLOBAL int scroll_on;     // 0-Sin scroll, 1-Un plano, 2-Dos planos
//GLOBAL int scr_x,scr_y;   // Posici�n en copia de la ventana de scroll (regi�n)
//GLOBAL int scr_an,scr_al; // Dimensiones de la ventana de scroll
//GLOBAL byte *_sscr1,*sscr1; // Primer plano de scroll, buffer circular
//GLOBAL byte *_sscr2,*sscr2; // Segundo plano de scroll, buffer circular
//GLOBAL byte *map1,*map2;  // Mapas, primer y segundo plano
//GLOBAL int map1_an,map1_al,map2_an,map2_al; // Dimensiones de los mapas
//GLOBAL int map_flags;     // Indica si los planos son c�clicos, y en que ejes
//GLOBAL int map1_x,map1_y,map2_x,map2_y; // Posici�n del scroll (esq. sup. izda.)
//GLOBAL int block1,block2; // N� de scanes de la primera porci�n de los buffers

//�����������������������������������������������������������������������������
// Sistema de memoria dinamica - Struct interno
//�����������������������������������������������������������������������������

struct _divmalloc {
  byte *ptr;
  int  imem1;
  int  imem2;
};

GLOBAL struct _divmalloc divmalloc[256];

//�����������������������������������������������������������������������������
// Variables relativas a c�lculos de la paleta
//�����������������������������������������������������������������������������

GLOBAL byte * ghost; // Tabla de ghost layering
GLOBAL byte * ghost_inicial; // Las primeras 256 medias de la tabla ghost

GLOBAL byte _r,_g,_b,find_col; // C�lculos sobre la paleta (tabla ghost)
GLOBAL int find_min;

GLOBAL byte last_c1;    // Ultimo color del font del sistema (en paleta cargada)

//�����������������������������������������������������������������������������
// Control de n� m�ximo de frames
//�����������������������������������������������������������������������������

GLOBAL int reloj;   // L�mite de velocidad
GLOBAL int ticks;

GLOBAL int old_reloj; // Para el "timing" de funciones
GLOBAL int ultimo_reloj;
GLOBAL double freloj,ireloj;

GLOBAL int max_saltos;                  // M�ximo n� de volcados saltados

GLOBAL int saltar_volcado,volcados_saltados;

//�����������������������������������������������������������������������������
// Buffer de detecci�n de colisiones
//�����������������������������������������������������������������������������

GLOBAL byte * buffer;                   // Buffer de colisiones
GLOBAL int buffer_an,buffer_al;         // Ancho y alto del buffer

//�����������������������������������������������������������������������������
// Formato de los ficheros gr�ficos fpg
//�����������������������������������������������������������������������������

// char     8   cabecera "fpg\x1a\x0d\x0a\x00\x00"
// char   768   dac
// char 16*36   reglas de color
// --------------------- 1 ---------------------------
// int      1   codigo del grafico (000-999)
// int      1   longitud del grafico incluida cabezera
// char    32   descripcion del grafico
// char    12   nombre del fichero desde el que se creo
// int      1   ancho
// int      1   alto
// int      1   n - n�mero de puntos
// short  2*n   puntos {x,y}
// char an*al   grafico
// --------------------- N ---------------------------

#ifdef DEBUG

//����������������������������������������������������������������������������
//      Include con prototipos y variables relacionadas con el debugger
//����������������������������������������������������������������������������

#define v ventana[0]
#define max_items 24    // N� m�ximo de objetos en una ventana
#define max_windows 8   // N� m�ximo de ventanas

GLOBAL int big,big2; // big(0,1), big2(1,2)
GLOBAL int mouse_graf;

GLOBAL byte c0,c1,c2,c3,c4,text_color; // Colores del entorno
GLOBAL byte c01,c12,c23,c34; // Colores intermedios
GLOBAL byte c_r,c_g,c_b,c_r_low,c_g_low,c_b_low;

GLOBAL byte * fondo_raton; // Buffer para guardar el fondo del rat�n

GLOBAL byte * graf_ptr, * graf[256];    // Gr�ficos del entorno
GLOBAL byte * text_font; // Font est�ndar, 7 puntos de alto, ancho proporcional

GLOBAL int wmouse_x,wmouse_y; // Rat�n dentro de una ventana
GLOBAL int old_mouse_b;

typedef struct _t_item {
  int tipo;             // 0-ninguno,1-boton,2-get,3-switch
  int estado;           // Estado del item (raton sobre �l, pulsado o no ...)
  union {
    struct {
      byte * texto;
      int x,y,center;
    } button;
    struct {
      byte * texto;
      byte * buffer;
      int x,y,an,lon_buffer;
      int r0,r1;
    } get;
    struct {
      byte * texto;
      int * valor;
      int x,y;
    } flag;
  };
}t_item;

typedef struct _tventana {
  int tipo;                             // 0-n/a, 1-di�logo
  int primer_plano;                     // 1-si 0-no (oscurecida)
  byte * titulo;                        // Nombre en la barra de t�tulo
  int paint_handler,click_handler,close_handler;
  int x,y,an,al;                        // Posici�n y dimensiones de la ventana
  byte * ptr;                           // Buffer de la ventana
  int estado;
  int volcar;                           // Indica si se debe volcar la ventana
  t_item item[max_items];        // Botones, gets, switches, etc...
  int items;                            // N� de objetos definidos
  int active_item;                      // Cuando alg�n item produce un efecto
  int selected_item;                    // El item seleccionado (para teclado)
}tventana;

GLOBAL tventana ventana[max_windows];

struct t_listbox{
  int x,y;              // Posici�n del listbox en la ventana
  char * lista;         // El puntero a la lista
  int lista_an;         // N� de car�cteres de cada registro
  int lista_al;         // N� de registros visualizados de una vez
  int an,al;            // Ancho en pixels de la zona de texto
  int inicial;          // Registro inicial visualizado (desde 0)
  int maximo;           // N� total de registros existentes (0 n/a)
  int s0,s1,slide;      // Posici�n inicial, final y actual de la "slide bar"
  int zona;             // Zona seleccionada
  int botones;          // Indica si esta pulsado el bot�n up(1) o down(2)
  int creada;           // Indica si ya est� creada la lista en pantalla
};

GLOBAL char * v_titulo;                 // T�tulo de la ventana
GLOBAL char * v_texto;                  // Texto de la ventana
GLOBAL int v_aceptar;                   // Acceptar / Cancelar

GLOBAL int fin_dialogo;

GLOBAL int debugger_step,call_to_debug,process_stoped;

//����������������������������������������������������������������������������
//      Funciones de debug
//����������������������������������������������������������������������������

void init_debug(void);
void debug(void);
void init_colors(void);
void init_big(void);

//����������������������������������������������������������������������������
//  Breakpoints en el debugger
//����������������������������������������������������������������������������

#define max_breakpoint 32

struct _breakpoint {
  int line;   // Source line for that breakpoint
  int offset; // mem[] offset for debug fixup
  int code;   // code to fixup
};

GLOBAL struct _breakpoint breakpoint[max_breakpoint];

GLOBAL int ibreakpoint;

GLOBAL int new_palette,new_mode;

#endif

GLOBAL int v_function;                  // Funci�n en ejecuci�n actualmente

//����������������������������������������������������������������������������

void frame_start(void);
void frame_end(void);

//����������������������������������������������������������������������������

void find_color(int r,int g,int b);
byte media(byte a,byte b);

GLOBAL int dr,dg,db;

//����������������������������������������������������������������������������
GLOBAL byte kbdFLAGS[128];

#define key(x) kbdFLAGS[x]

void kbdInit(void);
void kbdReset(void);
void tecla(void);
void vacia_buffer(void);

//����������������������������������������������������������������������������

GLOBAL int x0s,x1s,y0s,y1s;    // Regi�n ocupada por un sprite al ser pintado

//����������������������������������������������������������������������������

struct _callback_data {
    unsigned short mouse_action;
    unsigned short mouse_bx;
    unsigned short mouse_cx;
    unsigned short mouse_dx;
};

typedef struct _callback_data callback_data;
extern callback_data cbd;

void mouse_on (void);
void mouse_off (void);
void set_mouse(int,int);

extern int _mouse_x,_mouse_y;

//�����������������������������������������������������������������������������
// Funciones a implementar en DLL
//�����������������������������������������������������������������������������

// DLL_2

GLOBAL void (*set_video_mode)();
GLOBAL void (*process_palette)();
GLOBAL void (*process_active_palette)();

GLOBAL void (*process_sound)(char * sound, int sound_lenght);

GLOBAL void (*process_fpg)(char * fpg, int fpg_lenght);
GLOBAL void (*process_map)(char * map, int map_lenght);
GLOBAL void (*process_fnt)(char * fnt, int fnt_lenght);

GLOBAL void (*background_to_buffer)();
GLOBAL void (*buffer_to_video)();

GLOBAL void (*post_process_scroll)();
GLOBAL void (*post_process_m7)();
GLOBAL void (*pre_process_buffer)();
GLOBAL void (*post_process_buffer)();

GLOBAL void (*post_process)();

GLOBAL void (*putsprite)(byte * si, int x, int y, int an, int al,
                         int xg, int yg, int ang, int size, int flags);

GLOBAL void (*ss_init)();
GLOBAL void (*ss_frame)();
GLOBAL void (*ss_end)();

void text_out(char *texto,int x,int y);
int _random(int min,int max);

GLOBAL int ss_time;
GLOBAL int ss_time_counter;
GLOBAL int ss_status;
GLOBAL int ss_exit;

//����������������������������������������������������������������������������

GLOBAL char packfile[128];
GLOBAL int npackfiles;

struct _packdir {
  char filename[16];
  int offset;
  int len;
  int len_desc;
};

GLOBAL struct _packdir * packdir;

GLOBAL byte * packptr;

int capar(int dir); // Funcion para capar direcciones (0 si outbounds)

GLOBAL char divpath[PATH_MAX+1];
GLOBAL unsigned divnum;

//�����������������������������������������������������������������������������
//  Modos de video
//�����������������������������������������������������������������������������

GLOBAL int VersionVesa;

void detectar_vesa(void);

//����������������������������������������������������������������������������
//  Vuelca informacion en un fichero
//����������������������������������������������������������������������������

void DebugInfo  (char *Msg);
void DebugData  (int Val);

//����������������������������������������������������������������������������

GLOBAL int demo;

//����������������������������������������������������������������������������
//      Mensajes de error en ejecuci�n
//����������������������������������������������������������������������������

void e(int texto);

GLOBAL int omitidos[128];
GLOBAL int nomitidos;

extern char * fname[];

