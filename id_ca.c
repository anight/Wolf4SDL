// ID_CA.C

// this has been customized for WOLF

/*
=============================================================================

Id Software Caching Manager

=============================================================================
*/

#include <sys/types.h>
#if defined _WIN32
    #include <io.h>
#elif defined _arch_dreamcast
    #include <unistd.h>
#else
    #include <sys/uio.h>
    #include <unistd.h>
#endif

#include "wl_def.h"

#define THREEBYTEGRSTARTS

/*
=============================================================================

                             LOCAL CONSTANTS

=============================================================================
*/

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


/*
=============================================================================

                             GLOBAL VARIABLES

=============================================================================
*/

word    *mapsegs[MAPPLANES];
maptype *mapheaderseg[NUMMAPS];
byte    *audiosegs[NUMSNDCHUNKS];
byte    *grsegs[NUMCHUNKS];

mapfiletype *tinf;

/*
=============================================================================

                             LOCAL VARIABLES

=============================================================================
*/

char extension[5]; // Need a string, not constant to change cache files
char graphext[5];
char audioext[5];
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

static int32_t  grstarts[NUMCHUNKS + 1];
static int32_t* audiostarts; // array of offsets in audio / audiot

#ifdef GRHEADERLINKED
huffnode *grhuffman;
#else
huffnode grhuffman[255];
#endif

FILE   *audiofile;

int32_t   chunkcomplen,chunkexplen;

byte   oldsoundmode;


static int32_t GRFILEPOS(const size_t idx)
{
	assert(idx < lengthof(grstarts));
	return grstarts[idx];
}

/*
=============================================================================

                            LOW LEVEL ROUTINES

=============================================================================
*/


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
============================
=
= CAL_GetGrChunkLength
=
= Gets the length of an explicit length chunk (not tiles)
= The file pointer is positioned so the compressed data can be read in next.
=
============================
*/

