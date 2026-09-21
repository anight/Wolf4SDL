// ID_VW.C

#include "wl_def.h"

// Uncomment the following line, if you get destination out of bounds
// assertion errors and want to ignore them during debugging
//#define IGNORE_BAD_DEST

#ifdef IGNORE_BAD_DEST
#undef assert
#define assert(x) if(!(x)) return
#define assert_ret(x) if(!(x)) return 0
#else
#define assert_ret(x) assert(x)
#endif

#ifdef NOTYET
    //
    // KS: need to find out how to support these
    // with the new code
    //
    #if defined(_arch_dreamcast)
    int      screenWidth = 320;
    int      screenHeight = 200;
    int      screenBits = 8;
    #elif defined(GP2X)
    int      screenWidth = 320;
    int      screenHeight = 240;
        #if defined(GP2X_940)
        int      screenBits = 8;
        #else
        int      screenBits = 16;
        #endif
    #endif
#endif

//
// The port is fixed at VGA mode 13h: 320x200 in 256 indexed colours.  That is
// the format the original art is already stored in, so at this size nothing
// scales and nothing is resampled - every pic, wall texture and sprite lands
// on the framebuffer at its authored size and screen.scale is 1.  It is also
// the only mode the target hardware has.
//
// screen.bits is the depth of screen.buffer, the framebuffer the game draws
// into.  screen.surface is the display it is shown on and need not be 8-bit:
// on a desktop it is whatever the window manager hands out, and the blit in
// VW_UpdateScreen() stands in for the hardware that expands indices through a
// CLUT on the way to a panel.  Code that touches the display asks the display
// what format it is; it must not assume screen.bits.
//
screen_t screen =
{
    .width  = SCREENWIDTH,
    .height = SCREENHEIGHT,
    .bits   = 8,
    .scale  = 1,
};

unsigned bordercolor;

pictabletype	*pictable;

int     px,py;
byte	fontcolor,backcolor;
int	    fontnumber;

uint32_t *ylookup;

SDL_Color palette1[256], palette2[256];
SDL_Color curpal[256];


#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1];
#define RGB(r, g, b) {(r)*255/63, (g)*255/63, (b)*255/63, SDL_ALPHA_OPAQUE}

SDL_Color gamepal[]={
#ifdef SPEAR
    #include "sodpal.inc"
#else
    #include "wolfpal.inc"
#endif
};

CASSERT(lengthof(gamepal) == 256)

//===========================================================================


/*
=======================
=
= VW_Startup
=
=======================
*/

#ifdef PICOWOLF

/*
= The framebuffer.  One of them: the game draws into the same bytes the panel
= is sent, because at 8bpp indexed there is nothing to convert between them -
= PicoSDL's PIO expands the indices through the CLUT on the way out.
=
= Static because PicoSDL allocates no pixels and its arena is 16 KB against
= the 62,500 this needs.
*/
#define PICOWOLF_CANVAS (SCREENWIDTH * SCREENHEIGHT)

static Uint8 PanelCanvas[PICOWOLF_CANVAS];

void VW_Startup (void)
{
#ifdef SPEAR
    screen.title = "Spear of Destiny";
#else
    screen.title = "Wolfenstein 3D";
#endif

    if (screen.width * screen.height != PICOWOLF_CANVAS)
        Quit ("VW_Startup: canvas is %dx%d, not 320x200",screen.width,screen.height);

    screen.window = PSDL_CreateWindow(PanelCanvas,screen.width,screen.height,
                                      screen.width);

    if (!screen.window)
        Quit ("Unable to create window: %s\n",SDL_GetError());

    VW_SetupVideo ();
    VW_InitRndMask ();
}

#else

