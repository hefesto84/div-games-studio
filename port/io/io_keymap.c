/*
 * io_keymap.c -- tablas de traduccion teclado raylib <-> PC set-1 / CP850.
 *
 * Ver io_keymap.h. La lista `entries` recoge solo las teclas que DIV sabe
 * interpretar; el resto se ignora deliberadamente.
 */

#include "io_keymap.h"

#include <string.h>

#include "raylib.h"

#define MAX_RKEY 512

static unsigned char scan_of[MAX_RKEY];
static unsigned char text_of[MAX_RKEY];
static io_keymap_entry_t entries[160];
static int entry_count = 0;
static int ready = 0;

static void add(int rkey, unsigned char scan, unsigned char text)
{
    if (rkey <= 0 || rkey >= MAX_RKEY)
        return;
    if (entry_count >= (int)(sizeof(entries) / sizeof(entries[0])))
        return;
    scan_of[rkey] = scan;
    text_of[rkey] = text;
    entries[entry_count].rkey = rkey;
    entries[entry_count].scan = scan;
    entries[entry_count].text = text;
    entry_count++;
}

void io_keymap_init(void)
{
    static const unsigned char letters[26] = {
        /* A..Z -> set-1 */
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14,
        0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
    };
    static const unsigned char digits[10] = {
        /* 0..9 -> set-1 */
        0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
    };
    int i;

    if (ready)
        return;
    ready = 1;
    memset(scan_of, 0, sizeof(scan_of));
    memset(text_of, 0, sizeof(text_of));

    /* --- Teclas de control (no generan texto) --- */
    add(KEY_ESCAPE, 0x01, 0);
    add(KEY_BACKSPACE, 0x0E, 0);
    add(KEY_TAB, 0x0F, 0);
    add(KEY_ENTER, 0x1C, 0);
    add(KEY_KP_ENTER, 0x1C, 0);
    add(KEY_CAPS_LOCK, 0x3A, 0);
    add(KEY_NUM_LOCK, 0x45, 0);
    add(KEY_SCROLL_LOCK, 0x46, 0);

    add(KEY_LEFT_SHIFT, 0x2A, 0);
    add(KEY_RIGHT_SHIFT, 0x36, 0);
    add(KEY_LEFT_CONTROL, 0x1D, 0);
    add(KEY_RIGHT_CONTROL, 0x5A, 0);
    add(KEY_LEFT_ALT, 0x38, 0);
    add(KEY_RIGHT_ALT, 0x38, 0);

    /* Bloque de edicion/cursor. En DOS estas teclas "grises" mandaban el
     * prefijo E0 y el mismo scancode que el teclado numerico, asi que DIV
     * acaba mirando siempre estos codigos (ver divmouse.cpp, que consulta
     * _RIGHT y _C_RIGHT indistintamente). */
    add(KEY_INSERT, 0x52, 0);
    add(KEY_DELETE, 0x53, 0);
    add(KEY_HOME, 0x47, 0);
    add(KEY_END, 0x4F, 0);
    add(KEY_PAGE_UP, 0x49, 0);
    add(KEY_PAGE_DOWN, 0x51, 0);
    add(KEY_UP, 0x48, 0);
    add(KEY_DOWN, 0x50, 0);
    add(KEY_LEFT, 0x4B, 0);
    add(KEY_RIGHT, 0x4D, 0);

    for (i = 0; i < 10; i++)
        add(KEY_F1 + i, (unsigned char)(0x3B + i), 0);
    add(KEY_F11, 0x57, 0);
    add(KEY_F12, 0x58, 0);

    /* --- Teclas que producen texto --- */
    for (i = 0; i < 26; i++)
        add(KEY_A + i, letters[i], 1);

    add(KEY_ZERO, digits[0], 1);
    for (i = 1; i < 10; i++)
        add(KEY_ONE + (i - 1), digits[i], 1);

    add(KEY_SPACE, 0x39, 1);
    add(KEY_MINUS, 0x0C, 1);
    add(KEY_EQUAL, 0x0D, 1);
    add(KEY_LEFT_BRACKET, 0x1A, 1);
    add(KEY_RIGHT_BRACKET, 0x1B, 1);
    add(KEY_SEMICOLON, 0x27, 1);
    add(KEY_APOSTROPHE, 0x28, 1);
    add(KEY_GRAVE, 0x29, 1);
    add(KEY_BACKSLASH, 0x2B, 1);
    add(KEY_COMMA, 0x33, 1);
    add(KEY_PERIOD, 0x34, 1);
    add(KEY_SLASH, 0x35, 1);

    /* Teclado numerico: con NumLock comparte scancodes con el bloque de
     * cursor, igual que en el PC real. */
    add(KEY_KP_0, 0x52, 1);
    add(KEY_KP_1, 0x4F, 1);
    add(KEY_KP_2, 0x50, 1);
    add(KEY_KP_3, 0x51, 1);
    add(KEY_KP_4, 0x4B, 1);
    add(KEY_KP_5, 0x4C, 1);
    add(KEY_KP_6, 0x4D, 1);
    add(KEY_KP_7, 0x47, 1);
    add(KEY_KP_8, 0x48, 1);
    add(KEY_KP_9, 0x49, 1);
    add(KEY_KP_DECIMAL, 0x53, 1);
    add(KEY_KP_DIVIDE, 0x35, 1);
    add(KEY_KP_MULTIPLY, 0x37, 1);
    add(KEY_KP_SUBTRACT, 0x4A, 1);
    add(KEY_KP_ADD, 0x4E, 1);
}

const io_keymap_entry_t *io_keymap_entries(int *count)
{
    io_keymap_init();
    if (count)
        *count = entry_count;
    return entries;
}

unsigned char io_keymap_scan(int rkey)
{
    io_keymap_init();
    if (rkey <= 0 || rkey >= MAX_RKEY)
        return 0;
    return scan_of[rkey];
}

int io_keymap_is_text(int rkey)
{
    io_keymap_init();
    if (rkey <= 0 || rkey >= MAX_RKEY)
        return 0;
    return text_of[rkey] != 0;
}

/* Suplemento Latin-1 (U+00A0..U+00FF) -> CP850. 0 = no representable. */
static const unsigned char latin1_to_cp850[96] = {
    255, 173, 189, 156, 207, 190, 221, 245, 249, 184, 166, 174, 170, 240, 169, 238,
    248, 241, 253, 252, 239, 230, 244, 250, 247, 251, 167, 175, 172, 171, 243, 168,
    183, 181, 182, 199, 142, 143, 146, 128, 212, 144, 210, 211, 222, 214, 215, 216,
    209, 165, 227, 224, 226, 229, 153, 158, 157, 235, 233, 234, 154, 237, 232, 225,
    133, 160, 131, 198, 132, 134, 145, 135, 138, 130, 136, 137, 141, 161, 140, 139,
    208, 164, 149, 162, 147, 228, 148, 246, 155, 151, 163, 150, 129, 236, 231, 152
};

unsigned char io_keymap_cp850(int codepoint)
{
    if (codepoint >= 32 && codepoint < 127)
        return (unsigned char)codepoint;
    if (codepoint >= 0xA0 && codepoint <= 0xFF)
        return latin1_to_cp850[codepoint - 0xA0];

    switch (codepoint) {
    case 0x0192: return 159; /* f minuscula con gancho */
    case 0x0131: return 213; /* i sin punto */
    case 0x2017: return 242; /* doble subrayado */
    case 0x25A0: return 254; /* cuadrado solido */
    default:     return 0;
    }
}
