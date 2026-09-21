// ID_CA.C

// this has been customized for WOLF

/*
=============================================================================

Id Software Caching Manager

=============================================================================
*/

#include <sys/types.h>
#if defined PICOWOLF
    // No filesystem, so none of the headers that describe one.
#elif defined _WIN32
    #include <io.h>
#elif defined _arch_dreamcast
    #include <unistd.h>
#else
    #include <sys/uio.h>
    #include <unistd.h>
#endif

#include "wl_def.h"

#ifdef USE_FLASH_ASSETS
#include "wolf_assets.h"
#endif

#define THREEBYTEGRSTARTS

#ifdef THREEBYTEGRSTARTS
    #define GRSTARTSIZE     3
#else
    #define GRSTARTSIZE     4
#endif

/*
=============================================================================

                             GLOBAL VARIABLES

=============================================================================
*/

word     *mapsegs[MAPPLANES];
byte     *audiosegs[NUMSNDCHUNKS];
byte     *grsegs[NUMCHUNKS];

unsigned mapwidth,mapheight;

char mapname[MAPNAMESIZE + 1];
char extension[5]; // Need a string, not constant to change cache files

static const char gheadname[] = "vgahead.";
static const char gfilename[] = "vgagraph.";
static const char gdictname[] = "vgadict.";
static const char mheadname[] = "maphead.";
#ifdef CARMACIZED
static const char mfilename[] = "gamemaps.";
#else
static const char mfilename[] = "maptemp.";
#endif
static const char aheadname[] = "audiohed.";
static const char afilename[] = "audiot.";


/*
=============================================================================

                            LOW LEVEL ROUTINES

=============================================================================
*/

void CA_CannotOpen (const char *string)
{
    Quit ("Can't open %s: %s",string,strerror(errno));
}


/*
==========================
=
= CA_GetFileLength
=
==========================
*/

int32_t CA_GetFileLength (FILE *file)
{
    int32_t length;

    fseek (file,0,SEEK_END);
    length = ftell(file);
    fseek (file,0,SEEK_SET);

    return length;
}


/*
==========================
=
= CA_WriteFile
=
= Writes a file from a memory buffer
=
==========================
*/

void CA_WriteFile (const char *filename, void *ptr, int32_t length)
{
    FILE *file;

    file = fopen(filename,"wb");

    if (!file)
        CA_CannotOpen (filename);

    if (!fwrite(ptr,length,1,file))
        Quit ("Error writing file %s: %s",filename,strerror(errno));

    fclose (file);
}


/*
==========================
=
= CA_LoadFile
=
= Allocate space for and load a file
=
==========================
*/

void CA_LoadFile (const char *filename, void **ptr)
{
    FILE   *file;
    size_t size;

    file = fopen(filename,"rb");

    if (!file)
        CA_CannotOpen (filename);

    size = CA_GetFileLength(file);

    *ptr = SafeMalloc(size);

    if (!fread(*ptr,size,1,file))
        Quit ("Error reading file %s: %s",filename,strerror(errno));

    fclose (file);
}


/*
============================================================================

                COMPRESSION routines, see JHUFF.C for more

============================================================================
*/

static void CAL_HuffExpand(byte *source, byte *dest, int32_t length, huffnode *hufftable)
{
    byte *end;
    huffnode *headptr, *huffptr;

    if(!length || !dest)
    {
        Quit("length or dest is null!");
        return;
    }

    headptr = hufftable+254;        // head node is always node 254

    int written = 0;

    end=dest+length;

    byte val = *source++;
    byte mask = 1;
    word nodeval;
    huffptr = headptr;
    while(1)
    {
        if(!(val & mask))
            nodeval = huffptr->bit0;
        else
            nodeval = huffptr->bit1;
        if(mask==0x80)
        {
            val = *source++;
            mask = 1;
        }
        else mask <<= 1;

        if(nodeval<256)
        {
            *dest++ = (byte) nodeval;
            written++;
            huffptr = headptr;
            if(dest>=end) break;
        }
        else
        {
            huffptr = hufftable + (nodeval - 256);
        }
    }
}

/*
======================
=
= CAL_CarmackExpand
=
= Length is the length of the EXPANDED data
=
======================
*/

#define NEARTAG 0xa7
#define FARTAG  0xa8