void VW_Startup (void)
{
    int      x,y;
    int      w,h;
    uint32_t flags = 0;

    //
    // The frame is presented through the window's own surface, which is sized
    // to the window and is invalidated when the window changes size.  So the
    // window is not resizable, and fullscreen asks for a mode of the game's
    // own size rather than SDL_WINDOW_FULLSCREEN_DESKTOP, which would hand
    // back a desktop-sized surface for the game to draw a corner of.
    //
    if (screen.flags & SC_FULLSCREEN)
        flags |= SDL_WINDOW_FULLSCREEN;
    else
    {
        if (screen.flags & SC_INPUTGRABBED)
            flags |= SDL_WINDOW_INPUT_GRABBED;
    }

    x = SDL_WINDOWPOS_CENTERED;
    y = SDL_WINDOWPOS_CENTERED;
    w = screen.width;
    h = screen.height;
#ifdef SPEAR
    screen.title = "Spear of Destiny";
#else
    screen.title = "Wolfenstein 3D";
#endif
    screen.window = SDL_CreateWindow(screen.title,x,y,w,h,flags);

    if (!screen.window)
        Quit ("Unable to create window: %s\n",SDL_GetError());

    VW_SetupVideo ();
    VW_InitRndMask ();
}

#endif  /* PICOWOLF */


/*
===================
=
= VW_ClearVideo
=
= Deallocate what VW_SetupVideo allocated
=
===================
*/

void VW_ClearVideo (void)
{
    //
    // screen.surface is the window's own framebuffer.  SDL owns it and frees
    // it with the window, so it is dropped here rather than freed - and under
    // PICOWOLF screen.buffer is the same surface, so it is dropped too.
    //
    if (screen.buffer != screen.surface)
        SDL_FreeSurface (screen.buffer);

    screen.buffer = NULL;
    screen.surface = NULL;

    ylookup = NULL;                 // static storage; nothing to give back
}


/*
=======================
=
= VW_Shutdown
=
= Deallocate everything
=
=======================
*/

void VW_Shutdown (void)
{
    VW_ClearVideo ();

    SDL_DestroyWindow (screen.window);
    screen.window = NULL;
}


/*
=======================
=
= VW_SetupVideo
=
=======================
*/

void VW_SetupVideo (void)
{
    int i;
    int w,h;

    w = screen.width;
    h = screen.height;

    //
    // The window's own framebuffer is the presentation target: no renderer, no
    // texture, no format negotiation.  The platform states the depth it gives
    // us; screen.bits describes screen.buffer and is not it.
    //
    screen.surface = SDL_GetWindowSurface(screen.window);

    if (!screen.surface)
        Quit ("Unable to get the window surface: %s\n",SDL_GetError());

    //
    // create 8 bit screen buffer for drawing
    //
#ifdef PICOWOLF
    //
    // The same surface.  PicoSDL's window surface already wraps PanelCanvas
    // at 8bpp indexed, which is the format the game draws in, so there is no
    // second buffer and no conversion - VW_UpdateScreen() only presents.
    //
    screen.buffer = screen.surface;
#else
    screen.buffer = SDL_CreateRGBSurface(0,w,h,8,0,0,0,0);
#endif

    if (!screen.buffer)
        Quit ("Unable to create screen buffer surface: %s\n",SDL_GetError());

    VW_SetPalette (gamepal,false);

    //
    // Row offsets, one per screen row.  Static for the same reason as the
    // renderer's tables: the resolution is a constant.
    //
    {
        static uint32_t YLookupBuf[SCREENHEIGHT];

        if (screen.height > SCREENHEIGHT)
            Quit ("VW_SetupVideo: %d rows is more than the %d ylookup covers",
                  screen.height,SCREENHEIGHT);

        ylookup = YLookupBuf;
    }

    for (i = 0; i < screen.height; i++)
        ylookup[i] = i * screen.buffer->pitch;

    screen.scale = w / 320;
    screen.basewidth = w / screen.scale;
    screen.baseheight = h / screen.scale;
    screen.heightoffset = (screen.baseheight % 200) / 2;

    VW_SetBufferOffset (screen.heightoffset);

#ifndef PICOWOLF
    SDL_SetWindowMinimumSize (screen.window,screen.basewidth,screen.baseheight);
#endif
}


/*
===================
=
= VW_ChangeDisplay
=
===================
*/

#ifdef PICOWOLF

/*
= The panel is one size for ever and there is no window manager to ask, so
= there is no display to change.  The menu item that called this is gone with
= the resolution options; this stays because the fullscreen toggle still
= reaches it.
*/
void VW_ChangeDisplay (screen_t *scr)
{
    (void)scr;
}

void VW_ChangeWindow (screen_t *scr)
{
    (void)scr;
}

#else

