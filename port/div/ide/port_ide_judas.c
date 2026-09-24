/*
 * port_ide_judas.c -- shim de compatibilidad para la API real de JUDAS que
 * llama divpcm.cpp (editor de sonido/PCM del IDE), implementado sobre
 * port/io/io_audio.c (efectos, ya en produccion en el runtime) e
 * port/io/io_song.c (tracker MOD/S3M/XM via libmikmod, idem).
 *
 * Esto NO es un port de JUDAS: JUDAS es su propio motor de mezcla completo
 * (~9.400 lineas en 3rdparty/judas/, sin compilar en este port). Se
 * reimplementan solo las funciones que divpcm.cpp llama de verdad (ver
 * docs/architecture/13-handoff.md, hito "audio del IDE"), con la fidelidad
 * que permite delegar en raylib/libmikmod en vez de un mixer propio:
 *
 *  - Muestras (WAV/RAW): reales. judas_loadwav()/judas_loadrawsample() (y
 *    sus variantes _mem) normalizan a mono 16 bits en memoria, igual que
 *    el JUDAS real (ver 3rdparty/judas/judaswav.c/judasraw.c, consultados
 *    como referencia); judas_playsample() envuelve esos datos en un WAV
 *    minimo al vuelo y los reproduce via io_load_sound()/io_play_sound().
 *    Sin loop real (VM_LOOP se ignora: es un editor de preescucha, no un
 *    reproductor de bucles) y sin panning fino (se aproxima con el pan de
 *    raylib).
 *  - Tracker (XM/S3M/MOD): reales, via io_load_song()/io_play_song()
 *    (autodetectan el formato con libmikmod), asi que judas_loadxm()/
 *    judas_loads3m()/judas_loadmod() aceptan cualquiera de los tres --
 *    SongType puede no coincidir exactamente con el formato real si el
 *    llamador prueba xm primero y acierta con un .mod, pero la
 *    reproduccion es correcta igualmente (mismo comportamiento aceptado
 *    como parte del shim, no una regresion nueva).
 *  - judas_getvumeter(): siempre 0 (medidor plano). Extraer el nivel real
 *    por canal exigiria enganchar el mixer interno de libmikmod; fuera de
 *    alcance de este shim (degradacion aceptada).
 *  - Grabacion (RecordSound()/PollRecord() en divpcm.cpp, via DSP/DMA real
 *    de Sound Blaster) NO se implementa: no hay hardware real que grabar en
 *    un PC moderno. Los simbolos que necesita (sbinit, sbrec, spkon y
 *    similares, MIX_SetVolume, set_mixer, timer_uninit) son no-ops en
 *    port_ide_stubs.c.
 */

#include "global.h"
#include "judas/judas.h"
#include "div_io.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Estado global JUDAS que el resto del IDE espera encontrar          */
/* ------------------------------------------------------------------ */

int judas_error = JUDAS_OK;      /* judas.c:178 */
CHANNEL judas_channel[CHANNELS]; /* judas.c:217 */

/* PORT: handle de io_audio.c (io_load_sound) para el WAV que este canal
 * logico de JUDAS esta reproduciendo ahora mismo, o -1. Separado de
 * judas_channel[].fractpos (que aqui se reaprovecha para el CANAL de
 * reproduccion de io_audio, no de handle de muestra) porque el mismo
 * handle no puede reutilizarse: cada judas_playsample() construye un WAV
 * nuevo (la frecuencia de reproduccion es un parametro por-llamada en la
 * API real de JUDAS, no algo fijo en la muestra). */
static int port_judas_smp_handle[CHANNELS];
static int port_judas_init_done = 0;

static void port_judas_ensure_init(void)
{
    int i;
    if (port_judas_init_done)
        return;
    port_judas_init_done = 1;
    memset(judas_channel, 0, sizeof(judas_channel));
    for (i = 0; i < CHANNELS; i++)
        port_judas_smp_handle[i] = -1;
}

void judas_uninit(void)
{
    unsigned c;
    for (c = 0; c < CHANNELS; c++)
        judas_stopsample(c);
    io_stop_song();
}

int judas_songisplaying(void)
{
    return io_is_playing_song();
}

