#ifndef DIV_IO_H_
#define DIV_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 *  Capa de I/O compartida del port (Fase 1).
 *
 *  Sustituye la I/O DOS del original (v.cpp/vesa.asm, divsound/timer.asm,
 *  divkeybo/mouse/joy, PIT/PIC) por backends raylib, conservando los
 *  contratos internos que consume el codigo DIV:
 *
 *    - Video  : framebuffer 8-bit indexado (lo que era el puntero vga[]) +
 *               paleta de 256 entradas RGB. La conversion a RGBA y el
 *               "present" ocurre solo al subir la textura (UpdateTexture).
 *    - Audio  : API 1:1 con divsound.h (canales 0..31, Frec=ratio Frec/256,
 *               Volumen=0..256).
 *    - Input  : scancodes BIOS/set-1 (las constantes _XXX de divkeybo.h).
 *    - Timer  : QueryPerformanceCounter reemplazando PIT/PIC.
 *
 *  Los ficheros de implementacion son unidades de compilacion separadas
 *  para no mezclar cabeceras (windows.h + raylib.h) en un mismo TU.
 * ========================================================================== */

/* ============================ VIDEO ============================ */

/* Crea la ventana (raylib) y el framebuffer 8-bit indexado de w x h.
 * Devuelve el puntero al framebuffer, o NULL si falla. window_scale es el
 * multiplo de tamano de ventana sobre la resolucion logica (1 = 1:1).
 * La ventana es redimensionable; el contenido se escala al mayor rectangulo
 * con la misma proporcion. */
uint8_t *io_video_init(int w, int h, int window_scale, const char *title);
void io_video_close(void);

/* Cambia la resolucion logica sin cerrar la ventana (set_mode() del lenguaje
 * DIV, que un programa puede llamar varias veces). Recrea el framebuffer y la
 * textura, y reajusta el tamano de la ventana. Devuelve el NUEVO puntero al
 * framebuffer -- el anterior queda liberado, hay que reasignarlo siempre -- o
 * NULL si no se pudo. Si la resolucion ya era esa, devuelve el actual. */
uint8_t *io_video_resize(int w, int h);

/* Con la ventana ya abierta, la reajusta a la mayor escala entera que quepa
 * en el monitor (un 90% de su alto/ancho). Devuelve la escala aplicada. No
 * cambia la resolucion logica. Lo usa el IDE al crear la ventana. */
float io_video_fit_window(int w, int h);

int io_width(void);
int io_height(void);

/* El framebuffer no se mueve mientras no se llame a io_video_resize(). */
uint8_t *io_framebuffer(void);

/* Paleta: 256 entradas RGB (r,g,b,r,g,b,...). Se copia completa o por
 * entrada. El "present" usa una LUT precalculada a partir de esta paleta. */
void io_palette_set(const uint8_t rgb[768]);
void io_palette_set_entry(int index, uint8_t r, uint8_t g, uint8_t b);

/* Convierte el framebuffer a RGBA (via la LUT) y lo dibuja. Equivale al
 * volcado/present del v.cpp original. */
void io_present(void);

/* true cuando el usuario ha pedido cerrar la ventana (boton X o Alt+F4).
 * Solo informa: no cierra nada. */
bool io_window_should_close(void);

/* true si hay una ventana viva (equivalente a raylib IsWindowReady). */
bool io_window_ready(void);

/* Vuelca el framebuffer ya convertido a RGBA en un fichero de imagen
 * (formato deducido de la extension). Util para verificar el render sin
 * mirar la pantalla. */
void io_screenshot(const char *path);

/* Muestra u oculta el cursor del sistema. El IDE dibuja su propio cursor
 * dentro del framebuffer, asi que hay que ocultar el del SO. */
void io_show_cursor(bool visible);

/* Igual que io_present(), pero llama a overlay() dentro del Begin/End de
 * raylib, tras dibujar la textura, para que quien lo use pueda pintar
 * encima (HUD, crosshair, etc.). */
void io_present_ui(void (*overlay)(void));

/* ============================ AUDIO ============================ */

/* Devuelve 0 si ok. El resto de la API replica divsound.h:
 *    - los sonidos viven en un pool de 128 slots (sonido[128] original);
 *    - hay 32 canales; los sonidos se asignan a canales 16..31 (los 16
 *      primeros quedan reservados para musica, igual que DIV);
 *    - Volumen: 0..256 -> raylib 0..1 (Volumen/256);
 *    - Frec (pitch): ratio 0..256 -> raylib pitch = Frec/256 (256 = tono
 *      original, como en (sonido[].freq*Frec)/256 de divsound.cpp).
 * Nota: el flag de loop de io_load_sound: raylib 5.0 no expone una API
 * publica de loop para Sound (rAudioBuffer es opaco); el loop se aplicara
 * cuando se actualice el backend. */
int io_audio_init(void);
void io_audio_close(void);

/* Carga un sonido desde memoria (WAV, o PCM crudo sin cabecera al que se
 * le anade un header WAV sintetico). Devuelve handle 0..127 o -1. */
int io_load_sound(void *data, long len, int loop);
int io_unload_sound(int handle);

