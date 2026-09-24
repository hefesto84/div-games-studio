/*
 * io_song.c -- musica de tracker (MOD/S3M/XM) con libmikmod para el port DIV.
 *
 * TU separado a proposito: mikmod.h incluye <windows.h>, cuyas macros
 * (PlaySound/CloseWindow/ShowCursor/Rectangle...) chocan con las
 * declaraciones de raylib.h (ver el propio div_io.h: "Los ficheros de
 * implementacion son unidades de compilacion separadas para no mezclar
 * cabeceras"). Por eso la musica vive aqui SOLO con mikmod.h, mientras
 * io_audio.c (raylib, efectos) la inicializa/cierra via io_song_init()/
 * io_song_close().
 *
 * Driver: winmm (waveOut). El driver NO tiene hilo propio (es poll-driven):
 * hay que llamar io_song_update() (-> MikMod_Update() -> DriverUpdate ->
 * VC_WriteBytes) una vez por frame desde el bucle del juego (frame_end());
 * sin ella, tras llenar los ~2 buffers iniciales la musica se queda congelada.
 *
 * Solo hay UNA cancion residente (la que suena), igual que el original
 * JUDAS: divsound.cpp valida el formato en LoadSong() y guarda el fichero
 * en cancion[].ptr; PlaySong() pasa aqui esa copia persistente, io_play_song()
 * detiene lo que suene, (re)analiza con Player_LoadMem() y arranca.
 */

#include "div_io.h"

#include "mikmod.h"

static MODULE *io_song_mod = NULL;
static int io_music_ok = 0;

int io_song_init(void)
{
    io_song_mod = NULL;

    md_mixfreq = 44100;
    md_mode = DMODE_16BITS | DMODE_STEREO | DMODE_SOFT_MUSIC;
    MikMod_RegisterAllDrivers();   /* con config.h solo registra drv_win + drv_nos */
    MikMod_RegisterLoader(&load_s3m);
    MikMod_RegisterLoader(&load_mod);
    MikMod_RegisterLoader(&load_xm);
    io_music_ok = (MikMod_Init("") == 0);
    if (io_music_ok) {
        /* Player_LoadMem(maxchan=0) NO configura las voces por si solo
         * (mloader.c solo llama SetNumVoices si maxchan>0): sin esto
         * vc_softchn queda a 0 y VC_WriteBytes escribe silencio sin avanzar. */
        MikMod_SetNumVoices(32, 0);
    }
    return io_music_ok ? 0 : -1;
}

void io_song_close(void)
{
    io_stop_song();
    if (io_music_ok) {
        MikMod_Exit();
        io_music_ok = 0;
    }
}

void io_song_update(void)
{
    if (io_music_ok)
        MikMod_Update();
}

int io_load_song(void *data, int len, int loop)
{
    MODULE *mod;

    if (!io_music_ok || !data || len <= 0)
        return -1;

    /* Solo validacion: analiza el formato y descarta el modulo. El analisis
     * "de verdad" ocurre en io_play_song() sobre el buffer persistente del
     * slot (mismo patron que el original JUDAS). */
    mod = Player_LoadMem(data, len, 0, 0);
    if (!mod)
        return -1;
    Player_Free(mod);
    return 0;
}

int io_play_song(void *data, int len, int loop)
{
    MODULE *mod;

    if (!io_music_ok || !data || len <= 0)
        return -1;

    io_stop_song();

    mod = Player_LoadMem(data, len, 0, 0);
    if (!mod)
        return -1;
    mod->loop = loop ? 1 : 0;
    if (loop)
        mod->reppos = 0; /* loop=1 en DIV: recomenzar desde el principio */
    io_song_mod = mod;
    Player_Start(mod);
    return 1;
}

void io_stop_song(void)
{
    if (!io_song_mod)
        return;
    Player_Stop();
    Player_Free(io_song_mod);
    io_song_mod = NULL;
}

int io_is_playing_song(void)
{
    if (!io_music_ok || !io_song_mod)
        return 0;
    return Player_Active() ? 1 : 0;
}

int io_song_channels(void)
{
    if (!io_song_mod)
        return 0;
    return io_song_mod->numchn;
}

void io_set_song_pos(int pos)
{
    if (!io_music_ok || !io_song_mod || !Player_Active())
        return;
    MikMod_Lock();
    Player_SetPosition((UWORD)pos);
    MikMod_Unlock();
}

int io_get_song_pos(void)
{
    if (!io_music_ok || !io_song_mod || !Player_Active())
        return -1;
    return Player_GetOrder();
}

int io_get_song_line(void)
{
    if (!io_music_ok || !io_song_mod || !Player_Active())
        return -1;
    return Player_GetRow();
}