/* ------------------------------------------------------------------ */
/*  Muestras: reserva/liberacion                                       */
/* ------------------------------------------------------------------ */

/* PORT: vuprofile (perfil de VU precalculado del mixer real de JUDAS, que
 * aqui no existe) se reaprovecha para nada -- se deja a NULL. El tamano
 * real de la muestra se calcula siempre como (end - start), no hace falta
 * guardarlo aparte. */

SAMPLE *judas_allocsample(int length)
{
    SAMPLE *smp;

    port_judas_ensure_init();

    if (length <= 0) {
        judas_error = JUDAS_ILLEGAL_CONFIG;
        return NULL;
    }
    smp = (SAMPLE *)calloc(1, sizeof(SAMPLE));
    if (!smp) {
        judas_error = JUDAS_OUT_OF_MEMORY;
        return NULL;
    }
    smp->start = (char *)calloc(1, (size_t)length);
    if (!smp->start) {
        free(smp);
        judas_error = JUDAS_OUT_OF_MEMORY;
        return NULL;
    }
    smp->repeat = smp->start;
    smp->end = smp->start + length;
    smp->vuprofile = NULL;
    smp->voicemode = VM_OFF;
    judas_error = JUDAS_OK;
    return smp;
}

/* PORT: en el JUDAS real, prepara los bytes justo antes/despues del bucle
 * para que el mixer propio interpole sin clics al cruzar el punto de
 * repeticion. Aqui no hay mixer propio (raylib hace la mezcla real) y el
 * loop no se implementa (ver comentario de cabecera) -- no-op real. */
void judas_ipcorrect(SAMPLE *smp)
{
    (void)smp;
}

void judas_freesample(SAMPLE *smp)
{
    if (!smp)
        return;
    free(smp->start);
    free(smp);
}

/* ------------------------------------------------------------------ */
/*  Muestras: construir un WAV minimo en memoria a partir de PCM crudo  */
/* ------------------------------------------------------------------ */

#pragma pack(push, 1)
typedef struct {
    char rifftext[4];
    unsigned totallength;
    char wavetext[4];
    char formattext[4];
    unsigned formatlength;
    unsigned short format;
    unsigned short channels;
    unsigned freq;
    unsigned avgbytes;
    unsigned short blockalign;
    unsigned short bits;
    char datatext[4];
    unsigned datalength;
} port_wav_header_t;
#pragma pack(pop)

/* Construye un WAV PCM mono de 16 bits en un buffer nuevo (malloc). NULL si
 * falla. `*out_len` recibe el tamano total del buffer devuelto. */
static unsigned char *port_build_wav16(const void *pcm, long pcm_len, int freq, long *out_len)
{
    port_wav_header_t h;
    unsigned char *buf;
    long total;

    if (!pcm || pcm_len <= 0 || freq <= 0)
        return NULL;

    memcpy(h.rifftext, "RIFF", 4);
    memcpy(h.wavetext, "WAVE", 4);
    memcpy(h.formattext, "fmt ", 4);
    memcpy(h.datatext, "data", 4);
    h.formatlength = 16;
    h.format = 1; /* PCM */
    h.channels = 1;
    h.freq = (unsigned)freq;
    h.bits = 16;
    h.blockalign = 2;
    h.avgbytes = h.freq * h.blockalign;
    h.datalength = (unsigned)pcm_len;
    h.totallength = (unsigned)(sizeof(h) - 8 + pcm_len);

    total = (long)sizeof(h) + pcm_len;
    buf = (unsigned char *)malloc((size_t)total);
    if (!buf)
        return NULL;

    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), pcm, (size_t)pcm_len);

    *out_len = total;
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Muestras: reproduccion                                             */
/* ------------------------------------------------------------------ */