void VW_ChangeDisplay (screen_t *scr)
{
    VW_ClearScreen (BLACK);
    VW_UpdateScreen ();

    VW_ChangeWindow (scr);

    if (scr->scale != screen.scale || scr->width != screen.width || scr->height != screen.height)
    {
        //
        // update screen variables and re-allocate everything
        //
        screen.width = scr->width;
        screen.height = scr->height;

        Shutdown3DRenderer ();
        VW_ClearVideo ();

        VW_SetupVideo ();
        VW_InitRndMask ();
        Init3DRenderer ();
    }
    else
    {
        //
        // Nothing to re-allocate, but going to or from fullscreen resized the
        // window, and the window's surface does not survive that: SDL frees it
        // and builds another.  The pointer has to be taken again.
        //
        screen.surface = SDL_GetWindowSurface(screen.window);

        if (!screen.surface)
            Quit ("Unable to get the window surface: %s\n",SDL_GetError());
    }
}


/*
===================
=
= VW_ChangeWindow
=
= Change the current window resolution and/or
= go to/from fullscreen
=
===================
*/

void VW_ChangeWindow (screen_t *scr)
{
    uint32_t        flags;
    SDL_DisplayMode dm;

    flags = SDL_GetWindowFlags(screen.window);

    if (screen.flags & SC_FULLSCREEN)
    {
        if (scr->scale != screen.scale || scr->width != screen.width || scr->height != screen.height)
        {
            if (SDL_GetWindowDisplayMode(screen.window,&dm))
                Quit ("Unable to get display mode: %s\n",SDL_GetError());

            dm.w = scr->width;
            dm.h = scr->height;

            if (SDL_SetWindowDisplayMode(screen.window,&dm))
                Quit ("Unable to set display mode: %s\n",SDL_GetError());
        }

        if (!(flags & SDL_WINDOW_FULLSCREEN))
        {
            if (SDL_SetWindowFullscreen(screen.window,SDL_WINDOW_FULLSCREEN_DESKTOP))
                Quit ("Unable to set fullscreen mode: %s\n",SDL_GetError());
        }
    }
    else
    {
        if (flags & SDL_WINDOW_FULLSCREEN)
        {
            if (SDL_SetWindowFullscreen(screen.window,0))
                Quit ("Unable to set windowed mode: %s\n",SDL_GetError());
        }

        //
        // KS: there's a weird bug here where switching back from 320x240
        // to 320x200 will not resize the window height - it stays at 240 and
        // scales up the screen. None of the other resolutions do this, but I
        // can't find out why it happens...
        //
        SDL_SetWindowSize (screen.window,scr->width,scr->height);
        SDL_SetWindowPosition (screen.window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    }
}

#endif  /* PICOWOLF */


/*
=============================================================================

						PALETTE OPS

		To avoid snow, do a WaitVBL BEFORE calling these

=============================================================================
*/

/*
=================
=
= VW_ConvertPalette
=
=================
*/

void VW_ConvertPalette(byte *srcpal, SDL_Color *destpal, int numColors)
{
    int i;

    for(i=0; i<numColors; i++)
    {
        destpal[i].r = *srcpal++ * 255 / 63;
        destpal[i].g = *srcpal++ * 255 / 63;
        destpal[i].b = *srcpal++ * 255 / 63;
        destpal[i].a = SDL_ALPHA_OPAQUE;
    }
}

/*
=================
=
= VW_FillPalette
=
=================
*/

void VW_FillPalette (int red, int green, int blue)
{
    int i;
    SDL_Color pal[256];

    for(i=0; i<256; i++)
    {
        pal[i].r = red;
        pal[i].g = green;
        pal[i].b = blue;
        pal[i].a = SDL_ALPHA_OPAQUE;
    }

    VW_SetPalette(pal, true);
}

//===========================================================================

/*
=================
=
= VW_GetColor
=
=================
*/

void VW_GetColor	(int color, int *red, int *green, int *blue)
{
    SDL_Color *col = &curpal[color];
    *red = col->r;
    *green = col->g;
    *blue = col->b;
}

//===========================================================================

/*
=================
=
= VW_SetPalette
=
=================
*/

void VW_SetPalette (SDL_Color *palette, bool forceupdate)
{
    memcpy(curpal, palette, sizeof(SDL_Color) * 256);

    SDL_SetPaletteColors(screen.buffer->format->palette, palette, 0, 256);

    if (screen.surface->format->palette)
    {
        //
        // An indexed display is the CLUT itself, so the frame already on it
        // stays valid and a fade costs 256 colour writes rather than a whole
        // screen of conversion.  This is the path the board takes.
        //
        SDL_SetPaletteColors(screen.surface->format->palette, palette, 0, 256);

        if (forceupdate)
            SDL_UpdateWindowSurface (screen.window);
    }
    else if (forceupdate)
    {
        // Otherwise the frame must be converted again through the new palette.
        VW_UpdateScreen ();
    }
}


//===========================================================================

/*
=================
=
= VW_GetPalette
=
=================
*/

void VW_GetPalette (SDL_Color *palette)
{
    memcpy(palette, curpal, sizeof(SDL_Color) * 256);
}


//===========================================================================

/*
=================
=
= VW_FadePaletteOut
=
= Fades the current palette to the given color in the given number of steps
=
=================
*/

void VW_FadePaletteOut (int red, int green, int blue, int steps)
{
	int		    i,j,orig,delta;
	SDL_Color   *origptr, *newptr;

    red = red * 255 / 63;
    green = green * 255 / 63;
    blue = blue * 255 / 63;

	VW_WaitVBL(1);
	VW_GetPalette(palette1);
	memcpy(palette2, palette1, sizeof(SDL_Color) * 256);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		origptr = &palette1[0];
		newptr = &palette2[0];
		for (j=0;j<256;j++)
		{
			orig = origptr->r;
			delta = red-orig;
			newptr->r = orig + delta * i / steps;
			orig = origptr->g;
			delta = green-orig;
			newptr->g = orig + delta * i / steps;
			orig = origptr->b;
			delta = blue-orig;
			newptr->b = orig + delta * i / steps;
			newptr->a = SDL_ALPHA_OPAQUE;
			origptr++;
			newptr++;
		}

        VW_WaitVBL(1);
		VW_SetPalette (palette2, true);
	}

//
// final color
//
	VW_FillPalette (red,green,blue);

	screen.flags |= SC_FADED;
}


