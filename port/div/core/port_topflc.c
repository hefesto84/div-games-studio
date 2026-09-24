/*
 * port_topflc.c -- decodificador FLI/FLC nuevo, implementando la API
 * publica de TopFLC v1.0 (Johannes Lehtinen, 1996) que src/div32run/
 * divfli.cpp (copiado sin tocar a port/div/core/divfli.cpp) ya espera.
 *
 * TopFLC (3rdparty/topflc/) solo tiene vendored el header (topflc.h) y la
 * documentacion (topflc.txt) -- el codigo fuente de la libreria
 * (tfframe.c/tfanimat.c/tfbuffer.c/tflib.c) nunca se vendoreo en este
 * repositorio. En vez de perseguir esa libreria de 1996, este fichero
 * implementa solo las funciones que divfli.cpp llama de verdad
 * (TFErrorHandler_Set, TFAnimation_NewFile, TFAnimation_Delete,
 * TFAnimation_GetInfo, TFAnimation_SetLooping, TFAnimation_SetPaletteFunction,
 * TFBuffers_Set, TFFrame_Decode), leyendo el formato FLI/FLC directamente
 * contra la especificacion publica del formato (Autodesk Animator / Animator
 * Pro, documentada extensamente por terceros -- p.ej. la spec clasica de
 * John Bridges/"flifmt.txt" y las implementaciones libres de referencia
 * como ffmpeg/libavcodec/flicvideo.c, consultadas de memoria para verificar
 * los algoritmos exactos de cada chunk).
 *
 * Alcance deliberado (suficiente para "reproducir un FLI/FLC real dentro de
 * un programa DIV", no una reimplementacion completa de TopFLC):
 *  - TFAnimation_NewHandle/NewMem, TFAnimation_DeleteAll, TFBuffers_Alloc/
 *    Free, TFFrame_Reset/Seek: NO implementadas (divfli.cpp no las llama).
 *  - Looping: divfli.cpp NO usa el mecanismo de "ring frame" del formato
 *    FLI/FLC para bucle -- ResetFli() (divfli.cpp) simplemente cierra y
 *    reabre el fichero entero cuando el juego DIV quiere repetir la
 *    animacion. TFAnimation_SetLooping() aqui solo guarda el flag, sin
 *    logica de ring-frame (no hace falta).
 *  - Chunks soportados: FLI_COLOR256, FLI_COLOR64 (paleta), FLI_BRUN (frame
 *    completo, primer frame tipico), FLI_COPY (frame completo sin
 *    comprimir), FLI_BLACK (frame completo a negro), FLI_LC (delta por
 *    linea, formato FLI original), FLI_SS2 (delta por linea orientado a
 *    palabras de 16 bits, formato FLC/Animator Pro), FLI_PSTAMP (thumbnail,
 *    se salta sin decodificar -- no se usa para reproduccion).
 *  - Paleta: FLI_COLOR64 ya trae valores VGA de 6 bits (0..63), igual que
 *    `paleta[768]` del propio DIV (ver v.cpp: `paleta[x]*4` al presentar);
 *    FLI_COLOR256 trae valores de 8 bits (0..255) y se reescalan a 6 bits
 *    (`>>2`) antes de guardarlos, para que ambos tipos de chunk acaben en
 *    la misma convencion que espera el resto del motor.
 */

#include "topflc.h"

#include <string.h>

#define FLI_TYPE   0xAF11u
#define FLC_TYPE   0xAF12u
#define FRAME_TYPE 0xF1FAu

#define CHUNK_COLOR256 4
#define CHUNK_SS2      7
#define CHUNK_COLOR64  11
#define CHUNK_LC       12
#define CHUNK_BLACK    13
#define CHUNK_BRUN     15
#define CHUNK_COPY     16
#define CHUNK_PSTAMP   18

#define HEADER_SIZE 128

struct TFAnimation {
    FILE *f;
    unsigned short type;
    unsigned short width, height;
    unsigned short num_frames;
    unsigned short cur_frame;   /* indice (0-based) del proximo frame a decodificar */
    TFStatus loop;
    TFUByte *framebuf;          /* puesto por TFBuffers_Set, no es dueno */
    TFUByte (*palbuf)[256][3];  /* puesto por TFBuffers_Set, no es dueno */
    TFPaletteFunction palfunc;
};

