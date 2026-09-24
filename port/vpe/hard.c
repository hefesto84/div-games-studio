/* PORT: copiado de src/vpe/hard.cpp. MemAlloc/MemRealloc/MemZAlloc/
 * MemFreeAll son C puro, sin cambios. FatalError pierde las llamadas DOS
 * (rvmode/kbdReset/ShutGraph via modo VGA real) -- el port no conmuta modos
 * de video BIOS, así que basta con parar el motor VPE y salir. FixMul/
 * FixDiv reemplazan el "#pragma aux" de hard.h (MSVC lo ignora en silencio,
 * igual que mul_24/mul_16 en port_stubs.c): mismo formato de punto fijo
 * 16.16 que usaba el ensamblador original (imul/shrd de 32x32->64 bits). */
#include "internal.h"
#include "inter.h"

int num_blocks;
void *mem_blocks[8192];

void *MemAlloc(long int size)
{
  void *p;

  p=malloc(size);
  if (p==NULL)
    FatalError(ER_MEMORY,NULL);
  else {
    mem_blocks[num_blocks]=p;
  }

  num_blocks++;
  return(p);
}

void *MemRealloc(int *pointer, int *old_size, int size)
{
  int i;

  if (pointer==NULL)
    pointer=(int *)MemAlloc(size);
  else if (size<*old_size)
    return(pointer);
  else {
    for (i=0;i<num_blocks;i++) {
      if (mem_blocks[i]==pointer) {
        free(pointer);
        pointer=(int *)MemAlloc(size);
        mem_blocks[i]=pointer;
        *old_size=size;
        return(pointer);
      }
    }
  }

  return(pointer);
}

void *MemZAlloc(long int size)
{
  void *p;
  p=MemAlloc(size);
  if (p) memset(p,0,size);
  return(p);
}

void MemFreeAll(void)
{
  int i;

  for (i=0;i<num_blocks;i++) {
    if (mem_blocks[i]!=NULL)
      free(mem_blocks[i]);
  }

  num_blocks=0;
}

void FatalError(int type, char *msg)
{
  (void)type;
  (void)msg;
  e(100);

  VPE_Stop();
  VPE_Shut();
  ShutGraph();

  exit(26);
}

FIXED FixMul(FIXED a, FIXED b)
{
  return (FIXED)(((long long)a*(long long)b)>>16);
}

FIXED FixDiv(FIXED a, FIXED b)
{
  return (FIXED)(((long long)a<<16)/b);
}
