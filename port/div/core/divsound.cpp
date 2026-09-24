
//�����������������������������������������������������������������������������
//      Sonido
//�����������������������������������������������������������������������������

/*
 * PORT: reescrito (mismo criterio que v.cpp/mouse.cpp/divkeybo.cpp -- este
 * modulo tambien es un "backend" que dependia de una libreria de terceros
 * con acceso directo a hardware: el original (src/div32run/divsound.cpp,
 * 486 lineas) implementaba TODA la carga/reproduccion llamando a JUDAS
 * (judas_loadwav_mem/judas_playsample/judas_loadxm_mem/etc, biblioteca de
 * sonido con acceso directo a Sound Blaster/DMA -- ver
 * docs/architecture/08-aspectos-tecnicos.md). Sustituido aqui por
 * port/io (io_audio.c), que ya expone una API 1:1 con divsound.h (ver el
 * propio div_io.h: "Audio: API 1:1 con divsound.h").
 *
 * Efectos de sonido (LoadSound/PlaySound/StopSound/ChangeSound/
 * ChangeChannel/IsPlayingSound) funcionan via raylib.
 *
 * Musica de tracker (MOD/S3M/XM): LoadSong/PlaySong/StopSong/UnloadSong/
 * SetSongPos/GetSongPos/GetSongLine/IsPlayingSong funcionan via libmikmod
 * vendored (3rdparty/mikmod, LGPL), driver winmm (waveOut). El flujo replica
 * el del original JUDAS: LoadSong solo valida el formato y guarda una copia
 * del fichero en cancion[].ptr (con su longitud en cancion[].Len, campo
 * nuevo del port); PlaySong detiene lo que suene, (re)analiza esa copia y la
 * arranca; solo hay UNA cancion residente extendida (la que suena). Ver
 * docs/architecture/11-port-windows11-mikedx.md §4.2 y el subpunto de
 * integracion de libmikmod en 03-port-progreso.md.
 *
 * Conviven dos dispositivos de audio: raylib reproducen los efectos y winmm
 * la musica; Windows los mezcla a nivel de sistema.
 */

#include "inter.h"
#include "divsound.h"
#include "div_io.h"

tSonido  sonido[128];
tCancion cancion[128];

/* PORT: declarada extern en judas.h (SDK real, sin tocar) pero sin
 * almacenamiento real en ningun sitio porque no usamos JUDAS. Solo la
 * consulta i.cpp (if(judascfg_device!=DEV_NOSOUND) set_init_mixer()) y
 * esta misma InitSound(); en DEV_NOSOUND permanente, esa rama nunca se
 * ejecuta -- set_init_mixer() (port_stubs.c) nunca hace falta llamarlo
 * de verdad. */
unsigned judascfg_device=0;

int MusicChannels=0;
int *NewSound;
int ChannelCon=0;

void InitSound(void) {
  int con;

  io_audio_init();
  judascfg_device=DEV_NOSOUND; /* PORT: no hay hardware real que configurar/detectar */

  NewSound=mem+end_struct+32; /* igual que el original: apunta al bloque channel(0..31) dentro de mem[] */

  for (con=0;con<128;con++) { sonido[con].smp=NULL;  sonido[con].freq=0; }
  for (con=0;con<128;con++) { cancion[con].ptr=NULL; cancion[con].Len=0; cancion[con].loop=0; cancion[con].SongType=0; }
  for (con=0;con<32;con++)  channel(con)=0;

  MusicChannels=0;
}

void ResetSound(void) {
  int con;
  for (con=0;con<32;con++) StopSound(con);
  for (con=0;con<32;con++) channel(con)=0;
  MusicChannels=0;
}

int LoadSound(char *ptr, long Len, int Loop) {
  return io_load_sound(ptr, Len, Loop);
}

int UnloadSound(int NumSonido) {
  io_unload_sound(NumSonido);
  return 1;
}

int PlaySound(int NumSonido, int Volumen, int Frec) { // Vol y Frec (0..256)
  return io_play_sound(NumSonido, Volumen, Frec);
}

int StopSound(int NumChannel) {
  io_stop_sound(NumChannel);
  return 1;
}

int ChangeSound(int NumChannel, int Volumen, int Frec) {
  io_change_sound(NumChannel, Volumen, Frec);
  return 1;
}

int ChangeChannel(int NumChannel, int Volumen, int Panning) {
  io_change_channel(NumChannel, Volumen, Panning);
  return 1;
}

int IsPlayingSound(int NumChannel) {
  return io_is_playing_sound(NumChannel);
}

//�����������������������������������������������������������������������������
//      Musica de tracker (MOD/S3M/XM) via libmikmod (ver cabecera)
//�����������������������������������������������������������������������������

int LoadSong(char *ptr, int Len, int Loop) {
  int con=0;

  while(con<128 && cancion[con].ptr!=NULL) con++;
  if(con==128) return(-1);

  if(io_load_song(ptr, Len, Loop)<0) return(-1);
  if((cancion[con].ptr=(char*)malloc(Len))==NULL) return(-1);

  memcpy(cancion[con].ptr, ptr, Len);
  cancion[con].Len=Len;
  cancion[con].loop=Loop;

  return(con);
}

int PlaySong(int NumSong) {
  if(NumSong>127 || !cancion[NumSong].ptr) return(-1);

  StopSong();

  if(io_play_song(cancion[NumSong].ptr, cancion[NumSong].Len, cancion[NumSong].loop)<0)
    return(-1);

  MusicChannels=io_song_channels();

  return(1);
}

void StopSong(void) {
  io_stop_song();
  MusicChannels=0;
}

void UnloadSong(int NumSong) {
  if(NumSong>127 || !cancion[NumSong].ptr) return;

  free(cancion[NumSong].ptr);
  cancion[NumSong].ptr=NULL;
  cancion[NumSong].Len=0;
}

void SetSongPos(int SongPat) {
  io_set_song_pos(SongPat);
}

int GetSongPos(void) {
  return io_get_song_pos();
}

int GetSongLine(void) {
  return io_get_song_line();
}

int IsPlayingSong(void) {
  return io_is_playing_song();
}

void EndSound(void) {
  io_audio_close();
}
