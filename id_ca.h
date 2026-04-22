#ifndef __ID_CA_H_
#define __ID_CA_H_

//===========================================================================

#define NUMMAPS         60
#define MAPPLANES       3

//===========================================================================

typedef struct
{
    word bit0,bit1;       // 0-255 is a character, > is a pointer to a node
} huffnode;


typedef struct
{
    word RLEWtag;
#if MAPPLANES >= 4
    word numplanes;       // unused, but WDC needs 2 bytes here for internal usage
#endif
    int32_t headeroffsets[NUMMAPS];
} mapfiletype;


typedef struct
{
    int32_t planestart[MAPPLANES];
    word    planelength[MAPPLANES];
    word    width,height;
    char    name[16];
} maptype;

//===========================================================================

extern  word    *mapsegs[MAPPLANES];
extern  maptype *mapheaderseg[NUMMAPS];
extern  byte    *audiosegs[NUMSNDCHUNKS];
extern  byte    *grsegs[NUMCHUNKS];

extern  char  extension[5];

//===========================================================================

int32_t CA_GetFileLength (FILE *file);

void CA_LoadFile (const char *filename, void **ptr);
void CA_WriteFile (const char *filename, void *ptr, int32_t length);

int32_t CA_RLEWCompress (word *source, int32_t length, word *dest, word rlewtag);

void CA_RLEWexpand (word *source, word *dest, int32_t length, word rlewtag);

void CA_Startup (void);
void CA_Shutdown (void);

void CA_CacheGrChunks (int32_t *offset, huffnode *hufftable, FILE *file);
void CA_CacheMap (int mapnum);

void CA_CannotOpen (const char *name);

#endif