/*
=================
=
= VW_FadePaletteIn
=
=================
*/

void VW_FadePaletteIn (SDL_Color *palette, int steps)
{
	int i,j,delta;

	VW_WaitVBL(1);
	VW_GetPalette(palette1);
	memcpy(palette2, palette1, sizeof(SDL_Color) * 256);

//
// fade through intermediate frames
//
	for (i=0;i<steps;i++)
	{
		for (j=0;j<256;j++)
		{
			delta = palette[j].r-palette1[j].r;
			palette2[j].r = palette1[j].r + delta * i / steps;
			delta = palette[j].g-palette1[j].g;
			palette2[j].g = palette1[j].g + delta * i / steps;
			delta = palette[j].b-palette1[j].b;
			palette2[j].b = palette1[j].b + delta * i / steps;
			palette2[j].a = SDL_ALPHA_OPAQUE;
		}

        VW_WaitVBL(1);
		VW_SetPalette(palette2, true);
	}

//
// final color
//
	VW_SetPalette (palette, true);

	screen.flags &= ~SC_FADED;
}


/*
=============================================================================

							PIXEL OPS

=============================================================================
*/

//
// PicoSDL has no SDL_MUSTLOCK: a surface is plain memory there and never needs
// locking, so the answer is always no.
//
#ifndef SDL_MUSTLOCK
#define SDL_MUSTLOCK(s) (0)
#endif

void *VW_LockSurface (SDL_Surface *surface)
{
    if (SDL_MUSTLOCK(surface))
    {
        if (SDL_LockSurface(surface) < 0)
            return NULL;
    }

    return surface->pixels;
}

void VW_UnlockSurface (SDL_Surface *surface)
{
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface (surface);
}


/*
=================
=
= VW_SetBufferOffset
=
= Set the offset in the screen buffer to start drawing
=
= The offset MUST be 0 while in the 3D renderer!
=
=================
*/