void judas_playsample(SAMPLE *smp, unsigned chnum, unsigned frequency,
                       unsigned short volume, unsigned char panning)
{
    unsigned char *wav;
    long wavlen, pcmlen;
    int handle, chan;

    port_judas_ensure_init();

    if (chnum >= CHANNELS)
        return;

    judas_stopsample(chnum);

    if (!smp || !smp->start || smp->end <= smp->start || frequency == 0)
        return;

    pcmlen = (long)(smp->end - smp->start);
    wav = port_build_wav16(smp->start, pcmlen, (int)frequency, &wavlen);
    if (!wav)
        return;

    handle = io_load_sound(wav, wavlen, 0);
    free(wav);
    if (handle < 0)
        return;

    chan = io_play_sound(handle, (int)(volume >> 8), 256);
    if (chan < 0) {
        io_unload_sound(handle);
        return;
    }
    io_change_channel(chan, (int)(volume >> 8), panning);

    judas_channel[chnum].smp = smp;
    judas_channel[chnum].panning = panning;
    judas_channel[chnum].voicemode = VM_ON;
    judas_channel[chnum].fractpos = (unsigned short)chan; /* PORT: reaprovechado: canal real de io_audio */
    port_judas_smp_handle[chnum] = handle;
}

void judas_stopsample(unsigned chnum)
{
    port_judas_ensure_init();

    if (chnum >= CHANNELS)
        return;
    if (port_judas_smp_handle[chnum] >= 0) {
        io_stop_sound((int)judas_channel[chnum].fractpos);
        io_unload_sound(port_judas_smp_handle[chnum]);
        port_judas_smp_handle[chnum] = -1;
    }
    memset(&judas_channel[chnum], 0, sizeof(CHANNEL));
}

float judas_getvumeter(unsigned chnum)
{
    (void)chnum;
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/*  WAV: carga real, normalizando siempre a mono 16 bits (igual que el   */
/*  JUDAS real -- ver 3rdparty/judas/judaswav.c, consultado como         */
/*  referencia de la semantica exacta, no compilado)                    */
/* ------------------------------------------------------------------ */

static SoundInfo *port_wav_fail(SoundInfo *SI, int err)
{
    judas_error = err;
    return SI;
}

/* Busca una subcadena de 4 bytes ("fmt "/"data") a partir de `pos` dentro
 * de `len` bytes, devolviendo el offset justo despues de la etiqueta+4
 * bytes de longitud de chunk, o -1 si no cabe/no aparece antes de agotar
 * el buffer. Recorre chunk a chunk (RIFF es una lista de chunks
 * etiqueta+longitud+datos), igual que el JUDAS real. */
static long port_wav_find_chunk(const unsigned char *buf, long len, long pos, const char *tag, unsigned *chunklen)
{
    while (pos + 8 <= len) {
        unsigned clen;
        memcpy(&clen, buf + pos + 4, 4);
        if (memcmp(buf + pos, tag, 4) == 0) {
            *chunklen = clen;
            return pos + 8;
        }
        pos += 8 + (long)clen + (clen & 1); /* los chunks RIFF van alineados a 2 bytes */
    }
    return -1;
}

SoundInfo *judas_loadwav_mem_len(const unsigned char *buf, long len)
{
    SoundInfo *SI;
    long fmt_pos, data_pos;
    unsigned fmt_len = 0, data_len = 0;
    unsigned short format, channels, bits;
    unsigned freq;
    SAMPLE *smp;
    long mono_len;
    int con;

    SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
    if (!SI)
        return NULL;
    SI->sample = NULL;

    if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0)
        return port_wav_fail(SI, JUDAS_WRONG_FORMAT);

    fmt_pos = port_wav_find_chunk(buf, len, 12, "fmt ", &fmt_len);
    if (fmt_pos < 0 || fmt_pos + 16 > len)
        return port_wav_fail(SI, JUDAS_WRONG_FORMAT);

    memcpy(&format, buf + fmt_pos + 0, 2);
    memcpy(&channels, buf + fmt_pos + 2, 2);
    memcpy(&freq, buf + fmt_pos + 4, 4);
    memcpy(&bits, buf + fmt_pos + 14, 2);
    if (format != 1 || channels == 0 || (bits != 8 && bits != 16))
        return port_wav_fail(SI, JUDAS_WRONG_FORMAT);

    data_pos = port_wav_find_chunk(buf, len, fmt_pos + fmt_len + (fmt_len & 1), "data", &data_len);
    if (data_pos < 0 || data_pos + (long)data_len > len)
        return port_wav_fail(SI, JUDAS_WRONG_FORMAT);

    /* Normaliza a mono 16 bits, igual que el JUDAS real. */
    mono_len = (long)(data_len / channels / (bits / 8));
    smp = judas_allocsample((int)(mono_len * 2));
    if (!smp)
        return port_wav_fail(SI, judas_error);

    {
        const unsigned char *src = buf + data_pos;
        short *dst = (short *)smp->start;
        if (bits == 16) {
            const short *src16 = (const short *)src;
            for (con = 0; con < mono_len; con++) {
                long acc = 0;
                unsigned ch;
                for (ch = 0; ch < channels; ch++)
                    acc += src16[con * channels + ch];
                dst[con] = (short)(acc / channels);
            }
        } else {
            for (con = 0; con < mono_len; con++) {
                long acc = 0;
                unsigned ch;
                for (ch = 0; ch < channels; ch++)
                    acc += (int)src[con * channels + ch] - 128;
                dst[con] = (short)((acc / (long)channels) * 256);
            }
        }
    }
    smp->repeat = smp->start;
    smp->end = smp->start + mono_len * 2;
    smp->voicemode = VM_ON | VM_16BIT;

    SI->SoundFreq = (int)freq;
    SI->SoundBits = 16;
    SI->SoundSize = (int)mono_len;
    SI->SoundData = (short *)smp->start;
    SI->sample = smp;

    judas_error = JUDAS_OK;
    return SI;
}

