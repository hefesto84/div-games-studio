/*
 * io_input.c -- raton y consultas puntuales de teclado.
 *
 * La cola de teclado estilo INT 16h vive en io_keyboard.c y las tablas de
 * traduccion en io_keymap.c.
 */

#include "div_io.h"
#include "io_keymap.h"

#include "raylib.h"

static int mouse_lx = 0;
static int mouse_ly = 0;

void io_poll(void)
{
    Vector2 m;

    /* PORT: normalmente EndDrawing() (io_video.c) ya llama a
     * PollInputEvents() una vez por frame real. Pero cualquier bucle de
     * espera activa que solo llame a read_mouse() sin pasar por volcado()
     * -- p.ej. "while (mouse_b&1) read_mouse();" en divpcm.cpp, el patron
     * DOS de esperar a que se suelte el boton, que en DOS funcionaba solo
     * porque el estado del raton lo actualizaba una IRQ hardware, no una
     * llamada explicita -- nunca vuelve a bombear eventos de ventana: el
     * estado de IsMouseButtonDown() se queda congelado con el boton
     * "pulsado" para siempre (cuelgue real, no solo lento). Se llama aqui
     * tambien para que cualquier llamador de io_poll() (incluida la
     * emulacion de INT 33h del IDE, port_ide_dos.c) quede protegido sin
     * tener que acordarse de encadenar volcado(). Redundante pero inocuo
     * cuando EndDrawing() ya lo hizo este mismo frame. */
    PollInputEvents();

    m = GetMousePosition();

    int winW = GetScreenWidth();
    int winH = GetScreenHeight();
    int vw = io_width();
    int vh = io_height();
    float scale = (float)winW / (float)vw;
    float sh = (float)winH / (float)vh;
    if (sh < scale)
        scale = sh;
    float ox = ((float)winW - vw * scale) * 0.5f;
    float oy = ((float)winH - vh * scale) * 0.5f;

    mouse_lx = (int)((m.x - ox) / scale);
    mouse_ly = (int)((m.y - oy) / scale);
    if (mouse_lx < 0)
        mouse_lx = 0;
    if (mouse_ly < 0)
        mouse_ly = 0;
    if (mouse_lx >= vw)
        mouse_lx = vw - 1;
    if (mouse_ly >= vh)
        mouse_ly = vh - 1;
}

bool io_key_down(int scancode)
{
    int n, i;
    const io_keymap_entry_t *map = io_keymap_entries(&n);

    for (i = 0; i < n; i++)
        if (map[i].scan == scancode && IsKeyDown(map[i].rkey))
            return true;
    return false;
}

bool io_key_pressed(int scancode)
{
    int n, i;
    const io_keymap_entry_t *map = io_keymap_entries(&n);

    for (i = 0; i < n; i++)
        if (map[i].scan == scancode && IsKeyPressed(map[i].rkey))
            return true;
    return false;
}

void io_get_mouse(int *x, int *y)
{
    if (x)
        *x = mouse_lx;
    if (y)
        *y = mouse_ly;
}

void io_get_mouse_delta(int *dx, int *dy)
{
    Vector2 d = GetMouseDelta();
    if (dx)
        *dx = (int)d.x;
    if (dy)
        *dy = (int)d.y;
}

bool io_mouse_button(int button)
{
    return IsMouseButtonDown(button);
}

bool io_mouse_button_pressed(int button)
{
    return IsMouseButtonPressed(button);
}
