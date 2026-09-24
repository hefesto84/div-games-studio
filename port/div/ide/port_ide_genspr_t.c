/* PORT: reescritura en C de src/div/visor/t.asm (nucleo de mapeado de
 * texturas del generador de sprites GENSPR, Mapas -> Generador de sprites).
 *
 * El original son 12 procedimientos ensamblador (Watcom, .586): 2
 * variantes -- normal y "con mascara" (color 0 = transparente) --
 * desenrolladas a mano 6 veces, una por cada tamano de textura de
 * potencia de dos soportado (8/16/32/64/128/256 texels de lado). Las 12
 * son la MISMA rutina parametrizada solo por el numero de bits de ancho
 * de textura (aqui "shift": 3..8) -- ver el nucleo generico de abajo.
 *
 * Cada rutina asm empaqueta las coordenadas de textura (u,v) fraccionarias
 * en un unico par de registros edx:ecx, de forma que un solo ADD/ADC
 * avanza ambas coordenadas a la vez (equivalente a sumar un entero de 64
 * bits); luego enmascara y rota (ROL) los bits enteros de u/v para
 * obtener directamente el indice lineal dentro de la textura (evita una
 * multiplicacion). La traduccion de abajo es literal, instruccion a
 * instruccion (verificada a mano contra t.asm), usando aritmetica sin
 * signo de 32/64 bits para reproducir exactamente el comportamiento de
 * SHL/SHLD/ROL/ADC -- no es una reescritura "limpia" del algoritmo.
 *
 * direccion_textura (declarada en t.h) es el puntero a la textura activa,
 * asignado por llrender.cpp antes de pintar. En el .asm original vivia en
 * un dword de 32 bits (valido en el DOS de 1999, donde un puntero cabe en
 * 32 bits); en este port x64 el mismo patron trunca el puntero real y
 * produce lecturas fuera de rango -- ver t.h, ensanchado a intptr_t.
 */

#include <stdint.h>
#include "t.h"

/* Almacenamiento real de "direccion_textura" (t.h solo la declara extern).
 * En t.asm original vivia en el .DATA del propio fichero asm; al no
 * compilarse t.asm en este port, alguien tiene que definirla. */
intptr_t direccion_textura = 0;

static uint32_t rotl32(uint32_t x, int n)
{
    return (n == 0) ? x : ((x << n) | (x >> (32 - n)));
}

/* shift = log2(tamano de textura): 3->8, 4->16, 5->32, 6->64, 7->128, 8->256 */
static void nucleo8_generic(int v_, int u_, int du_, int dv_, int ancho, short *destino, int shift, int con_mascara)
{
    uint32_t v = (uint32_t)v_, u = (uint32_t)u_;
    uint32_t du = (uint32_t)du_, dv = (uint32_t)dv_;
    int shift_v = 16 - shift;       /* = 13,12,11,10,9,8 para shift=3..8 */
    int shift_u = 16 - 2 * shift;   /* = 10,8,6,4,2,0     para shift=3..8 */
    uint32_t mask = (0xFFFFFFFFu << (32 - shift)) | ((1u << shift) - 1u);

    /* Empaquetado inicial (equivalente a los SHL/SHLD del preambulo de
     * cada rutina .asm) y el "paso" que se suma cada texel. */
    uint32_t edx_init = ((u << shift_u) << shift) | ((v << shift_v) >> (32 - shift));
    uint32_t clock1   = ((du << shift_u) << shift) | ((dv << shift_v) >> (32 - shift));
    uint32_t ecx_init = v << 16;
    uint32_t clock2   = dv << 16;

    uint64_t acc  = ((uint64_t)edx_init << 32) | ecx_init;
    uint64_t step = ((uint64_t)clock1 << 32) | clock2;

    const short *textura = (const short *)(intptr_t)direccion_textura;
    int i;

    for (i = 0; i < ancho; i++) {
        uint32_t idx = rotl32(mask & (uint32_t)(acc >> 32), shift);
        short texel = textura[idx];
        if (!con_mascara || texel != 0) destino[i] = texel;
        acc += step;
    }
}

void nucleo8_8(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 3, 0); }
void nucleo8_16(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 4, 0); }
void nucleo8_32(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 5, 0); }
void nucleo8_64(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 6, 0); }
void nucleo8_128(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 7, 0); }
void nucleo8_256(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 8, 0); }

void mask_nucleo8_8(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 3, 1); }
void mask_nucleo8_16(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 4, 1); }
void mask_nucleo8_32(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 5, 1); }
void mask_nucleo8_64(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 6, 1); }
void mask_nucleo8_128(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 7, 1); }
void mask_nucleo8_256(int v, int u, int du, int dv, int ancho, short *destino)
{ nucleo8_generic(v, u, du, dv, ancho, destino, 8, 1); }
