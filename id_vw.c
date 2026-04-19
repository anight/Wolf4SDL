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

boolean  fullscreen = true;
#if defined(_arch_dreamcast)
int16_t  screenWidth = 320;
int16_t  screenHeight = 200;
int      screenBits = 8;
#elif defined(GP2X)
int16_t  screenWidth = 320;
int16_t  screenHeight = 240;
#if defined(GP2X_940)
int      screenBits = 8;
#else
int      screenBits = 16;
#endif
#else
int16_t  screenWidth = 640;
int16_t  screenHeight = 400;
int      screenBits = -1;      // use "best" color depth according to libSDL
#endif

SDL_Surface *screen = NULL;
unsigned screenPitch;

SDL_Surface *screenBuffer = NULL;
unsigned bufferPitch;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

int      scaleFactor;
int      basescreenWidth;
int      basescreenHeight;

boolean	 screenfaded;
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
= VW_Shutdown
=
=======================
*/

void VW_Shutdown (void)
{
    SDL_FreeSurface (screenBuffer);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(texture);

    SafeFree (ylookup);
    SafeFree (pixelangle);
    SafeFree (wallheight);
#if defined(USE_FLOORCEILINGTEX) || defined(USE_CLOUDSKY)
    SafeFree (spanstart);
#endif
    screenBuffer = NULL;
    renderer = NULL;
    window = NULL;
    texture = NULL;
}


/*
=======================
=
= VW_SetVGAPlaneMode
=
=======================
*/