void VW_SetBufferOffset (unsigned offset)
{
    offset *= screen.scale;

	assert (offset < screen.height && "VW_SetBufferOffset: Invalid buffer offset!");

    screen.bufferofs = ylookup[offset];
}


/*
=================
=
= VW_Plot
=
=================
*/

void VW_Plot (int x, int y, int color)
{
    VW_Bar (x,y,1,1,color);
}


/*
=================
=
= VW_GetPixel
=
=================
*/

byte VW_GetPixel (int x, int y)
{
    byte *source;
    int  pixel;

    assert_ret(x >= 0 && x < screen.width
            && y >= 0 && y < screen.height
            && "VW_GetPixel: Pixel out of bounds!");

    source = VW_LockSurface(screen.buffer);

    if (source == NULL)
        return 0;

    pixel = source[screen.bufferofs + ylookup[y] + x];

    VW_UnlockSurface (screen.buffer);

    return pixel;
}


/*
=================
=
= VW_Bar
=
=================
*/

void VW_Bar (int x, int y, int width, int height, int color)
{
	byte *dest;

    x *= screen.scale;
    y *= screen.scale;
    width *= screen.scale;
    height *= screen.scale;

	assert (x >= 0 && x + width <= screen.width
            && y >= 0 && y + height <= screen.height
			&& "VW_Bar: Destination rectangle out of bounds!");

	dest = VW_LockSurface(screen.buffer);

	if (dest == NULL)
        return;

	dest += screen.bufferofs + ylookup[y] + x;

	while (height--)
	{
		memset (dest,color,width);

		dest += screen.buffer->pitch;
	}

	VW_UnlockSurface (screen.buffer);
}


void VW_Hlin (int x1, int x2, int y, int color)
{
    VW_Bar (x1,y,x2 - x1 + 1,1,color);
}

void VW_Vlin (int y1, int y2, int x, int color)
{
    VW_Bar (x,y1,1,y2 - y1 + 1,color);
}


/*
============================================================================

							STRING OPS

============================================================================
*/

void VW_DrawPropString (const char *string)
{
	fontstruct  *font;
	int		    width, step, height;
	byte	    *source, *dest;
	byte	    ch;
	int i;
	int sx, sy;

	dest = VW_LockSurface(screen.buffer);
	if(dest == NULL) return;

	font = (fontstruct *)grsegs[STARTFONT + fontnumber];
	height = font->height;
	dest += screen.bufferofs + (screen.scale * (ylookup[py] + px));

	while ((ch = (byte)*string++) != 0)
	{
		width = step = font->width[ch];
		source = ((byte *)font)+font->location[ch];
		while (width--)
		{
			for(i=0; i<height; i++)
			{
				if(source[i*step])
				{
					for(sy=0; sy<screen.scale; sy++)
						for(sx=0; sx<screen.scale; sx++)
							dest[ylookup[screen.scale*i+sy]+sx]=fontcolor;
				}
			}

			source++;
			px++;
			dest+=screen.scale;
		}
	}

	VW_UnlockSurface (screen.buffer);
}


/*
===================
=
= VW_MeasurePropString
=
===================
*/

void VW_MeasurePropString (const char *string, word *width, word *height)
{
    fontstruct *font;

    font = (fontstruct *)grsegs[STARTFONT + fontnumber];

    *height = font->height;

    for (*width = 0; *string; string++)
        *width += font->width[*((byte *)string)];    // proportional width
}


/*
============================================================================

							MEMORY OPS

============================================================================
*/


/*
===================
=
= VW_DePlaneVGA
=
= Unweave a VGA graphic to simplify drawing
=
===================
*/

void VW_DePlaneVGA (byte *source, int width, int height)
{
    int  x,y,plane;
    word size,pwidth;
    byte *temp,*dest,*srcline;

    size = width * height;

    if (width & 3)
        Quit ("DePlaneVGA: width not divisible by 4!");

    temp = SafeMalloc(size);

//
// munge pic into the temp buffer
//
    srcline = source;
    pwidth = width >> 2;

    for (plane = 0; plane < 4; plane++)
    {
        dest = temp;

        for (y = 0; y < height; y++)
        {
            for (x = 0; x < pwidth; x++)
                *(dest + (x << 2) + plane) = *srcline++;

            dest += width;
        }
    }

//
// copy the temp buffer back into the original source
//
    memcpy (source,temp,size);

    SafeFree (temp);
}