SoundInfo *judas_loadwav_mem(unsigned char *FileBuffer)
{
    /* PORT: la API real no recibe longitud -- confia en los campos de
     * longitud del propio RIFF. Con datos ya en memoria (siempre el caso
     * real: el llamador ya leyo el fichero entero a un buffer antes de
     * llamar), basta con darle un limite generoso y dejar que el parser de
     * chunks se detenga solo al encontrar "data". 64 MB cubre cualquier
     * .wav real que vaya a cargar el editor. */
    return judas_loadwav_mem_len(FileBuffer, 64L * 1024 * 1024);
}

SoundInfo *judas_loadwav(char *name)
{
    FILE *f;
    unsigned char *buf;
    long len;
    SoundInfo *SI;

    f = fopen(name, "rb");
    if (!f) {
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_OPEN_ERROR;
        return SI;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_READ_ERROR;
        return SI;
    }
    buf = (unsigned char *)malloc((size_t)len);
    if (!buf) {
        fclose(f);
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_OUT_OF_MEMORY;
        return SI;
    }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_READ_ERROR;
        return SI;
    }
    fclose(f);

    SI = judas_loadwav_mem_len(buf, len);
    free(buf);
    return SI;
}

/* ------------------------------------------------------------------ */
/*  RAW: 8 bits sin signo, mono, 11025 Hz fijo -- misma convencion que    */
/*  el JUDAS real (3rdparty/judas/judasraw.c, consultado como referencia) */
/* ------------------------------------------------------------------ */

#define PORT_RAW_FREQ 11025

static SoundInfo *port_raw_from_mem(const unsigned char *data, long length, int repeat, int end, unsigned char voicemode)
{
    SoundInfo *SI;
    SAMPLE *smp;
    short *dst;
    long con;

    SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
    if (!SI)
        return NULL;
    SI->sample = NULL;

    if (!data || length <= 0)
        return port_wav_fail(SI, JUDAS_READ_ERROR);

    smp = judas_allocsample((int)(length * 2));
    if (!smp)
        return port_wav_fail(SI, judas_error);

    dst = (short *)smp->start;
    for (con = 0; con < length; con++)
        dst[con] = (short)(((int)data[con] - 128) * 256);

    if (end == 0 || end > length * 2)
        end = (int)(length * 2);
    if (repeat > length * 2 - 1)
        repeat = (int)(length * 2 - 1);
    smp->repeat = smp->start + repeat;
    smp->end = smp->start + end;
    smp->voicemode = voicemode | VM_ON | VM_16BIT;

    SI->SoundFreq = PORT_RAW_FREQ;
    SI->SoundBits = 8;
    SI->SoundSize = (int)length;
    SI->SoundData = (short *)smp->start;
    SI->sample = smp;

    judas_error = JUDAS_OK;
    return SI;
}

