#include "div_io.h"

#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"

static uint8_t *fb = NULL;
static int vw = 0;
static int vh = 0;
static int win_scale = 1;

static Color lut[256];
static uint8_t *rgba = NULL;
static Texture2D tex = { 0 };

/* Crea/recrea la textura de presentacion a la resolucion logica actual. */
static bool io_video_make_texture(void)
{
    Image img = {
        .data = rgba,
        .width = vw,
        .height = vh,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };

    if (tex.id != 0) {
        UnloadTexture(tex);
        tex.id = 0;
    }
    tex = LoadTextureFromImage(img);
    if (tex.id == 0)
        return false;
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return true;
}

uint8_t *io_video_init(int w, int h, int window_scale, const char *title)
{
    if (fb || IsWindowReady())
        return NULL;

    vw = w;
    vh = h;
    win_scale = window_scale > 0 ? window_scale : 1;

    fb = (uint8_t *)calloc((size_t)vw * vh, 1);
    if (!fb)
        return NULL;

    rgba = (uint8_t *)malloc((size_t)vw * vh * 4);
    if (!rgba) {
        free(fb);
        fb = NULL;
        return NULL;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(vw * window_scale, vh * window_scale, title);
    /* PORT: raylib cierra la ventana con ESC por defecto; el IDE de DIV usa
     * ESC constantemente (cerrar dialogos), asi que se desactiva. */
    SetExitKey(KEY_NULL);

    for (int i = 0; i < 256; i++) {
        lut[i].r = 0;
        lut[i].g = 0;
        lut[i].b = 0;
        lut[i].a = 255;
    }

    Image img = {
        .data = rgba,
        .width = vw,
        .height = vh,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    SetTargetFPS(60);

    return fb;
}
/* Cambio de resolucion en caliente (set_mode() del lenguaje DIV).
 *
 * Un programa DIV elige su resolucion en tiempo de ejecucion y puede
 * cambiarla varias veces (tipico: 320x200 al arrancar y 640x480 al entrar
 * al juego). Mientras esto no existio, el nucleo escribia una imagen de
 * la nueva anchura dentro de un framebuffer con la anchura vieja: la imagen
 * salia desdoblada o desplazada, porque el "stride" no coincidia.
 *
 * Se recrean framebuffer, buffer RGBA y textura, y se ajusta la ventana a un
 * multiplo entero que quepa en el monitor (la presentacion escala y centra
 * igualmente, asi que un tamano no exacto tampoco rompe nada).
 */
uint8_t *io_video_resize(int w, int h)
{
    uint8_t *nfb;
    uint8_t *nrgba;
    int escala, max_w, max_h;

    if (w <= 0 || h <= 0)
        return fb;
    if (!fb || !IsWindowReady())
        return NULL;
    if (w == vw && h == vh)
        return fb;

    nfb = (uint8_t *)calloc((size_t)w * h, 1);
    nrgba = (uint8_t *)malloc((size_t)w * h * 4);
    if (!nfb || !nrgba) {
        free(nfb);
        free(nrgba);
        return NULL;
    }

    free(fb);
    free(rgba);
    fb = nfb;
    rgba = nrgba;
    vw = w;
    vh = h;

    if (!io_video_make_texture()) {
        /* La textura es solo presentacion: el framebuffer nuevo ya es valido
         * y hay que devolverlo igualmente, o el nucleo se quedaria con el
         * puntero viejo (ya liberado). */
        return fb;
    }

    escala = win_scale;
    max_w = GetMonitorWidth(GetCurrentMonitor());
    max_h = GetMonitorHeight(GetCurrentMonitor());
    while (escala > 1 && (w * escala > max_w * 9 / 10 || h * escala > max_h * 9 / 10))
        escala--;
    SetWindowSize(w * escala, h * escala);

    return fb;
}

/* Devuelve la escala real de presentacion (window_w / w) que va a aplicar
 * io_present_ui sobre la resolucion logica en el estado actual de la ventana.
 * Sirve para que el IDE decida si activa la letra doble (big) automatica. */
float io_video_fit_window(int w, int h)
{
    int escala = 1;
    int max_w, max_h;
    int mon = GetCurrentMonitor();
    Vector2 monpos;

    if (w <= 0 || h <= 0 || !IsWindowReady())
        return 1.0f;

    max_w = GetMonitorWidth(mon);
    max_h = GetMonitorHeight(mon);

    /* Si la resolucion logica ya se sale del monitor a 1:1, la ventana no
     * puede crecer: se ajusta a la mayor que quepa (la presentacion escala
     * y centra el framebuffer a la ventana en io_present_ui). */
    if (w > max_w * 9 / 10 || h > max_h * 9 / 10) {
        float aspect = (float)w / (float)h;
        int ww = max_w * 9 / 10;
        int wh = (int)(ww / aspect);
        if (wh > max_h * 9 / 10) {
            wh = max_h * 9 / 10;
            ww = (int)(wh * aspect);
        }
        if (ww < 1)
            ww = 1;
        if (wh < 1)
            wh = 1;
        win_scale = 1;
        SetWindowSize(ww, wh);
        goto centrar;
    }

    while (escala < 8 && w * (escala + 1) <= max_w * 9 / 10 &&
           h * (escala + 1) <= max_h * 9 / 10)
        escala++;
    win_scale = escala;
    SetWindowSize(w * escala, h * escala);

centrar:
    /* PORT: InitWindow() no centra la ventana en el monitor. Redimensionar y
     * reposicionarla, para que la barra de menus de arriba no quede fuera de
     * pantalla (ocurria con resoluciones mayores que el monitor, p.ej.
     * 3840x2160 en un 1920x1080). */
    monpos = GetMonitorPosition(mon);
    SetWindowPosition((int)monpos.x + (max_w - GetScreenWidth()) / 2,
                      (int)monpos.y + (max_h - GetScreenHeight()) / 2);

    return (float)GetScreenWidth() / (float)w;
}

void io_video_close(void)
{
    if (tex.id != 0) {
        UnloadTexture(tex);
        tex.id = 0;
    }
    if (IsWindowReady())
        CloseWindow();
    free(rgba);
    rgba = NULL;
    free(fb);
    fb = NULL;
    vw = vh = 0;
}

int io_width(void)
{
    return vw;
}

int io_height(void)
{
    return vh;
}

uint8_t *io_framebuffer(void)
{
    return fb;
}

bool io_window_should_close(void)
{
    if (!IsWindowReady())
        return false;
    return WindowShouldClose();
}

bool io_window_ready(void)
{
    return IsWindowReady();
}

void io_palette_set(const uint8_t rgb[768])
{
    for (int i = 0; i < 256; i++) {
        lut[i].r = rgb[i * 3 + 0];
        lut[i].g = rgb[i * 3 + 1];
        lut[i].b = rgb[i * 3 + 2];
        lut[i].a = 255;
    }
}

void io_palette_set_entry(int index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < 0 || index > 255)
        return;
    lut[index].r = r;
    lut[index].g = g;
    lut[index].b = b;
    lut[index].a = 255;
}

void io_present_ui(void (*overlay)(void))
{
    uint32_t *dst = (uint32_t *)rgba;
    for (int i = 0, n = vw * vh; i < n; i++) {
        Color c = lut[fb[i]];
        dst[i] = ((uint32_t)c.r << 0) | ((uint32_t)c.g << 8) |
                 ((uint32_t)c.b << 16) | (0xFFu << 24);
    }

    UpdateTexture(tex, rgba);

    int winW = GetScreenWidth();
    int winH = GetScreenHeight();
    float scale = (float)winW / (float)vw;
    float sh = (float)winH / (float)vh;
    if (sh < scale)
        scale = sh;

    float dw = vw * scale;
    float dh = vh * scale;
    Vector2 pos = { (winW - dw) * 0.5f, (winH - dh) * 0.5f };

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextureEx(tex, pos, 0.0f, scale, WHITE);
    if (overlay)
        overlay();
    EndDrawing();

    /* PORT: EndDrawing() termina llamando a PollInputEvents(), que vacia las
     * colas de teclado de raylib. Hay que trasvasarlas ya o se pierden los
     * eventos hasta que alguien las lea. Ver port/io/io_keyboard.c. */
    io_input_drain();
}

void io_present(void)
{
    io_present_ui(NULL);
}

void io_screenshot(const char *path)
{
    Image img = {
        .data = rgba,
        .width = vw,
        .height = vh,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    ExportImage(img, path);
}

void io_show_cursor(bool visible)
{
    if (visible)
        ShowCursor();
    else
        HideCursor();
}