void VW_DrawTile8 (int x, int y, int tile)
{
	VW_MemToScreen (grsegs[STARTTILE8]+tile*64,8,8,x,y);
}


void VW_DrawPic (int x, int y, int chunknum)
{
	int	picnum = chunknum - STARTPICS;
	unsigned width,height;

	x &= ~7;

	width = pictable[picnum].width;
	height = pictable[picnum].height;

	VW_MemToScreen (grsegs[chunknum],width,height,x,y);
}


/*
=================
=
= VW_MemToScreen
=
= Draws a block of data to the screen
=
=================
*/

void VW_MemToScreen (const byte *source, int width, int height, int x, int y)
{
    byte *dest;
    int color;
    int i,j,sci,scj;
    int m,n;

    x *= screen.scale;
    y *= screen.scale;

    assert (x >= 0 && x + width * screen.scale <= screen.width
            && y >= 0 && y + height * screen.scale <= screen.height
            && "VW_MemToScreen: Destination rectangle out of bounds!");

    dest = VW_LockSurface(screen.buffer);

    if (dest == NULL)
        return;

    dest += screen.bufferofs;

    for (j = 0, scj = 0; j < height; j++, scj += screen.scale)
    {
        for (i = 0, sci = 0; i < width; i++, sci += screen.scale)
        {
            color = source[(j * width) + i];

            for (m = 0; m < screen.scale; m++)
            {
                for (n = 0; n < screen.scale; n++)
                    dest[ylookup[scj + m + y] + sci + n + x] = color;
            }
        }
    }

    VW_UnlockSurface (screen.buffer);
}


/*
=================
=
= VW_SegToScreen
=
= Draws a segment of a block of data to the screen.
= The block has the size srcwidth * height.
= The part at (srcx, srcy) has the size width * height
= and will be drawn to (destx, desty)
=
=================
*/

void VW_SegToScreen (byte *source, int srcwidth, int srcx, int srcy,
                     int destx, int desty, int width, int height)
{
    byte *dest;
    int color;
    int i,j,sci,scj;
    int m,n;

    destx *= screen.scale;
    desty *= screen.scale;

    assert (destx >= 0 && destx + width * screen.scale <= screen.width
            && desty >= 0 && desty + height * screen.scale <= screen.height
            && "VW_MemToScreenScaledCoord: Destination rectangle out of bounds!");

    dest = VW_LockSurface(screen.buffer);

    if (dest == NULL)
        return;

    dest += screen.bufferofs;

    for (j = 0, scj = 0; j < height; j++, scj += screen.scale)
    {
        for (i = 0, sci = 0; i < width; i++, sci += screen.scale)
        {
            color = source[((j + srcy) * srcwidth) + (i + srcx)];

            for (m = 0; m < screen.scale; m++)
            {
                for (n = 0; n < screen.scale; n++)
                    dest[ylookup[scj + m + desty] + sci + n + destx] = color;
            }
        }
    }

    VW_UnlockSurface (screen.buffer);
}


/*
=============================================================================

				Double buffer management routines

=============================================================================
*/

void VW_UpdateScreen (void)
{
    //
    // When the two are one surface there is nothing to copy: the game has
    // been drawing into the bytes the panel is about to be sent.  On a
    // desktop they differ in format and the blit is the conversion the
    // panel's CLUT does for free.
    //
    if (screen.buffer != screen.surface)
        SDL_BlitSurface (screen.buffer,NULL,screen.surface,NULL);

    SDL_UpdateWindowSurface (screen.window);

#ifdef PICOWOLF
    //
    // Wait for the panel to finish reading the framebuffer before anything
    // draws into it again.
    //
    // The present starts a DMA and returns; with two buffers the next frame
    // goes into the other one and the transfer runs underneath it, which is
    // where the frame rate comes from.  With one there is no other one - the
    // game's next write lands in the bytes the panel is still reading, and
    // what reaches the glass is half of each frame.  That is invisible on a
    // desktop, where the host backend's present copies synchronously; it is
    // the whole of the difference on hardware.
    //
    // Waiting here rather than before the next draw costs that overlap.  It is
    // the only point the game passes through between presenting and drawing
    // again - menus, the HUD and the 3D view all draw from their own places -
    // so buying the time back means finding a later one.  See TODO.md.
    //
    PSDL_PresentSync ();
#endif
}