void VW_SetVGAPlaneMode (void)
{
    int i;
    uint32_t a,r,g,b;

#ifdef SPEAR
    const char* title = "Spear of Destiny";
#else
    const char* title = "Wolfenstein 3D";
#endif
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight,
        (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | SDL_WINDOW_OPENGL);

    SDL_PixelFormatEnumToMasks (SDL_PIXELFORMAT_ARGB8888,&screenBits,&r,&g,&b,&a);

    screen = SDL_CreateRGBSurface(0,screenWidth,screenHeight,screenBits,r,g,b,a);

    if(!screen)
    {
        printf("Unable to set %ix%ix%i video mode: %s\n", screenWidth, screenHeight, screenBits, SDL_GetError());
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_ShowCursor(SDL_DISABLE);

    SDL_SetPaletteColors(screen->format->palette, gamepal, 0, 256);
    memcpy(curpal, gamepal, sizeof(SDL_Color) * 256);

    screenBuffer = SDL_CreateRGBSurface(0, screenWidth,
        screenHeight, 8, 0, 0, 0, 0);
    if(!screenBuffer)
    {
        printf("Unable to create screen buffer surface: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetPaletteColors(screenBuffer->format->palette, gamepal, 0, 256);

    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        screenWidth, screenHeight);

    screenPitch = screen->pitch;
    bufferPitch = screenBuffer->pitch;

    scaleFactor = screenWidth/320;
    if(screenHeight/200 < scaleFactor) scaleFactor = screenHeight/200;

    basescreenWidth = screenWidth / scaleFactor;
    basescreenHeight = screenHeight / scaleFactor;

    ylookup = SafeMalloc(screenHeight * sizeof(*ylookup));
    pixelangle = SafeMalloc(screenWidth * sizeof(*pixelangle));
    wallheight = SafeMalloc(screenWidth * sizeof(*wallheight));
#if defined(USE_FLOORCEILINGTEX) || defined(USE_CLOUDSKY)
    spanstart = SafeMalloc((screenHeight / 2) * sizeof(*spanstart));
#endif

    for (i = 0; i < screenHeight; i++)
        ylookup[i] = i * bufferPitch;
}

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

    if(screenBits == 8)
        SDL_SetPaletteColors(screen->format->palette, palette, 0, 256);
    else
    {
        SDL_SetPaletteColors(screenBuffer->format->palette, palette, 0, 256);
        if (forceupdate)
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

	screenfaded = true;
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
	screenfaded = false;
}


/*
=============================================================================

							PIXEL OPS

=============================================================================
*/

byte *VW_LockSurface (SDL_Surface *surface)
{
    if (SDL_MUSTLOCK(surface))
    {
        if (SDL_LockSurface(surface) < 0)
            return NULL;
    }

    return (byte *)surface->pixels;
}

void VW_UnlockSurface (SDL_Surface *surface)
{
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface (surface);
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

    assert_ret(x >= 0 && x < screenWidth
            && y >= 0 && y < screenHeight
            && "VW_GetPixel: Pixel out of bounds!");

    source = VW_LockSurface(screenBuffer);

    if (source == NULL)
        return 0;

    pixel = source[ylookup[y] + x];

    VW_UnlockSurface(screenBuffer);

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

    x *= scaleFactor;
    y *= scaleFactor;
    width *= scaleFactor;
    height *= scaleFactor;

	assert (x >= 0 && x + width <= screenWidth
            && y >= 0 && y + height <= screenHeight
			&& "VW_Bar: Destination rectangle out of bounds!");

	dest = VW_LockSurface(screenBuffer);

	if (dest == NULL)
        return;

	dest += ylookup[y] + x;

	while (height--)
	{
		memset (dest,color,width);

		dest += bufferPitch;
	}

	VW_UnlockSurface (screenBuffer);
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

	dest = VW_LockSurface(screenBuffer);
	if(dest == NULL) return;

	font = (fontstruct *) grsegs[STARTFONT+fontnumber];
	height = font->height;
	dest += scaleFactor * (ylookup[py] + px);

	while ((ch = (byte)*string++)!=0)
	{
		width = step = font->width[ch];
		source = ((byte *)font)+font->location[ch];
		while (width--)
		{
			for(i=0; i<height; i++)
			{
				if(source[i*step])
				{
					for(sy=0; sy<scaleFactor; sy++)
						for(sx=0; sx<scaleFactor; sx++)
							dest[ylookup[scaleFactor*i+sy]+sx]=fontcolor;
				}
			}

			source++;
			px++;
			dest+=scaleFactor;
		}
	}

	VW_UnlockSurface(screenBuffer);
}


void VW_MeasurePropString (const char *string, word *width, word *height)
{
    fontstruct *font;

    font = (fontstruct *)grsegs[STARTFONT + fontnumber];

    *height = font->height;

	for (*width = 0; *string; string++)
		*width += font->width[*((byte *)string)];	// proportional width
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

void VW_MemToScreen (byte *source, int width, int height, int x, int y)
{
    byte *dest;
    int color;
    int i,j,sci,scj;
    int m,n;

    x *= scaleFactor;
    y *= scaleFactor;

    assert (x >= 0 && x + width * scaleFactor <= screenWidth
            && y >= 0 && y + height * scaleFactor <= screenHeight
            && "VW_MemToScreen: Destination rectangle out of bounds!");

    dest = VW_LockSurface(screenBuffer);

    if (dest == NULL)
        return;

    for (j = 0, scj = 0; j < height; j++, scj += scaleFactor)
    {
        for (i = 0, sci = 0; i < width; i++, sci += scaleFactor)
        {
            color = source[(j * width) + i];

            for (m = 0; m < scaleFactor; m++)
            {
                for (n = 0; n < scaleFactor; n++)
                    dest[ylookup[scj + m + y] + sci + n + x] = color;
            }
        }
    }

    VW_UnlockSurface (screenBuffer);
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

    destx *= scaleFactor;
    desty *= scaleFactor;

    assert (destx >= 0 && destx + width * scaleFactor <= screenWidth
            && desty >= 0 && desty + height * scaleFactor <= screenHeight
            && "VW_MemToScreenScaledCoord: Destination rectangle out of bounds!");

    dest = VW_LockSurface(screenBuffer);

    if (dest == NULL)
        return;

    for (j = 0, scj = 0; j < height; j++, scj += scaleFactor)
    {
        for (i = 0, sci = 0; i < width; i++, sci += scaleFactor)
        {
            color = source[((j + srcy) * srcwidth) + (i + srcx)];

            for (m = 0; m < scaleFactor; m++)
            {
                for (n = 0; n < scaleFactor; n++)
                    dest[ylookup[scj + m + desty] + sci + n + destx] = color;
            }
        }
    }

    VW_UnlockSurface (screenBuffer);
}


/*
=============================================================================

				Double buffer management routines

=============================================================================
*/

void VW_UpdateScreen (void)
{
	SDL_BlitSurface (screenBuffer,NULL,screen,NULL);

    SDL_UpdateTexture(texture, NULL, screen->pixels, screenPitch);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


/*
===================
=
= VW_FizzleFade
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

void VW_Startup (void)
{
    int rndbits_x = log2_ceil(screenWidth);
    rndbits_y = log2_ceil(screenHeight);

    int rndbits = rndbits_x + rndbits_y;
    if(rndbits < 17)
        rndbits = 17;       // no problem, just a bit slower
    else if(rndbits > 25)
        rndbits = 25;       // fizzle fade will not fill whole screen

    rndmask = rndmasks[rndbits - 17];
}

boolean VW_FizzleFade (int x1, int y1, int width, int height, int frames, boolean abortable)
{
    unsigned x, y, p, frame, pixperframe;
    int32_t  rndval;

    x1 *= scaleFactor;
    y1 *= scaleFactor;
    width *= scaleFactor;
    height *= scaleFactor;

    rndval = 1;
    pixperframe = width * height / frames;

    IN_StartAck ();

    frame = GetTimeCount();
    byte *srcptr = VW_LockSurface(screenBuffer);
    if(srcptr == NULL) return false;

    while (1)
    {
        IN_ProcessEvents();

        if(abortable && IN_CheckAck ())
        {
            VW_UnlockSurface(screenBuffer);
            VW_UpdateScreen ();
            return true;
        }

        byte *destptr = VW_LockSurface(screen);

        if (!destptr)
            Quit ("Unable to lock dest surface: %s\n",SDL_GetError());

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

            if (x >= width || y >= height)
                p--;                         // not into the view area; get a new pair
            else
            {
                //
                // copy one pixel
                //
                if(screenBits == 8)
                {
                    *(destptr + (y1 + y) * screen->pitch + x1 + x)
                        = *(srcptr + (y1 + y) * screenBuffer->pitch + x1 + x);
                }
                else
                {
                    byte col = *(srcptr + (y1 + y) * screenBuffer->pitch + x1 + x);
                    uint32_t fullcol = SDL_MapRGBA(screen->format, curpal[col].r, curpal[col].g, curpal[col].b,SDL_ALPHA_OPAQUE);
                    memcpy(destptr + (y1 + y) * screen->pitch + (x1 + x) * screen->format->BytesPerPixel,
                        &fullcol, screen->format->BytesPerPixel);
                }
            }

            if (rndval == 1)
            {
                //
                // entire sequence has been completed
                //
                VW_UnlockSurface (screenBuffer);
                VW_UnlockSurface (screen);
                VW_UpdateScreen ();

                return false;
            }
        }

        VW_UnlockSurface(screen);

        SDL_UpdateTexture(texture, NULL, screen->pixels, screenPitch);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame++;
        Delay(frame - GetTimeCount());        // don't go too fast
    }

    return false;
}