static TFErrorHandler g_error_handler = NULL;

static void report_error(const char *msg)
{
    if (g_error_handler)
        g_error_handler((char *)msg);
}

/* ------------------------------------------------------------------ */
/*  Lectura de enteros little-endian desde un buffer en memoria         */
/* ------------------------------------------------------------------ */

static unsigned short rd16(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned long rd32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* ------------------------------------------------------------------ */
/*  Cabecera / apertura                                                */
/* ------------------------------------------------------------------ */

TFStatus TFErrorHandler_Set(TFErrorHandler handler)
{
    g_error_handler = handler;
    return TF_SUCCESS;
}

TFAnimation *TFAnimation_NewFile(char *name)
{
    unsigned char hdr[HEADER_SIZE];
    FILE *f;
    TFAnimation *a;
    unsigned short type;

    f = fopen(name, "rb");
    if (!f) {
        report_error("TopFLC: no se pudo abrir el fichero");
        return NULL;
    }
    if (fread(hdr, 1, HEADER_SIZE, f) != HEADER_SIZE) {
        report_error("TopFLC: fichero demasiado pequeno para ser FLI/FLC");
        fclose(f);
        return NULL;
    }

    type = rd16(hdr + 4);
    if (type != FLI_TYPE && type != FLC_TYPE) {
        report_error("TopFLC: no es un fichero FLI/FLC valido");
        fclose(f);
        return NULL;
    }

    a = (TFAnimation *)calloc(1, sizeof(TFAnimation));
    if (!a) {
        report_error("TopFLC: sin memoria");
        fclose(f);
        return NULL;
    }

    a->f = f;
    a->type = type;
    a->num_frames = rd16(hdr + 6);
    a->width = rd16(hdr + 8);
    a->height = rd16(hdr + 10);
    a->cur_frame = 0;
    a->loop = TF_FALSE;
    a->framebuf = NULL;
    a->palbuf = NULL;
    a->palfunc = NULL;

    /* El primer frame empieza siempre justo despues de la cabecera de 128
     * bytes, tanto en FLI como en FLC (el campo "oframe1" de la cabecera
     * FLC es redundante con esto en la practica para un fichero real). */
    fseek(f, HEADER_SIZE, SEEK_SET);

    return a;
}

TFAnimation *TFAnimation_NewHandle(FILE *handle)
{
    (void)handle;
    return NULL; /* no usado por divfli.cpp */
}

TFAnimation *TFAnimation_NewMem(void *raw)
{
    (void)raw;
    return NULL; /* no usado por divfli.cpp */
}

TFStatus TFAnimation_Delete(TFAnimation *animation)
{
    if (animation) {
        if (animation->f)
            fclose(animation->f);
        free(animation);
    }
    return TF_SUCCESS;
}

TFStatus TFAnimation_DeleteAll(void)
{
    return TF_SUCCESS; /* no usado por divfli.cpp */
}

TFStatus TFAnimation_GetInfo(TFAnimation *animation, TFAnimationInfo *info)
{
    if (!animation || !info)
        return TF_FAILURE;
    info->Width = (short)animation->width;
    info->Height = (short)animation->height;
    info->NumFrames = (short)animation->num_frames;
    info->CurFrame = (short)animation->cur_frame;
    info->Frame = animation->framebuf;
    info->Palette = animation->palbuf;
    info->LoopFlag = animation->loop;
    return TF_SUCCESS;
}

TFStatus TFAnimation_SetLooping(TFAnimation *animation, TFStatus state)
{
    if (!animation)
        return TF_FAILURE;
    animation->loop = state;
    return TF_SUCCESS;
}

TFStatus TFAnimation_SetPaletteFunction(TFAnimation *animation,
                                        TFPaletteFunction update_func)
{
    if (!animation)
        return TF_FAILURE;
    animation->palfunc = update_func;
    return TF_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  Buffers                                                             */
/* ------------------------------------------------------------------ */

TFStatus TFBuffers_Set(TFAnimation *animation, void *framebuffer,
                       void *palettebuffer)
{
    if (!animation)
        return TF_FAILURE;
    animation->framebuf = (TFUByte *)framebuffer;
    animation->palbuf = (TFUByte (*)[256][3])palettebuffer;
    return TF_SUCCESS;
}

TFStatus TFBuffers_Alloc(TFAnimation *animation)
{
    (void)animation;
    return TF_FAILURE; /* no usado por divfli.cpp */
}

TFStatus TFBuffers_Free(TFAnimation *animation)
{
    (void)animation;
    return TF_FAILURE; /* no usado por divfli.cpp */
}

/* ------------------------------------------------------------------ */
/*  Decodificacion de chunks                                            */
/* ------------------------------------------------------------------ */

/* PORT: FLI_COLOR256/FLI_COLOR64 -- lista de paquetes {skip, change,
 * colores...}. COLOR64 trae valores VGA de 6 bits (0..63, la convencion
 * nativa de `paleta[]`); COLOR256 trae 8 bits (0..255), reescalados aqui a
 * 6 bits para que ambos acaben en la misma unidad. */
static void decode_color(TFUByte pal[256][3], const unsigned char *data,
                          long len, int is256)
{
    long pos = 0;
    unsigned short packets, p;
    int idx = 0;

    if (len < 2)
        return;
    packets = rd16(data);
    pos = 2;

    for (p = 0; p < packets && pos + 2 <= len; p++) {
        unsigned char skip = data[pos++];
        unsigned char change = data[pos++];
        int count = change ? change : 256;
        int c;

        idx += skip;
        for (c = 0; c < count && idx < 256 && pos + 3 <= len; c++, idx++) {
            unsigned char r = data[pos++], g = data[pos++], b = data[pos++];
            if (is256) {
                r = (unsigned char)(r >> 2);
                g = (unsigned char)(g >> 2);
                b = (unsigned char)(b >> 2);
            }
            pal[idx][0] = r;
            pal[idx][1] = g;
            pal[idx][2] = b;
        }
    }
}

/* PORT: FLI_BRUN -- frame completo, comprimido por lineas (byte-run). Por
 * cada linea: 1 byte de "numero de paquetes" (se consume pero no hace
 * falta para decodificar, se avanza hasta cubrir el ancho) y luego
 * paquetes {n con signo}: n>0 -> repetir 1 byte n veces; n<0 -> copiar -n
 * bytes literales. */
static void decode_brun(TFUByte *frame, int width, int height,
                         const unsigned char *data, long len)
{
    long pos = 0;
    int y;

    for (y = 0; y < height && pos < len; y++) {
        TFUByte *row = frame + (long)y * width;
        int x = 0;
        pos++; /* byte de "packets count" de la linea, no se usa */
        while (x < width && pos < len) {
            signed char n = (signed char)data[pos++];
            if (n > 0) {
                unsigned char v = (pos < len) ? data[pos++] : 0;
                int run = n;
                while (run-- > 0 && x < width)
                    row[x++] = v;
            } else if (n < 0) {
                int run = -n;
                while (run-- > 0 && x < width && pos < len)
                    row[x++] = data[pos++];
            } else {
                break; /* paquete vacio: no deberia aparecer, evita bucle infinito */
            }
        }
    }
}

/* PORT: FLI_COPY -- frame completo sin comprimir, width*height bytes tal
 * cual. */
static void decode_copy(TFUByte *frame, int width, int height,
                         const unsigned char *data, long len)
{
    long n = (long)width * height;
    if (n > len)
        n = len;
    memcpy(frame, data, (size_t)n);
}

/* PORT: FLI_LC -- delta por linea (formato FLI original). Cabecera de 4
 * bytes (skip_lines, n_lines), luego por cada linea que cambia: 1 byte de
 * numero de paquetes y paquetes {skip, n con signo}: n>=0 -> copiar n
 * bytes literales; n<0 -> repetir 1 byte -n veces. Signo OPUESTO al de
 * FLI_BRUN -- ver comentario de cabecera del fichero. */
static void decode_lc(TFUByte *frame, int width,
                       const unsigned char *data, long len)
{
    long pos = 0;
    unsigned short skip_lines, n_lines, i;
    int y;

    if (len < 4)
        return;
    skip_lines = rd16(data);
    n_lines = rd16(data + 2);
    pos = 4;
    y = skip_lines;

    for (i = 0; i < n_lines && pos < len; i++, y++) {
        TFUByte *row = frame + (long)y * width;
        unsigned char packet_count = data[pos++];
        int x = 0, p;

        for (p = 0; p < packet_count && pos < len; p++) {
            unsigned char skip = data[pos++];
            signed char n;
            x += skip;
            if (pos >= len)
                break;
            n = (signed char)data[pos++];
            if (n >= 0) {
                int run = n;
                while (run-- > 0 && x < width && pos < len)
                    row[x++] = data[pos++];
            } else {
                int run = -n;
                unsigned char v = (pos < len) ? data[pos++] : 0;
                while (run-- > 0 && x < width)
                    row[x++] = v;
            }
        }
    }
}

/* PORT: FLI_SS2 -- delta por linea orientado a palabras de 16 bits
 * (formato FLC/Animator Pro). Cabecera de 2 bytes (n_lines). Por cada
 * "linea logica" se lee una palabra con signo:
 *   - bits altos "11" (word & 0xC000 == 0xC000): NO es una linea real, es
 *     "saltar -word lineas" (el valor es negativo); no consume el
 *     contador de lineas, se relee la palabra para la misma linea.
 *   - bit 15 puesto (y no es el caso anterior): el byte bajo es el ULTIMO
 *     pixel de la linea (se escribe directo); la palabra real de "numero
 *     de paquetes" viene justo despues.
 *   - si no, la palabra ES el numero de paquetes de la linea.
 * Cada paquete: {skip, n con signo}: n>=0 -> copiar n PALABRAS (2*n
 * bytes) literales; n<0 -> repetir 1 palabra (2 bytes) -n veces. */
static void decode_ss2(TFUByte *frame, int width,
                        const unsigned char *data, long len)
{
    long pos = 0;
    unsigned short num_lines;
    int y = 0;

    if (len < 2)
        return;
    num_lines = rd16(data);
    pos = 2;

    while (num_lines > 0 && pos + 2 <= len) {
        unsigned short raw = rd16(data + pos);
        signed short opcode = (signed short)raw;
        pos += 2;

        if ((raw & 0xC000u) == 0xC000u) {
            /* saltar lineas: opcode es negativo, -opcode lineas */
            y += (int)(-opcode);
            continue;
        }

        {
            TFUByte *row = frame + (long)y * width;
            unsigned short packet_count;
            int x = 0, p;

            if (raw & 0x8000u) {
                /* ultimo pixel de la linea viene en el byte bajo */
                if (width > 0)
                    row[width - 1] = (unsigned char)(raw & 0xFFu);
                if (pos + 2 > len)
                    break;
                packet_count = rd16(data + pos);
                pos += 2;
            } else {
                packet_count = raw;
            }

            for (p = 0; p < (int)packet_count && pos < len; p++) {
                unsigned char skip;
                signed char n;
                skip = data[pos++];
                x += skip;
                if (pos >= len)
                    break;
                n = (signed char)data[pos++];
                if (n >= 0) {
                    int run = n;
                    while (run-- > 0 && pos + 2 <= len && x + 1 < width) {
                        row[x++] = data[pos++];
                        row[x++] = data[pos++];
                    }
                } else {
                    int run = -n;
                    unsigned char v0, v1;
                    if (pos + 2 > len)
                        break;
                    v0 = data[pos++];
                    v1 = data[pos++];
                    while (run-- > 0 && x + 1 < width) {
                        row[x++] = v0;
                        row[x++] = v1;
                    }
                }
            }
        }
        y++;
        num_lines--;
    }
}

/* ------------------------------------------------------------------ */
/*  Frame                                                               */
/* ------------------------------------------------------------------ */

TFStatus TFFrame_Decode(TFAnimation *animation)
{
    unsigned char fhdr[16];
    unsigned long fsize;
    unsigned short ftype, nchunks, c;
    long frame_start, frame_end;
    int palette_changed = 0;

    if (!animation || !animation->f || !animation->framebuf)
        return TF_FAILURE;
    if (animation->cur_frame >= animation->num_frames)
        return TF_FAILURE;

    frame_start = ftell(animation->f);
    if (fread(fhdr, 1, 16, animation->f) != 16) {
        report_error("TopFLC: error leyendo cabecera de frame");
        return TF_FAILURE;
    }
    fsize = rd32(fhdr);
    ftype = rd16(fhdr + 4);
    nchunks = rd16(fhdr + 6);
    frame_end = frame_start + (long)fsize;

    if (ftype != FRAME_TYPE) {
        report_error("TopFLC: chunk de frame invalido");
        return TF_FAILURE;
    }

    for (c = 0; c < nchunks; c++) {
        unsigned char chdr[6];
        unsigned long csize;
        unsigned short ctype;
        long chunk_data_start, chunk_data_len;
        unsigned char *buf;

        if (fread(chdr, 1, 6, animation->f) != 6)
            break;
        csize = rd32(chdr);
        ctype = rd16(chdr + 4);
        chunk_data_start = ftell(animation->f);
        chunk_data_len = (long)csize - 6;
        if (chunk_data_len < 0)
            chunk_data_len = 0;

        buf = NULL;
        if (chunk_data_len > 0 &&
            (ctype == CHUNK_COLOR256 || ctype == CHUNK_COLOR64 ||
             ctype == CHUNK_BRUN || ctype == CHUNK_COPY ||
             ctype == CHUNK_LC || ctype == CHUNK_SS2)) {
            buf = (unsigned char *)malloc((size_t)chunk_data_len);
            if (buf)
                fread(buf, 1, (size_t)chunk_data_len, animation->f);
        }

        switch (ctype) {
        case CHUNK_COLOR256:
            if (buf && animation->palbuf) {
                decode_color(*animation->palbuf, buf, chunk_data_len, 1);
                palette_changed = 1;
            }
            break;
        case CHUNK_COLOR64:
            if (buf && animation->palbuf) {
                decode_color(*animation->palbuf, buf, chunk_data_len, 0);
                palette_changed = 1;
            }
            break;
        case CHUNK_BRUN:
            if (buf)
                decode_brun(animation->framebuf, animation->width,
                            animation->height, buf, chunk_data_len);
            break;
        case CHUNK_COPY:
            if (buf)
                decode_copy(animation->framebuf, animation->width,
                            animation->height, buf, chunk_data_len);
            break;
        case CHUNK_LC:
            if (buf)
                decode_lc(animation->framebuf, animation->width, buf,
                          chunk_data_len);
            break;
        case CHUNK_SS2:
            if (buf)
                decode_ss2(animation->framebuf, animation->width, buf,
                           chunk_data_len);
            break;
        case CHUNK_BLACK:
            memset(animation->framebuf, 0,
                   (size_t)animation->width * animation->height);
            break;
        case CHUNK_PSTAMP:
        default:
            break; /* desconocido o thumbnail: se ignora */
        }

        free(buf);
        fseek(animation->f, chunk_data_start + chunk_data_len, SEEK_SET);
    }

    /* Por si el tamano de frame declarado no cuadra exactamente con la
     * suma de sus chunks (no deberia pasar en un fichero real, pero
     * evita quedar desalineados para el siguiente frame). */
    fseek(animation->f, frame_end, SEEK_SET);

    animation->cur_frame++;

    if (palette_changed && animation->palfunc && animation->palbuf)
        animation->palfunc(animation->palbuf);

    return TF_SUCCESS;
}

TFStatus TFFrame_Reset(TFAnimation *animation)
{
    (void)animation;
    return TF_FAILURE; /* no usado por divfli.cpp */
}

TFStatus TFFrame_Seek(TFAnimation *animation, int frame)
{
    (void)animation;
    (void)frame;
    return TF_FAILURE; /* no usado por divfli.cpp */
}
