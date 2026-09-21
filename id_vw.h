// ID_VW.H

#ifndef _ID_VW_H_
#define _ID_VW_H_


#define WHITE			15			// graphics mode independant colors
#define BLACK			0


typedef struct screenstruct
{
    int          width,height;
    int          basewidth,baseheight;
    int          heightoffset;
    int          scale;
    int          bits;
    unsigned     flags;
    unsigned     bufferofs;
    const char   *title;

    SDL_Window   *window;

    //
    // surface is the window's own framebuffer and is owned by SDL; buffer is
    // the 8-bit indexed framebuffer the game draws into.
    //
    SDL_Surface  *surface,*buffer;
} screen_t;


enum screenflags
{
    SC_FULLSCREEN    = 0x0001,
    SC_FADED         = 0x0002,
    SC_FIZZLEIN      = 0x0004,
    SC_INPUTGRABBED  = 0x0008,
};


typedef struct
{
	int16_t width,height;
} pictabletype;


typedef struct
{
	int16_t height;
	int16_t location[256];
	int8_t width[256];
} fontstruct;


extern  screen_t        screen;
extern	pictabletype	*pictable;

extern  byte            fontcolor,backcolor;
extern	int             fontnumber;
extern	int             px,py;

extern	unsigned bordercolor;

extern  uint32_t *ylookup;

extern  SDL_Color gamepal[256];

//===========================================================================

#define SETFONTCOLOR(f,b) fontcolor=f;backcolor=b;

#define VW_WaitVBL(a)        SDL_Delay((a) * 8)
#define VW_ClearScreen(c)    SDL_FillRect(screen.buffer,NULL,(c))

#define VW_FadeIn()		    VW_FadePaletteIn(gamepal,30)
#define VW_FadeOut()	    VW_FadePaletteOut(0,0,0,30)

void VW_DePlaneVGA (byte *source, int width, int height);
void VW_SetupVideo (void);
void VW_Startup (void);
void VW_Shutdown (void);
void VW_InitRndMask (void);
void VW_ClearVideo (void);
void VW_ChangeDisplay (screen_t *scr);
void VW_ChangeWindow (screen_t *scr);

void VW_ConvertPalette (byte *srcpal, SDL_Color *destpal, int numColors);
void VW_FillPalette (int red, int green, int blue);
void VW_GetColor (int color, int *red, int *green, int *blue);
void VW_SetPalette (SDL_Color *palette, bool forceupdate);
void VW_GetPalette (SDL_Color *palette);
void VW_FadePaletteOut (int red, int green, int blue, int steps);
void VW_FadePaletteIn (SDL_Color *palette, int steps);

void *VW_LockSurface(SDL_Surface *surface);
void VW_UnlockSurface(SDL_Surface *surface);
void VW_SetBufferOffset (unsigned offset);

byte VW_GetPixel (int x, int y);
void VW_Plot (int x, int y, int color);
void VW_Hlin (int x1, int x2, int y, int color);
void VW_Vlin (int y1, int y2, int x, int color);
void VW_Bar (int x, int y, int width, int height, int color);

void VW_DrawPropString	 (const char *string);

void VW_DrawTile8 (int x, int y, int tile);
void VW_DrawPic (int x, int y, int chunknum);

void VW_UpdateScreen (void);

void VW_SegToScreen (byte *source, int srcwidth, int srcx, int srcy,
                     int destx, int desty, int width, int height);

void VW_MemToScreen (byte *source, int width, int height, int x, int y);

void VW_MeasurePropString (const char *string, word *width, word *height);

boolean VW_FizzleFade (int x1, int y1, int width, int height, int frames, boolean abortable);

#endif
