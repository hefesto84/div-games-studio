#include "div_io.h"

#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define IO_MAX_SOUNDS 128
#define IO_CHANNELS 32
#define IO_SOUND_START_CHANNEL 16
#define IO_RAW_SAMPLE_RATE 22050

typedef struct {
    Wave wave;
    int freq;
    bool used;
} io_sound_t;

typedef struct {
    Sound snd;
    int handle;
    int orig_freq;
} io_channel_t;

static io_sound_t io_sounds[IO_MAX_SOUNDS];
static io_channel_t io_chans[IO_CHANNELS];
static int io_next_channel = 0;
static bool io_audio_ok = false;

static int alloc_channel(void)
{
    int init = IO_SOUND_START_CHANNEL;
    int con = init;
    while (con < IO_CHANNELS && io_is_playing_sound(con))
        con++;
    if (con == IO_CHANNELS) {
        con = init + io_next_channel;
        io_next_channel++;
        if (init + io_next_channel >= IO_CHANNELS)
            io_next_channel = 0;
        if (con >= IO_CHANNELS)
            con = init;
    }
    io_stop_sound(con);
    return con;
}

static float io_pitch(float frec)
{
    float p = frec / 256.0f;
    if (p < 0.01f)
        p = 0.01f;
    return p;
}

static float io_vol(float volume)
{
    float v = volume / 256.0f;
    if (v < 0.0f)
        v = 0.0f;
    if (v > 1.0f)
        v = 1.0f;
    return v;
}

int io_audio_init(void)
{
    InitAudioDevice();
    io_audio_ok = IsAudioDeviceReady();
    memset(io_sounds, 0, sizeof(io_sounds));
    memset(io_chans, 0, sizeof(io_chans));
    for (int i = 0; i < IO_CHANNELS; i++)
        io_chans[i].handle = -1;

    /* Musica de tracker (MOD/S3M/XM): libmikmod en su propio TU (io_song.c,
     * para no juntar raylib.h y windows.h). Compite a nivel de sistema con
     * el dispositivo de raylib; Windows los mezcla. */
    io_song_init();

    return io_audio_ok ? 0 : -1;
}

void io_audio_close(void)
{
    io_song_close();
    if (!io_audio_ok)
        return;
    for (int i = 0; i < IO_CHANNELS; i++)
        io_stop_sound(i);
    for (int i = 0; i < IO_MAX_SOUNDS; i++)
        if (io_sounds[i].used) {
            UnloadWave(io_sounds[i].wave);
            io_sounds[i].used = false;
        }
    CloseAudioDevice();
    io_audio_ok = false;
}

static int raw_pcm_to_wave(void *data, long len, Wave *out)
{
    uint8_t header[44];
    uint32_t datalen = (uint32_t)len;
    uint32_t buffersize = 44 + datalen;

    memcpy(header + 0, "RIFF", 4);
    *(uint32_t *)(header + 4) = 36 + datalen;
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    *(uint32_t *)(header + 16) = 16;
    *(uint16_t *)(header + 20) = 1;
    *(uint16_t *)(header + 22) = 1;
    *(uint32_t *)(header + 24) = IO_RAW_SAMPLE_RATE;
    *(uint32_t *)(header + 28) = IO_RAW_SAMPLE_RATE;
    *(uint16_t *)(header + 32) = 1;
    *(uint16_t *)(header + 34) = 8;
    memcpy(header + 36, "data", 4);
    *(uint32_t *)(header + 40) = datalen;

    uint8_t *buf = (uint8_t *)malloc(buffersize);
    if (!buf)
        return -1;
    memcpy(buf, header, 44);
    memcpy(buf + 44, data, datalen);

    Wave w = LoadWaveFromMemory(".wav", buf, buffersize);
    free(buf);
    if (w.data == NULL)
        return -1;
    *out = w;
    return 0;
}

int io_load_sound(void *data, long len, int loop)
{
    (void)loop;
    if (!io_audio_ok)
        return -1;

    int handle = 0;
    while (handle < IO_MAX_SOUNDS && io_sounds[handle].used)
        handle++;
    if (handle == IO_MAX_SOUNDS)
        return -1;

    Wave w = LoadWaveFromMemory(".wav", data, len);
    if (w.data == NULL) {
        if (raw_pcm_to_wave(data, len, &w) != 0)
            return -1;
    }

    io_sounds[handle].wave = w;
    io_sounds[handle].freq = w.sampleRate;
    io_sounds[handle].used = true;

    return handle;
}

int io_unload_sound(int handle)
{
    if (handle < 0 || handle >= IO_MAX_SOUNDS || !io_sounds[handle].used)
        return -1;
    for (int c = 0; c < IO_CHANNELS; c++)
        if (io_chans[c].handle == handle)
            io_stop_sound(c);
    UnloadWave(io_sounds[handle].wave);
    io_sounds[handle].used = false;
    return 1;
}

int io_play_sound(int handle, int volume, int pitch)
{
    if (!io_audio_ok || handle < 0 || handle >= IO_MAX_SOUNDS || !io_sounds[handle].used)
        return -1;

    int con = alloc_channel();
    if (con < 0)
        return -1;

    Sound snd = LoadSoundFromWave(io_sounds[handle].wave);

    if (pitch <= 0) {
        SetSoundVolume(snd, 0.0f);
    } else {
        SetSoundVolume(snd, io_vol((float)volume));
        SetSoundPitch(snd, io_pitch((float)pitch));
    }
    PlaySound(snd);

    io_chans[con].snd = snd;
    io_chans[con].handle = handle;
    io_chans[con].orig_freq = io_sounds[handle].freq;

    return con;
}

int io_stop_sound(int channel)
{
    if (channel < 0 || channel >= IO_CHANNELS)
        return -1;
    if (io_chans[channel].handle >= 0) {
        StopSound(io_chans[channel].snd);
        UnloadSound(io_chans[channel].snd);
        io_chans[channel].snd = (Sound){ 0 };
        io_chans[channel].handle = -1;
        io_chans[channel].orig_freq = 0;
    }
    return 1;
}

int io_change_sound(int channel, int volume, int pitch)
{
    if (channel < 0 || channel >= IO_CHANNELS || io_chans[channel].handle < 0)
        return -1;
    SetSoundVolume(io_chans[channel].snd, io_vol((float)volume));
    SetSoundPitch(io_chans[channel].snd, io_pitch((float)pitch));
    return 1;
}

int io_change_channel(int channel, int volume, int panning)
{
    if (channel < 0 || channel >= IO_CHANNELS || io_chans[channel].handle < 0)
        return -1;
    SetSoundVolume(io_chans[channel].snd, io_vol((float)volume));
    float pan = (panning < 0) ? 0.0f : (panning > 255 ? 1.0f : panning / 255.0f);
    SetSoundPan(io_chans[channel].snd, pan);
    return 1;
}

int io_is_playing_sound(int channel)
{
    if (channel < 0 || channel >= IO_CHANNELS)
        return 0;
    if (io_chans[channel].handle < 0)
        return 0;
    return IsSoundPlaying(io_chans[channel].snd) ? 1 : 0;
}