/* Devuelve el canal asignado (16..31) o -1. */
int io_play_sound(int handle, int volume, int pitch);
int io_stop_sound(int channel);
int io_change_sound(int channel, int volume, int pitch);
int io_change_channel(int channel, int volume, int panning);
int io_is_playing_sound(int channel);

/* Musica (MOD/S3M/XM) con libmikmod vendored (3rdparty/mikmod), driver
   * winmm. El API refleja el flujo del divsound.cpp original:
   *    - io_load_song() solo VALIDA/analiza el formato (0 ok / -1);
   *    - io_play_song() corta la cancion actual, (re)analiza el buffer que
   *      le pasa divsound (es el copy persistente de la cancion[slot]) y la
   *      arranca de cero. Devuelve 1 ok / -1.
   *    - io_song_update() se llama UNA vez por frame: el driver winmm no
   *      tiene hilo propio (es poll-driven, MikMod_Update() -> DriverUpdate
   *      -> VC_WriteBytes), asi que sin esta llamada la musica se congela a
   *      los ~2 buffers (240ms).
   *    - en paralelo solo hay UNA cancion residente (la que suena), igual que el
   *      original (JUDAS reanalizaba en PlaySong el buffer del slot).
   * El lifetime de `data` es responsabilidad del llamador (divsound mantiene
   * el buffer vivo mientras la cancion sigue cargada en su slot). */
  int  io_song_init(void);
  void io_song_close(void);
  void io_song_update(void);
  int  io_load_song(void *data, int len, int loop);
  int  io_play_song(void *data, int len, int loop);
  void io_stop_song(void);
  int  io_is_playing_song(void);
  int  io_song_channels(void);
  void io_set_song_pos(int pos);
  int  io_get_song_pos(void);
  int  io_get_song_line(void);

/* ============================ INPUT ============================ */

/* Refresca los estados de teclado/raton. Llamar una vez al inicio de cada
 * frame (como el PollEvents del original). */
void io_poll(void);

/* scancode: constantes BIOS/set-1 (las _XXX de divkeybo.h, p. ej. _ESC=0x01,
 * _UP=0x48, _A=0x1E). */
bool io_key_down(int scancode);
bool io_key_pressed(int scancode);

/* --- Teclado estilo INT 16h, para el IDE (ver port/div/ide/port_ide_keybo.c) ---
 *
 * raylib solo refresca el estado del teclado dentro de EndDrawing(). El IDE
 * tiene bucles de espera activa (`do { tecla(); } while (key(_H));`) que no
 * vuelcan pantalla, asi que hace falta poder bombear eventos por separado.
 *
 * Contrato: los eventos NUNCA se pierden aunque nadie los consuma durante
 * varios frames, porque io_input_drain() los pasa de las colas de raylib
 * (que PollInputEvents vacia en cada pasada) a una cola propia persistente. */

/* Drena las colas de raylib a la cola propia. Lo llama io_present() al
 * terminar de dibujar; no hace falta invocarlo a mano. */
void io_input_drain(void);

/* PollInputEvents() + io_input_drain(). Para bucles que no vuelcan pantalla. */
void io_input_pump(void);

/* Evento ya emparejado (ascii + scancode + modificadores), como lo devolvia
 * la BIOS. `ascii` viene en CP850 (la codificacion de las fuentes del IDE);
 * vale 0 si la tecla no produce texto. */
typedef struct {
    unsigned char ascii;
    unsigned char scan;
    unsigned short shift;
} io_key_event_t;

/* Extrae el evento mas antiguo. Devuelve 0 si la cola esta vacia. */
int io_key_event_pop(io_key_event_t *ev);
void io_key_events_clear(void);

/* Estado de pulsacion por scancode set-1. `down` refleja las teclas fisicas
 * mantenidas; `made` marca las que han generado un codigo de pulsacion en
 * esta pasada (pulsacion inicial o repeticion automatica), que es lo que el
 * handler de la IRQ 9 original usaba para (re)activar kbdFLAGS. Cualquiera de
 * los dos punteros puede ser NULL. */
void io_keys_state(unsigned char down[128], unsigned char made[128]);

/* Palabra de estado de modificadores con el formato de INT 16h AH=12h
 * (las constantes SS_xxx de global.h). */
unsigned short io_shift_status(void);

/* Coordenadas del raton en espacio logico (framebuffer). */
void io_get_mouse(int *x, int *y);
void io_get_mouse_delta(int *dx, int *dy);
bool io_mouse_button(int button);        /* 0=izq, 1=der, 2=medio */
bool io_mouse_button_pressed(int button);

/* ============================ TIMER ============================ */

/* Reloj monotono (QPC en Windows). */
void io_timer_init(void);
double io_time_now(void);
uint64_t io_timer_micros(void);

/* Reloj de frames: replica el ciclo del original, donde frame_start()
 * esperaba (spin-wait) hasta que el contador de 100 Hz alcanzaba freloj
 * (por defecto 100/24 = 24 fps). */
typedef struct {
    double frame_interval;
    double frame_start;
} io_frameclock_t;

void io_frameclock_set_fps(io_frameclock_t *fc, double fps);
void io_frameclock_begin(io_frameclock_t *fc);
void io_frameclock_wait(io_frameclock_t *fc);
double io_frameclock_elapsed(io_frameclock_t *fc);

#ifdef __cplusplus
}
#endif

#endif /* DIV_IO_H_ */