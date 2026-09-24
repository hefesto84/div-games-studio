/*
 * io_keymap.h -- traduccion entre los codigos de tecla de raylib y los
 * scancodes del teclado PC (set 1), que son los que usa DIV en kbdFLAGS y en
 * `scan_code` (constantes _XXX de src/div/divkeybo.h).
 *
 * Uso interno de port/io; no forma parte del contrato publico de div_io.h.
 */

#ifndef IO_KEYMAP_H_
#define IO_KEYMAP_H_

/* Par (tecla raylib, scancode set-1) de las teclas que sabemos traducir.
 * Recorrer esta lista es mucho mas barato que barrer las 512 entradas de la
 * tabla dispersa, y hace falta una vez por llamada a tecla(). */
typedef struct {
    int rkey;           /* KEY_xxx de raylib */
    unsigned char scan; /* scancode set-1 */
    unsigned char text; /* 1 si la tecla puede generar texto imprimible */
} io_keymap_entry_t;

/* Construye las tablas si hace falta. Idempotente. */
void io_keymap_init(void);

/* Lista compacta de teclas conocidas. `count` recibe el numero de entradas. */
const io_keymap_entry_t *io_keymap_entries(int *count);

/* Scancode set-1 de una tecla de raylib, o 0 si no esta mapeada. */
unsigned char io_keymap_scan(int rkey);

/* 1 si la tecla puede producir un caracter imprimible. */
int io_keymap_is_text(int rkey);

/* Convierte un punto de codigo Unicode al byte equivalente en CP850 (la
 * codificacion de las fuentes y los ficheros del IDE). Devuelve 0 si el
 * caracter no es representable. */
unsigned char io_keymap_cp850(int codepoint);

#endif /* IO_KEYMAP_H_ */
