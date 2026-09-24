/*
 * PORT: original en src/div32run/det_vesa.cpp. Se conserva integra la tabla
 * fija de 6 modos nativos de DIV (320x200, 320x240, 320x400, 360x240,
 * 360x360, 376x282) -- eso es pura logica, sin hardware. Se elimina el
 * sondeo real de modos VESA/SVGA adicionales via BIOS (vbeInit/
 * vbeGetModeInfo/VbeInfoBlock/ModeInfoBlock, de "vesa.h" + vesa.asm), que
 * no aplica a este port: port/io puede renderizar a cualquier resolucion
 * logica que el juego pida, no hace falta enumerar hardware real. Esto
 * coincide exactamente con lo que el propio original hacia cuando
 * vbeInit()!=0 (sin VESA disponible): se queda solo con los 6 modos fijos
 * y VersionVesa=0. Ver docs/architecture/12-port-progreso.md.
 */

#include "inter.h"

//�����������������������������������������������������������������������������
//  Detecci�n de los modos de video (fijos; sin sondeo VESA real)
//�����������������������������������������������������������������������������

void detectar_vesa(void) {
  num_video_modes=6;
  video_modes[0].ancho=320; video_modes[0].alto=200; video_modes[0].modo=320200;
  video_modes[1].ancho=320; video_modes[1].alto=240; video_modes[1].modo=320240;
  video_modes[2].ancho=320; video_modes[2].alto=400; video_modes[2].modo=320400;
  video_modes[3].ancho=360; video_modes[3].alto=240; video_modes[3].modo=360240;
  video_modes[4].ancho=360; video_modes[4].alto=360; video_modes[4].modo=360360;
  video_modes[5].ancho=376; video_modes[5].alto=282; video_modes[5].modo=376282;

  VersionVesa=0;
}