/*
===================
=
= VW_FizzleFade
=
= Dissolve `color` into the framebuffer over a rectangle, a scattering of
= pixels at a time.
=
= It used to copy from screen.buffer into screen.surface, which meant the two
= had to be different pictures and so different memory.  Both callers filled
= the rectangle with a flat colour immediately beforehand and then dissolved
= that - so the source was never a picture, and writing the colour straight in
= is the same effect with nothing to copy from.  That is what lets the board
= hold one framebuffer instead of two.
=
= returns true if aborted
=
= It uses maximum-length Linear Feedback Shift Registers (LFSR) counters.
= You can find a list of them with lengths from 3 to 168 at:
= http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
= Many thanks to Xilinx for this list!!!
=
===================
*/

// XOR masks for the pseudo-random number sequence starting with n=17 bits
static const uint32_t rndmasks[] = {
                    // n    XNOR from (starting at 1, not 0 as usual)
    0x00012000,     // 17   17,14
    0x00020400,     // 18   18,11
    0x00040023,     // 19   19,6,2,1
    0x00090000,     // 20   20,17
    0x00140000,     // 21   21,19
    0x00300000,     // 22   22,21
    0x00420000,     // 23   23,18
    0x00e10000,     // 24   24,23,22,17
    0x01200000,     // 25   25,22      (this is enough for 8191x4095)
};

static unsigned int rndbits_y;
static unsigned int rndmask;

extern SDL_Color curpal[256];

// Returns the number of bits needed to represent the given value
static int log2_ceil(uint32_t x)
{
    int n = 0;
    uint32_t v = 1;
    while(v < x)
    {
        n++;
        v <<= 1;
    }
    return n;
}

void VW_InitRndMask (void)
{
    int rndbits_x = log2_ceil(screen.width);
    rndbits_y = log2_ceil(screen.height);

    int rndbits = rndbits_x + rndbits_y;
    if(rndbits < 17)
        rndbits = 17;       // no problem, just a bit slower
    else if(rndbits > 25)
        rndbits = 25;       // fizzle fade will not fill whole screen

    rndmask = rndmasks[rndbits - 17];
}

boolean VW_FizzleFade (int x1, int y1, int width, int height, int color,
                       int frames, boolean abortable)
{
    unsigned x,y,p,frame,pixperframe;
    int32_t  rndval;
    byte    *dest;

    x1 *= screen.scale;
    y1 *= screen.scale;
    width *= screen.scale;
    height *= screen.scale;

    rndval = 1;
    pixperframe = width * height / frames;

    IN_StartAck ();

    frame = GetTimeCount();

    dest = VW_LockSurface(screen.buffer);

    if (!dest)
        Quit ("VW_FizzleFade: unable to lock the framebuffer: %s\n",SDL_GetError());

    while (1)
    {
        IN_ProcessEvents();

        if (abortable && IN_CheckAck ())
        {
            VW_Bar (x1 / screen.scale,y1 / screen.scale,
                    width / screen.scale,height / screen.scale,color);
            VW_UnlockSurface (screen.buffer);
            VW_UpdateScreen ();
            return true;
        }

        for (p = 0; p < pixperframe; p++)
        {
            //
            // seperate random value into x/y pair
            //
            x = rndval >> rndbits_y;
            y = rndval & ((1 << rndbits_y) - 1);

            //
            // advance to next random element
            //
            rndval = (rndval >> 1) ^ (rndval & 1 ? 0 : rndmask);

            if (x >= (unsigned)width || y >= (unsigned)height)
                p--;                         // not into the view area; get a new pair
            else
                *(dest + (y1 + y) * screen.buffer->pitch + x1 + x) = (byte)color;

            if (rndval == 1)
            {
                //
                // entire sequence has been completed
                //
                VW_UnlockSurface (screen.buffer);
                VW_UpdateScreen ();

                return false;
            }
        }

        VW_UpdateScreen ();

        frame++;
        Delay(frame - GetTimeCount());        // don't go too fast
    }

    return false;
}

