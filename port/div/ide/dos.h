// stub for DOS/Watcom <dos.h>
#ifndef DOS_H
#define DOS_H

#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_VOLID  0x08
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20

struct find_t {
    char reserved[21];
    char attrib;
    unsigned short wr_time;
    unsigned short wr_date;
    unsigned long  size;
    char name[256];
};

unsigned _dos_findfirst(const char *name, unsigned attr, struct find_t *info);
unsigned _dos_findnext(struct find_t *info);

int getdisk(void);
int setdisk(int drive);

#endif