void CAL_GetGrChunkLength (FILE *grfile, int chunk)
{
    fseek (grfile,GRFILEPOS(chunk),SEEK_SET);
    fread (&chunkexplen,sizeof(chunkexplen),1,grfile);
    chunkcomplen = GRFILEPOS(chunk+1)-GRFILEPOS(chunk)-4;
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

void CAL_SetupGrFile (void)
{
    char fname[13];
    FILE *file;
    byte *compseg;

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

    fseek (file,0,SEEK_END);
    int32_t headersize = ftell(file);
    fseek (file,0,SEEK_SET);

	int expectedsize = lengthof(grstarts);

    if(!param_ignorenumchunks && headersize / 3 != expectedsize)
        Quit("Wolf4SDL was not compiled for these data files:\n"
            "%s contains a wrong number of offsets (%i instead of %i)!\n\n"
            "Please check whether you are using the right executable!\n"
            "(For mod developers: perhaps you forgot to update NUMCHUNKS?)",
            fname, headersize / 3, expectedsize);

    byte data[lengthof(grstarts) * 3];
    fread (data,sizeof(data),1,file);
    fclose (file);

    byte *d = data;
    int32_t* i;
    for (i = grstarts; i != endof(grstarts); ++i)
    {
        int32_t val = d[0] | (d[1] << 8) | (d[2] << 16);
        *i = (val == 0x00FFFFFF ? -1 : val);
        d += 3;
    }

//
// Open the graphics file
//
    snprintf (fname,sizeof(fname),"%s%s",gfilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

//
// load the pic and sprite headers into the arrays in the data segment
//
    pictable = SafeMalloc(NUMPICS * sizeof(*pictable));
    CAL_GetGrChunkLength (file,STRUCTPIC);                // position file pointer
    compseg = SafeMalloc(chunkcomplen);
    fread (compseg,chunkcomplen,1,file);
    CAL_HuffExpand(compseg, (byte*)pictable, NUMPICS * sizeof(*pictable), grhuffman);
    SafeFree (compseg);

    CA_CacheGrChunks (file);

    fclose (file);
}

//==========================================================================


/*
======================
=
= CAL_SetupMapFile
=
======================
*/

void CAL_SetupMapFile (void)
{
    int     i;
    int32_t pos;
    FILE *file;
    char fname[13];

//
// load maphead.ext (offsets and tileinfo for map file)
//
    snprintf (fname,sizeof(fname),"%s%s",mheadname,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

    tinf = SafeMalloc(sizeof(*tinf));

    fread (tinf,sizeof(*tinf),1,file);
    fclose (file);

//
// open the data file
//
    snprintf (fname,sizeof(fname),"%s%s",mfilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

//
// load all map header
//
    for (i=0;i<NUMMAPS;i++)
    {
        pos = tinf->headeroffsets[i];
        if (pos<0)                          // $FFFFFFFF start is a sparse map
            continue;

        mapheaderseg[i] = SafeMalloc(sizeof(*mapheaderseg[i]));

        fseek (file,pos,SEEK_SET);
        fread (mapheaderseg[i],sizeof(*mapheaderseg[i]),1,file);
    }

    fclose (file);

//
// allocate space for 3 64*64 planes
//
    for (i=0;i<MAPPLANES;i++)
        mapsegs[i] = SafeMalloc(MAPAREA * sizeof(*mapsegs[i]));
}


//==========================================================================


/*
======================
=
= CAL_SetupAudioFile
=
======================
*/

void CAL_SetupAudioFile (void)
{
    char fname[13];

//
// load audiohed.ext (offsets for audio file)
//
    snprintf (fname,sizeof(fname),"%s%s",aheadname,extension);

    CA_LoadFile (fname,(void **)&audiostarts);

//
// open the data file
//
    snprintf (fname,sizeof(fname),"%s%s",afilename,extension);

    audiofile = fopen(fname,"rb");

    if (!audiofile)
        CA_CannotOpen (fname);
}

//==========================================================================


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
    int i,start;

    if (audiofile)
        fclose (audiofile);

    for (i = 0; i < NUMCHUNKS; i++)
        SafeFree (grsegs[i]);

    for (i = 0; i < NUMMAPS; i++)
        SafeFree (mapheaderseg[i]);

    for (i = 0; i < MAPPLANES; i++)
        SafeFree (mapsegs[i]);

    SafeFree (pictable);
    SafeFree (tinf);

    if (oldsoundmode != sdm_Off)
    {
        switch (oldsoundmode)
        {
            case sdm_PC:
                start = STARTPCSOUNDS;
                break;

            case sdm_AdLib:
                start = STARTADLIBSOUNDS;
                break;
        }

        for (i = 0; i < NUMSOUNDS; i++, start++)
            SafeFree (audiosegs[start]);
    }
}

//===========================================================================

/*
======================
=
= CA_CacheAudioChunk
=
======================
*/

int32_t CA_CacheAudioChunk (int chunk)
{
    int32_t pos = audiostarts[chunk];
    int32_t size = audiostarts[chunk+1]-pos;

    if (audiosegs[chunk])
        return size;                        // already in memory

    audiosegs[chunk] = SafeMalloc(size);

    fseek (audiofile,pos,SEEK_SET);
    fread (audiosegs[chunk],size,1,audiofile);

    return size;
}

void CA_CacheAdlibSoundChunk (int chunk)
{
    AdLibSound *sound;
    byte    *bufferseg;
    byte    *ptr;
    int32_t pos = audiostarts[chunk];
    int32_t size = audiostarts[chunk+1]-pos;

    if (audiosegs[chunk])
        return;                        // already in memory

    fseek (audiofile,pos,SEEK_SET);

    bufferseg = SafeMalloc(ORIG_ADLIBSOUND_SIZE - 1);
    ptr = bufferseg;

    fread (ptr,ORIG_ADLIBSOUND_SIZE - 1,1,audiofile);   // without data[1]

    audiosegs[chunk] = SafeMalloc(size + sizeof(*sound) - ORIG_ADLIBSOUND_SIZE);
    sound = (AdLibSound *)audiosegs[chunk];

    sound->common.length = ReadLong(ptr);
    ptr += 4;

    sound->common.priority = ReadShort(ptr);
    ptr += 2;

    sound->inst.mChar = *ptr++;
    sound->inst.cChar = *ptr++;
    sound->inst.mScale = *ptr++;
    sound->inst.cScale = *ptr++;
    sound->inst.mAttack = *ptr++;
    sound->inst.cAttack = *ptr++;
    sound->inst.mSus = *ptr++;
    sound->inst.cSus = *ptr++;
    sound->inst.mWave = *ptr++;
    sound->inst.cWave = *ptr++;
    sound->inst.nConn = *ptr++;
    sound->inst.voice = *ptr++;
    sound->inst.mode = *ptr++;
    sound->inst.unused[0] = *ptr++;
    sound->inst.unused[1] = *ptr++;
    sound->inst.unused[2] = *ptr++;
    sound->block = *ptr++;

    fread (sound->data,size - ORIG_ADLIBSOUND_SIZE + 1,1,audiofile);  // + 1 because of byte data[1]

    SafeFree (bufferseg);
}

//===========================================================================

/*
======================
=
= CA_LoadAllSounds
=
= Purges all sounds, then loads all new ones (mode switch)
=
======================
*/

void CA_LoadAllSounds (void)
{
    unsigned start,i;

    if (oldsoundmode != sdm_Off)
    {
        switch (oldsoundmode)
        {
            case sdm_PC:
                start = STARTPCSOUNDS;
                break;
            case sdm_AdLib:
                start = STARTADLIBSOUNDS;
                break;
        }

        for (i = 0; i < NUMSOUNDS; i++, start++)
            SafeFree (audiosegs[start]);
    }

    oldsoundmode = SoundMode;

    switch (SoundMode)
    {
        case sdm_Off:
            start = STARTADLIBSOUNDS;   // needed for priorities...
            break;
        case sdm_PC:
            start = STARTPCSOUNDS;
            break;
        case sdm_AdLib:
            start = STARTADLIBSOUNDS;
            break;
    }

    if(start == STARTADLIBSOUNDS)
    {
        for (i=0;i<NUMSOUNDS;i++,start++)
            CA_CacheAdlibSoundChunk(start);
    }
    else
    {
        for (i=0;i<NUMSOUNDS;i++,start++)
            CA_CacheAudioChunk(start);
    }
}

//===========================================================================


/*
======================
=
= CAL_ExpandGrChunk
=
= Does whatever is needed with a pointer to a compressed chunk
=
======================
*/

void CAL_ExpandGrChunk (int chunk, byte *source)
{
    int32_t    expanded;

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

    CAL_HuffExpand (source,grsegs[chunk],expanded,grhuffman);
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

void CA_CacheGrChunks (FILE *grfile)
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
        pos = GRFILEPOS(chunk);

        if (pos<0)                              // $FFFFFFFF start is a sparse tile
            continue;

        next = chunk + 1;

        while (GRFILEPOS(next) == -1)           // skip past any sparse tiles
            next++;

        compressed = GRFILEPOS(next)-pos;

        fseek (grfile,pos,SEEK_SET);

        source = SafeRealloc(source,compressed);
        fread (source,compressed,1,grfile);

        CAL_ExpandGrChunk (chunk,source);

        if (chunk >= STARTPICS && chunk < STARTEXTERNS)
            CAL_DeplaneGrChunk (chunk);
    }

    SafeFree (source);
}



//==========================================================================


/*
======================
=
= CA_CacheMap
=
= WOLF: This is specialized for a 64*64 map size
=
======================
*/

void CA_CacheMap (int mapnum)
{
    FILE     *file;
    char     fname[13];
    int32_t  pos,compressed;
    int      plane;
    word     *source = NULL;
    word     *rlewtable = NULL;
    int32_t  expanded;

    if (mapheaderseg[mapnum]->width != MAPSIZE || mapheaderseg[mapnum]->height != MAPSIZE)
        Quit ("CA_CacheMap: Map not %u*%u!",MAPSIZE,MAPSIZE);

    snprintf (fname,sizeof(fname),"%s%s",mfilename,extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen (fname);

//
// load the planes into the allready allocated buffers
//
    for (plane = 0; plane < MAPPLANES; plane++)
    {
        pos = mapheaderseg[mapnum]->planestart[plane];
        compressed = mapheaderseg[mapnum]->planelength[plane];

        if (!compressed)
            continue;    // empty plane

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
        CA_RLEWexpand (rlewtable + 1,mapsegs[plane],expanded,tinf->RLEWtag);
#else
        //
        // unRLEW
        //
        expanded = *source;
        CA_RLEWexpand (source + 1,mapsegs[plane],expanded,tinf->RLEWtag);
#endif
    }

    SafeFree (source);
    SafeFree (rlewtable);

    fclose (file);
}

//===========================================================================

void CA_CannotOpen (const char *string)
{
    Quit ("Can't open %s!",string);
}