void CAL_CarmackExpand (byte *source, word *dest, int length)
{
    word ch,chhigh,count,offset;
    byte *inptr;
    word *copyptr, *outptr;

    length/=2;

    inptr = (byte *) source;
    outptr = dest;

    while (length>0)
    {
        ch = ReadShort(inptr);
        inptr += 2;
        chhigh = ch>>8;
        if (chhigh == NEARTAG)
        {
            count = ch&0xff;
            if (!count)
            {                               // have to insert a word containing the tag byte
                ch |= *inptr++;
                *outptr++ = ch;
                length--;
            }
            else
            {
                offset = *inptr++;
                copyptr = outptr - offset;
                length -= count;
                if(length<0) return;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        }
        else if (chhigh == FARTAG)
        {
            count = ch&0xff;
            if (!count)
            {                               // have to insert a word containing the tag byte
                ch |= *inptr++;
                *outptr++ = ch;
                length --;
            }
            else
            {
                offset = ReadShort(inptr);
                inptr += 2;
                copyptr = dest + offset;
                length -= count;
                if(length<0) return;
                while (count--)
                    *outptr++ = *copyptr++;
            }
        }
        else
        {
            *outptr++ = ch;
            length --;
        }
    }
}

/*
======================
=
= CA_RLEWcompress
=
======================
*/

int32_t CA_RLEWCompress (word *source, int32_t length, word *dest, word rlewtag)
{
    word value,count;
    unsigned i;
    word *start,*end;

    start = dest;

    end = source + (length+1)/2;

    //
    // compress it
    //
    do
    {
        count = 1;
        value = *source++;
        while (*source == value && source<end)
        {
            count++;
            source++;
        }
        if (count>3 || value == rlewtag)
        {
            //
            // send a tag / count / value string
            //
            *dest++ = rlewtag;
            *dest++ = count;
            *dest++ = value;
        }
        else
        {
            //
            // send word without compressing
            //
            for (i=1;i<=count;i++)
                *dest++ = value;
        }

    } while (source<end);

    return (int32_t)(2*(dest-start));
}


/*
======================
=
= CA_RLEWexpand
= length is EXPANDED length
=
======================
*/

void CA_RLEWexpand (word *source, word *dest, int32_t length, word rlewtag)
{
    word value,count,i;
    word *end=dest+length/2;

//
// expand it
//
    do
    {
        value = *source++;
        if (value != rlewtag)
            //
            // uncompressed
            //
            *dest++=value;
        else
        {
            //
            // compressed string
            //
            count = *source++;
            value = *source++;
            for (i=1;i<=count;i++)
                *dest++ = value;
        }
    } while (dest<end);
}



/*
=============================================================================

                                         CACHE MANAGER ROUTINES

=============================================================================
*/


/*
======================
=
= CAL_SetupGrFile
=
======================
*/

#ifdef USE_FLASH_ASSETS

/*
= The chunks are already decoded and deplaned in flash, so there is no
= dictionary to read, no Huffman to run and no chunk to allocate: grsegs[]
= just points into the blob.  A sparse chunk has no span and stays NULL,
= which is what the callers already test for.
*/
void CAL_SetupGrFile (void)
{
    int i;

    for (i = 0; i < NUMCHUNKS; i++)
        grsegs[i] = wolf_grspans[i].offset < 0 ? NULL
                  : (byte *)(uintptr_t)(wolf_vgagraph + wolf_grspans[i].offset);

    pictable = (pictabletype *)(uintptr_t)wolf_pictable;
}

#else

void CAL_SetupGrFile (void)
{
    int      i;
    char     fname[13];
    huffnode grhuffman[255];
    FILE     *file;
    byte     *compseg;
    byte     b[GRSTARTSIZE];
    int      expectedsize;
    int32_t  headersize;
    int32_t  compressed,expanded;
    int32_t  *grstarts;

//
// load ???dict.ext (huffman dictionary for graphics files)
//
    snprintf (fname,sizeof(fname),"%s%s",gdictname,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    fread (grhuffman,sizeof(grhuffman),1,file);
    fclose (file);

//
// load the data offsets from ???head.ext
//
    snprintf (fname,sizeof(fname),"%s%s",gheadname,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    headersize = CA_GetFileLength(file);

	expectedsize = NUMCHUNKS + 1;

    if (!param_ignorenumchunks && headersize / GRSTARTSIZE != expectedsize)
        Quit ("Wolf4SDL was not compiled for these data files:\n"
              "%s contains a wrong number of offsets (%i instead of %i)!\n\n"
              "Please check whether you are using the right executable!\n"
              "(For mod developers: perhaps you forgot to update NUMCHUNKS?)",
              fname,headersize / GRSTARTSIZE,expectedsize);

    grstarts = SafeMalloc(expectedsize * sizeof(*grstarts));
#ifdef THREEBYTEGRSTARTS
    for (i = 0; i < expectedsize; i++)
    {
        fread (b,sizeof(b),1,file);

        grstarts[i] = b[0] | (b[1] << 8) | (b[2] << 16);

        if (grstarts[i] == 0x00ffffff)
            grstarts[i] = -1;
    }
#else
    fread (grstarts,expectedsize * sizeof(*grstarts),1,file);
#endif
    fclose (file);

//
// Open the graphics file
//
    snprintf (fname,sizeof(fname),"%s%s",gfilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

//
// load the pic headers
//
    expanded = NUMPICS * sizeof(*pictable);

    compressed = grstarts[STRUCTPIC + 1] - grstarts[STRUCTPIC] - sizeof(expanded);

    fseek (file,grstarts[STRUCTPIC] + sizeof(expanded),SEEK_SET);

    compseg = SafeMalloc(compressed);
    fread (compseg,compressed,1,file);

    pictable = SafeMalloc(expanded);

    CAL_HuffExpand (compseg,(byte *)pictable,expanded,grhuffman);

    SafeFree (compseg);

    CA_CacheGrChunks (grstarts,grhuffman,file);

    SafeFree (grstarts);

    fclose (file);
}

#endif  /* USE_FLASH_ASSETS */


/*
======================
=
= CAL_SetupMapFile
=
======================
*/

#ifdef USE_FLASH_ASSETS

/*
= One level's worth of planes, and the scratch the Carmack pass decompresses
= into before the RLEW pass reads it.  Static because there is no allocator on
= the target and because these are the only map buffers the game will ever
= need: it holds one level at a time.
=
= The scratch is sized for a plane that does not compress at all, plus the
= two-byte length that precedes the RLEW stream.  The largest this data set
= actually needs is 6,028 bytes.
*/
static word MapPlanes[MAPPLANES][MAPAREA];
static word MapScratch[MAPAREA + 1];

void CAL_SetupMapFile (void)
{
    int i;

    for (i = 0; i < MAPPLANES; i++)
        mapsegs[i] = MapPlanes[i];
}

#else

void CAL_SetupMapFile (void)
{
    int i;

//
// allocate space for all planes
//
    for (i = 0; i < MAPPLANES; i++)
        mapsegs[i] = SafeMalloc(MAPAREA * sizeof(*mapsegs[i]));
}

#endif


/*
======================
=
= CAL_SetupAudioFile
=
======================
*/

#ifdef USE_FLASH_ASSETS

/*
= audiosegs[] is what the sound manager indexes, and the converter already
= produced it - the AdLib sounds extended, the music given its four-byte
= length, the digitised entries left out.  So this is the same pointer walk
= the graphics do.
*/
void CAL_SetupAudioFile (void)
{
    int chunk;

    for (chunk = 0; chunk < NUMSNDCHUNKS; chunk++)
        audiosegs[chunk] = wolf_audiospans[chunk].offset < 0 ? NULL
                         : (byte *)(uintptr_t)(wolf_audiot + wolf_audiospans[chunk].offset);
}

#else

void CAL_SetupAudioFile (void)
{
    int     i,chunk;
    word    length;
    int32_t pos,size;
    int32_t *audiostarts;
    byte    *dest;
    char    fname[13];
    FILE    *file;

//
// load audiohed.ext (offsets for audio file)
//
    snprintf (fname,sizeof(fname),"%s%s",aheadname,extension);

    CA_LoadFile (fname,(void **)&audiostarts);

//
// open the data file
//
    snprintf (fname,sizeof(fname),"%s%s",afilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    for (chunk = 0; chunk < NUMSNDCHUNKS; chunk++)
    {
        pos = audiostarts[chunk];
        size = audiostarts[chunk + 1] - pos;

        fseek (file,pos,SEEK_SET);

        if (chunk >= STARTMUSIC)
        {
            //
            // the original format stores a 2 byte length at
            // the start of the chunk and 88 bytes of Muse
            // data at the end
            //
            // for old format: add 2 bytes to size and remove
            // the Muse fluff
            //
            // for new format: add 4 bytes to size and reset
            // file pointer
            //
            fread (&length,sizeof(length),1,file);

            if (length)
            {
                size += sizeof(length);
                size -= 88;
            }
            else
            {
                size += sizeof(size);

                fseek (file,pos,SEEK_SET);
            }

            audiosegs[chunk] = SafeMalloc(size);
            dest = audiosegs[chunk];

            //
            // write 4 byte length to start of chunk
            //
            for (i = 0; i < sizeof(size); i++)
                *dest++ = (byte)(size >> (i << 3));

            size -= sizeof(size);       // remove length from data read
        }
        else
        {
            if (chunk >= STARTDIGISOUNDS)
            {
                chunk = STARTMUSIC - 1;     // skip over unused digi sound starts
                continue;
            }
            else if (chunk >= STARTADLIBSOUNDS)
                size += sizeof(AdLibSound) - sizeof(((AdLibSound *)0)->data);

            audiosegs[chunk] = SafeMalloc(size);
            dest = audiosegs[chunk];
        }

        fread (dest,size,1,file);
    }

    SafeFree (audiostarts);

    fclose (file);
}

#endif  /* USE_FLASH_ASSETS */


/*
======================
=
= CA_Startup
=
= Open all files and load in headers
=
======================
*/

void CA_Startup (void)
{
    CAL_SetupMapFile ();
    CAL_SetupGrFile ();
    CAL_SetupAudioFile ();
}

//==========================================================================


/*
======================
=
= CA_Shutdown
=
= Closes all files
=
======================
*/

void CA_Shutdown (void)
{
#ifdef USE_FLASH_ASSETS
    //
    // Nothing here was allocated: every pointer is into flash, or into the
    // static plane buffers.
    //
#else
    int i;

    for (i = 0; i < NUMCHUNKS; i++)
        SafeFree (grsegs[i]);

    for (i = 0; i < MAPPLANES; i++)
        SafeFree (mapsegs[i]);

    for (i = 0; i < NUMSNDCHUNKS; i++)
        SafeFree (audiosegs[i]);

    SafeFree (pictable);
#endif
}


/*
======================
=
= CAL_ExpandGrChunk
=
= Does whatever is needed with a pointer to a compressed chunk
=
======================
*/

void CAL_ExpandGrChunk (int chunk, byte *source, huffnode *hufftable)
{
    int32_t expanded;

    //
    // expanded sizes of tile8s are implicit,
    // everything else has an explicit 4 byte size
    //
    if (chunk >= STARTTILE8 && chunk < STARTEXTERNS)
        expanded = 64*NUMTILE8;     // tile 8s are all in one chunk!
    else
    {
        expanded = ReadLong(source);
        source += sizeof(expanded);
    }

    //
    // allocate final space and decompress it
    //
    grsegs[chunk] = SafeMalloc(expanded);

    CAL_HuffExpand (source,grsegs[chunk],expanded,hufftable);
}


/*
======================
=
= CAL_DeplaneGrChunk
=
======================
*/

void CAL_DeplaneGrChunk (int chunk)
{
    int     i;
    int16_t width,height;

    if (chunk == STARTTILE8)
    {
        width = height = 8;

        for (i = 0; i < NUMTILE8; i++)
            VW_DePlaneVGA (grsegs[chunk] + (i * (width * height)),width,height);
    }
    else
    {
        width = pictable[chunk - STARTPICS].width;
        height = pictable[chunk - STARTPICS].height;

        VW_DePlaneVGA (grsegs[chunk],width,height);
    }
}


/*
======================
=
= CA_CacheGrChunks
=
= Load all graphics chunks into memory
=
======================
*/

void CA_CacheGrChunks (int32_t *offset, huffnode *hufftable, FILE *grfile)
{
    byte    *source = NULL;
    int32_t pos,compressed;
    int     chunk,next;

    for (chunk = STRUCTPIC + 1; chunk < NUMCHUNKS; chunk++)
    {
        if (grsegs[chunk])
            continue;                             // already in memory

        //
        // load the chunk into a buffer
        //
        pos = offset[chunk];

        if (pos < 0)                         // $FFFFFFFF start is a sparse tile
            continue;

        next = chunk + 1;

        while (offset[next] == -1)           // skip past any sparse tiles
            next++;

        compressed = offset[next] - pos;

        fseek (grfile,pos,SEEK_SET);

        source = SafeRealloc(source,compressed);
        fread (source,compressed,1,grfile);

        CAL_ExpandGrChunk (chunk,source,hufftable);

        if (chunk >= STARTPICS && chunk < STARTEXTERNS)
            CAL_DeplaneGrChunk (chunk);
    }

    SafeFree (source);
}


/*
======================
=
= CA_CacheMap
=
======================
*/

#ifdef USE_FLASH_ASSETS

/*
= The only thing still decompressed at run time.  Sixty levels decoded would
= be 1.4 MB; compressed they are 148 KB, so they stay as the file holds them
= and one level is expanded into MapPlanes when it is loaded.
*/
void CA_CacheMap (int mapnum)
{
    const wolflevel_t *level;
    const byte        *src;
    int32_t            expanded;
    int                i;

    if ((unsigned)mapnum >= WOLF_NUMLEVELS)
        Quit ("CA_CacheMap: Tried to load sparse map %d",mapnum);

    level = &wolf_levels[mapnum];

    mapwidth = level->width;
    mapheight = level->height;

    if (mapwidth != MAPSIZE || mapheight != MAPSIZE)
        Quit ("CA_CacheMap: Map %d not %u*%u!",mapnum,MAPSIZE,MAPSIZE);

    snprintf (mapname,sizeof(mapname),"%s",level->name);

    for (i = 0; i < MAPPLANES; i++)
    {
        if (!level->planes[i].length)
        {
            memset (mapsegs[i],0,MAPAREA * sizeof(*mapsegs[i]));
            continue;
        }

        src = wolf_gamemaps + level->planes[i].offset;

#ifdef CARMACIZED
        //
        // decarmackize into the scratch, then unRLEW out of it.  Both streams
        // start with their own two byte expanded length.
        //
        expanded = ReadShort(src);

        if (expanded > (int32_t)sizeof(MapScratch))
            Quit ("CA_CacheMap: Map %d plane %d expands to %d, scratch is %zu",
                  mapnum,i,expanded,sizeof(MapScratch));

        CAL_CarmackExpand ((byte *)(uintptr_t)(src + 2),MapScratch,expanded);

        expanded = MapScratch[0];
        CA_RLEWexpand (MapScratch + 1,mapsegs[i],expanded,WOLF_RLEWTAG);
#else
        expanded = ReadShort(src);
        CA_RLEWexpand ((word *)(uintptr_t)(src + 2),mapsegs[i],expanded,WOLF_RLEWTAG);
#endif
    }
}

#else

void CA_CacheMap (int mapnum)
{
    maptype     mapheader;
    mapfiletype fileheader;
    FILE        *file;
    char        fname[13];
    int32_t     pos,compressed,expanded;
    int         i;
    word        *source = NULL;
    word        *rlewtable = NULL;

//
// load maphead.ext (offsets for map file)
//
    snprintf (fname,sizeof(fname),"%s%s",mheadname,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    fread (&fileheader,sizeof(fileheader),1,file);

    fclose (file);

//
// open the data file
//
    snprintf (fname,sizeof(fname),"%s%s",mfilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    pos = fileheader.mapstart[mapnum];

    if (pos < 0)
        Quit ("CA_CacheMap: Tried to load sparse map %d",mapnum);

    fseek (file,pos,SEEK_SET);
    fread (&mapheader,sizeof(mapheader),1,file);

    mapwidth = mapheader.width;
    mapheight = mapheader.height;

    if (mapwidth != MAPSIZE || mapheight != MAPSIZE)
        Quit ("CA_CacheMap: Map %d not %u*%u!",mapnum,MAPSIZE,MAPSIZE);

//
// map names are NOT null-terminated, so copy the exact
// length of the mapheader name buffer into the mapname
// buffer
//
    memcpy (mapname,mapheader.name,sizeof(mapheader.name));
    mapname[MAPNAMESIZE] = '\0';

//
// load the planes into the already allocated buffers
//
    for (i = 0; i < MAPPLANES; i++)
    {
        pos = mapheader.planestart[i];
        compressed = mapheader.planelength[i];

        if (!compressed)
        {
            //
            // empty plane
            //
            memset (mapsegs[i],0,MAPAREA * sizeof(*mapsegs[i]));
            continue;
        }

        fseek (file,pos,SEEK_SET);

        source = SafeRealloc(source,compressed);
        fread (source,compressed,1,file);
#ifdef CARMACIZED
        //
        // decarmackize, then unRLEW
        // Both chunks have a two byte expanded length first
        //
        expanded = *source;
        rlewtable = SafeRealloc(rlewtable,expanded);
        CAL_CarmackExpand ((byte *)(source + 1),rlewtable,expanded);

        expanded = *rlewtable;
        CA_RLEWexpand (rlewtable + 1,mapsegs[i],expanded,fileheader.RLEWtag);
#else
        //
        // unRLEW
        //
        expanded = *source;
        CA_RLEWexpand (source + 1,mapsegs[i],expanded,fileheader.RLEWtag);
#endif
    }

    SafeFree (source);
    SafeFree (rlewtable);

    fclose (file);
}

#endif  /* USE_FLASH_ASSETS */