SoundInfo *judas_loadrawsample_mem(char *FileBuffer, int length, int repeat, int end, unsigned char voicemode)
{
    return port_raw_from_mem((const unsigned char *)FileBuffer, length, repeat, end, voicemode);
}

SoundInfo *judas_loadrawsample(char *name, int repeat, int end, unsigned char voicemode)
{
    FILE *f;
    unsigned char *buf;
    long len;
    SoundInfo *SI;

    f = fopen(name, "rb");
    if (!f) {
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_OPEN_ERROR;
        return SI;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = JUDAS_READ_ERROR;
        return SI;
    }
    buf = (unsigned char *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        SI = (SoundInfo *)calloc(1, sizeof(SoundInfo));
        if (SI)
            judas_error = buf ? JUDAS_READ_ERROR : JUDAS_OUT_OF_MEMORY;
        return SI;
    }
    fclose(f);

    SI = port_raw_from_mem(buf, len, repeat, end, voicemode);
    free(buf);
    return SI;
}

/* ------------------------------------------------------------------ */
/*  Tracker (XM/S3M/MOD): un unico "modulo residente", igual que          */
/*  port/io/io_song.c y divsound.cpp (runtime) -- ver comentario de       */
/*  cabecera de io_song.c                                                */
/* ------------------------------------------------------------------ */

static unsigned char *port_song_data = NULL;
static long port_song_len = 0;

static int port_song_load_file(const char *name)
{
    FILE *f;
    long len;
    unsigned char *buf;

    f = fopen(name, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return -1;
    }
    buf = (unsigned char *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return -1;
    }
    fclose(f);

    if (io_load_song(buf, (int)len, 0) != 0) {
        free(buf);
        return -1;
    }

    free(port_song_data);
    port_song_data = buf;
    port_song_len = len;
    return 0;
}

static void port_song_free(void)
{
    io_stop_song();
    free(port_song_data);
    port_song_data = NULL;
    port_song_len = 0;
}

/* PORT: los tres formatos comparten la misma implementacion -- libmikmod
 * (io_load_song/io_play_song) autodetecta el formato real de verdad, asi
 * que no hace falta (ni se puede, sin portar XM/S3M/MOD por separado)
 * distinguirlos de verdad. divpcm.cpp los prueba en orden (xm, s3m, mod)
 * hasta que uno acepte el fichero: aqui los tres aceptan cualquier modulo
 * valido, así que el primero que se pruebe (normalmente judas_loadxm) ya
 * tiene exito con un .mod o .s3m real -- SongType puede no coincidir con
 * el formato real, pero la reproduccion es correcta (ver comentario de
 * cabecera del fichero). */
int judas_loadxm(char *name) { return (port_song_load_file(name) == 0) ? (judas_error = JUDAS_OK, 1) : (judas_error = JUDAS_WRONG_FORMAT, 0); }
int judas_loads3m(char *name) { return judas_loadxm(name); }
int judas_loadmod(char *name) { return judas_loadxm(name); }

void judas_freexm(void) { port_song_free(); }
void judas_frees3m(void) { port_song_free(); }
void judas_freemod(void) { port_song_free(); }

void judas_playxm(int rounds) { (void)rounds; if (port_song_data) io_play_song(port_song_data, (int)port_song_len, 1); }
void judas_plays3m(int rounds) { judas_playxm(rounds); }
void judas_playmod(int rounds) { judas_playxm(rounds); }

unsigned char judas_getxmpos(void) { int p = io_get_song_pos(); return (unsigned char)(p < 0 ? 0 : p); }
unsigned char judas_gets3mpos(void) { return judas_getxmpos(); }
unsigned char judas_getmodpos(void) { return judas_getxmpos(); }

unsigned char judas_getxmline(void) { int l = io_get_song_line(); return (unsigned char)(l < 0 ? 0 : l); }
unsigned char judas_gets3mline(void) { return judas_getxmline(); }
unsigned char judas_getmodline(void) { return judas_getxmline(); }

unsigned char judas_getxmchannels(void) { return (unsigned char)io_song_channels(); }
unsigned char judas_gets3mchannels(void) { return (unsigned char)io_song_channels(); }
unsigned char judas_getmodchannels(void) { return (unsigned char)io_song_channels(); }
