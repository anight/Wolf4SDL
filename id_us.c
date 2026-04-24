//
//	ID Engine
//	ID_US.c - User Manager - General routines
//	v1.1d1
//	By Jason Blochowiak
//
//	This module handles dealing with user input & feedback
//
//	Globals:
//		ingame - Flag set by game indicating if a game is in progress
//		loadedgame - Flag set if a game was loaded
//		PrintX, PrintY - Where the User Mgr will print (global coords)
//		WindowX,WindowY,WindowW,WindowH - The dimensions of the current
//			window
//

#include "wl_def.h"


#if _MSC_VER == 1200            // Visual C++ 6
	#define vsnprintf _vsnprintf
#endif

//	Global variables
int PrintX,PrintY;
int WindowX,WindowY,WindowW,WindowH;

//	Internal variables
#define	ConfigVersion	1

static	boolean		US_Started;

HighScore	Scores[MaxScores] =
{
    {"id software-'92",10000,1},
    {"Adrian Carmack",10000,1},
    {"John Carmack",10000,1},
    {"Kevin Cloud",10000,1},
    {"Tom Hall",10000,1},
    {"John Romero",10000,1},
    {"Jay Wilbur",10000,1},
};

int rndindex;

static byte rndtable[] =
{
      0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
	 74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
	 95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
	 52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
	175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
	 25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
	 94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
	136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
	 80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
	 24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
	145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
	 28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
	 71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
	 17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
	197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
	120, 163, 236, 249,
};

//	Internal routines

//	Public routines

///////////////////////////////////////////////////////////////////////////
//
//	US_Startup() - Starts the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void US_Startup (void)
{
	if (US_Started)
		return;

	US_InitRndT (true);		// Initialize the random number generator

	US_Started = true;
}


///////////////////////////////////////////////////////////////////////////
//
//	US_Shutdown() - Shuts down the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void US_Shutdown (void)
{
	if (!US_Started)
		return;

	US_Started = false;
}


/*
=============================================================================

                    WINDOW/PRINTING ROUTINES

=============================================================================
*/


/*
=====================
=
= US_Print
=
= Prints a string in the current window. Newlines are supported
=
= If the line starts with '\t', it will be centered horizontally
=
=====================
*/

void US_Print (const char *string)
{
	stringtype s;

	while (*string)
	{
		VW_MeasurePropString (string,&s,'\n');

        if (*string == '\t')
        {
            string++;
            PrintX = WindowX + ((WindowW - s.width) / 2);
        }

        if (WindowW <= s.width)
            Quit ("String \"%s\" exceeds width",string);

		px = PrintX;
		py = PrintY;
		VW_DrawPropString (string);

		string += s.length;

		if (*string == '\n')
		{
			string++;

			PrintX = WindowX;
			PrintY += fontsegs[fontnumber]->height;
		}
		else
			PrintX += s.width;
	}
}


/*
=====================
=
= US_PrintWindow
=
= Generates a window and prints a string in it
=
=====================
*/

void US_PrintWindow (const char *string)
{
    string = US_GenerateWindowFromString(string,0);

    US_Print (string);
}


/*
=====================
=
= US_GenerateWindowFromString
=
= Generates a window using info from the given string
=
= If the first character in the string is '\v',
= the string will be centered vertically
=
= Returns the string so that the caller can start printing
= after any formatting characters have been parsed
=
=====================
*/

const char *US_GenerateWindowFromString (const char *string, int maxchars)
{
    int        i,widest;
    int        extralines = 1;
    stringtype s;
    fontstruct *font;

    font = fontsegs[fontnumber];

    VW_MeasurePropString (string,&s,'\0');

    if (*string == '\v')
    {
        string++;
        extralines++;
    }

    //
    // if input is expected, add enough width to fit
    // the widest character in the current font in the
    // entire input
    //
    if (maxchars > 0)
    {
        widest = 0;

        for (i = 0; i < lengthof(font->width); i++)
        {
            if (font->width[i] > widest)
                widest = font->width[i];
        }

        //
        // round up widest to the next tile8
        //
        if (widest % 8)
        {
            widest += 8;
            widest -= widest % 8;
        }

        s.width += maxchars * widest;

        if (*string == '\t')
            s.width += 8;     // centered text
    }

    US_CenterWindow ((s.width + 7) / 8,s.lines + extralines);

    if (extralines > 1)
        PrintY = WindowY + ((WindowH - font->height) / 2);

    return string;
}


///////////////////////////////////////////////////////////////////////////
//
//  US_Printf() - Prints a formatted string
//
///////////////////////////////////////////////////////////////////////////

void US_Printf (const char *formatStr, ...)
{
    char    strbuf[256];
    va_list vlist;
    int     len;

    va_start (vlist,formatStr);
    len = vsnprintf(strbuf,sizeof(strbuf),formatStr,vlist);
    va_end (vlist);

    if (len < 0 || len >= sizeof(strbuf))
        strbuf[sizeof(strbuf) - 1] = 0;

    US_Print (strbuf);
}

///////////////////////////////////////////////////////////////////////////
//
//  US_PrintfWindow() - Prints a formatted string in a window
//
///////////////////////////////////////////////////////////////////////////

void US_PrintfWindow (const char *formatStr, ...)
{
    char    strbuf[256];
    va_list vlist;
    int     len;

    va_start (vlist,formatStr);
    len = vsnprintf(strbuf,sizeof(strbuf),formatStr,vlist);
    va_end (vlist);

    if (len < 0 || len >= sizeof(strbuf))
        strbuf[sizeof(strbuf) - 1] = 0;

    US_PrintWindow (strbuf);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_ClearWindow() - Clears the current window to white and homes the
//		cursor
//
///////////////////////////////////////////////////////////////////////////
void US_ClearWindow (void)
{
	VW_Bar (WindowX,WindowY,WindowW,WindowH,WHITE);

	PrintX = WindowX;
	PrintY = WindowY;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_DrawWindow() - Draws a frame and sets the current window parms
//
///////////////////////////////////////////////////////////////////////////
void US_DrawWindow (int x, int y, int w, int h)
{
    int	i;
    int sx,sy,sw,sh;

	WindowX = x * 8;
	WindowY = y * 8;
	WindowW = w * 8;
	WindowH = h * 8;

	PrintX = WindowX;
	PrintY = WindowY;

	sx = (x - 1) * 8;
	sy = (y - 1) * 8;
	sw = (w + 1) * 8;
	sh = (h + 1) * 8;

	US_ClearWindow ();

	VW_DrawTile8 (sx,sy,0);
	VW_DrawTile8 (sx,sy + sh,5);

	for (i = sx + 8; i <= sx + sw - 8; i += 8)
    {
		VW_DrawTile8 (i,sy,1);
		VW_DrawTile8 (i,sy + sh,6);
    }

	VW_DrawTile8 (i,sy,2);
	VW_DrawTile8 (i,sy + sh,7);

	for (i = sy + 8; i <= sy + sh - 8; i += 8)
    {
		VW_DrawTile8 (sx,i,3);
		VW_DrawTile8 (sx + sw,i,4);
    }
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CenterWindow() - Generates a window of a given width & height in the
//		middle of the screen
//
///////////////////////////////////////////////////////////////////////////
void US_CenterWindow (int w, int h)
{
	US_DrawWindow (((MaxX / 8) - w) / 2,((MaxY / 8) - h) / 2,w,h);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_SaveWindow() - Saves the current window parms into a record for
//		later restoration
//
///////////////////////////////////////////////////////////////////////////
void US_SaveWindow (WindowRec *win)
{
	win->x = WindowX;
	win->y = WindowY;
	win->w = WindowW;
	win->h = WindowH;

	win->px = PrintX;
	win->py = PrintY;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_RestoreWindow() - Sets the current window parms to those held in the
//		record
//
///////////////////////////////////////////////////////////////////////////
void US_RestoreWindow (WindowRec *win)
{
	WindowX = win->x;
	WindowY = win->y;
	WindowW = win->w;
	WindowH = win->h;

	PrintX = win->px;
	PrintY = win->py;
}

//	Input routines

///////////////////////////////////////////////////////////////////////////
//
//	USL_XORICursor() - XORs the I-bar text cursor. Used by US_LineInput()
//
///////////////////////////////////////////////////////////////////////////
static void USL_XORICursor (int x, int y, char *string, int cursor)
{
    static boolean status;      // VGA doesn't XOR...
    stringtype s;
    int        ch,oldfontcolor;

    //
    // save the char at cursor and
    // stick a null byte in
    //
	ch = string[cursor];
	string[cursor] = '\0';
	VW_MeasurePropString (string,&s,'\0');
    string[cursor] = ch;    // restore old char

	px = x + s.width - 1;
	py = y;

	if (status ^= 1)
		VW_DrawPropString ("\x80");
	else
	{
		oldfontcolor = fontcolor;
		fontcolor = backcolor;
		VW_DrawPropString ("\x80");
		fontcolor = oldfontcolor;
	}
}

char USL_RotateChar (char ch, int dir)
{
    static const char charSet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ.,-!?0123456789";
    const int numChars = sizeof(charSet) / sizeof(char) - 1;
    int i;

    for (i = 0; i < numChars; i++)
    {
        if (ch == charSet[i])
            break;
    }

    if (i == numChars)
        i = 0;

    i += dir;

    if (i < 0)
        i = numChars - 1;
    else if (i >= numChars)
        i = 0;

    return charSet[i];
}


///////////////////////////////////////////////////////////////////////////
//
//	US_LineInput() - Gets a line of user input at (x,y), the string defaults
//		to whatever is pointed at by def. Input is restricted to maxchars or
//      the width of the window from the start of input. If the user hits escape,
//      nothing is copied into buf, and 0 is returned. If the user hits return,
//      the current string is copied into buf, and the string's length is returned
//
///////////////////////////////////////////////////////////////////////////

int US_LineInput (int x, int y, char *buf, const char *def, int maxchars)
{
	boolean     cursorvis,cursormoved;
	boolean     done,checkkey;
	ScanCode	scan;
	char		*text;
	char		string[MaxString],oldstring[MaxString];
	int         cursor,inputlen;
	int  		i,temp,width;
	longword	curtime,lasttime,lastdirtime,lastbuttontime,lastdirmovetime;
	ControlInfo ci;
	stringtype  s;
	byte        lastdir = dir_None;

	if (def)
		cursor = snprintf(string,sizeof(string),"%s",def);
	else
    {
		*string = '\0';
		cursor = 0;
    }

	*oldstring = '\0';
	inputlen = -1;
	cursormoved = true;

	cursorvis = done = false;
	lasttime = lastdirtime = lastdirmovetime = GetTimeCount();
	lastbuttontime = lasttime + TickBase / 4;	// 250 ms => first button press accepted after 500 ms
	LastScan = sc_None;

	IN_ClearTextInput ();

	while (!done)
	{
		ReadAnyControl (&ci);

		if (cursorvis)
			USL_XORICursor (x,y,string,cursor);

		scan = LastScan;
		LastScan = sc_None;

		checkkey = true;
		curtime = GetTimeCount();

		//
		// after each direction change, accept the next
		// change after 250 ms and then everz 125 ms
		//
		if (ci.dir != lastdir || (curtime - lastdirtime > TickBase / 4 && curtime - lastdirmovetime > TickBase / 8))
		{
			if (ci.dir != lastdir)
			{
				lastdir = ci.dir;
				lastdirtime = curtime;
			}

            lastdirmovetime = curtime;

			switch (ci.dir)
			{
				case dir_West:
					if (cursor)
					{
					    //
						// remove trailing whitespace if cursor is at end of string
						//
						if (string[cursor] == ' ' && string[cursor + 1] == '\0')
							string[cursor] = '\0';

						cursor--;
					}

					cursormoved = true;
					checkkey = false;
					break;

				case dir_East:
					if (string[cursor] != '\0')
                        cursor++;

					cursormoved = true;
					checkkey = false;
					break;

				case dir_North:
					if (string[cursor] != '\0')
                        string[cursor] = USL_RotateChar(string[cursor],1);

					inputlen = -1;
					checkkey = false;
					break;

				case dir_South:
					if (string[cursor] != '\0')
                        string[cursor] = USL_RotateChar(string[cursor],-1);

					inputlen = -1;
					checkkey = false;
					break;
			}
		}

		if ((int)(curtime - lastbuttontime) > TickBase / 4)   // 250 ms
		{
			if (ci.button0)            // acts as return
			{
				inputlen = snprintf(buf,MaxString,"%s",string);
				done = true;
				checkkey = false;
			}

			if (ci.button1)            // acts as escape
			{
				done = true;
				checkkey = false;
			}

			if (ci.button2)            // acts as backspace
			{
				lastbuttontime = curtime;

				if (cursor)
				{
				    i = --cursor;

                    while ((string[i] = string[i + 1]) != '\0')
                        i++;

                    inputlen = -1;
				}

				cursormoved = true;
				checkkey = false;
			}
		}

		if (checkkey)
		{
			switch (scan)
			{
				case sc_LeftArrow:
					if (cursor)
						cursor--;

					cursormoved = true;
					break;

				case sc_RightArrow:
					if (string[cursor])
						cursor++;

					cursormoved = true;
					break;

				case sc_Home:
                    if (cursor > 0)
                    {
                        cursor--;

                        //
                        // delete trailing whitespace
                        //
                        while (cursor >= 0 && string[cursor] == ' ' && string[cursor + 1] == '\0')
                            string[cursor--] = '\0';

                        cursor = 0;
                    }

					cursormoved = true;
					break;

				case sc_End:
					cursor = inputlen;
					cursormoved = true;
					break;

				case sc_Return:
					inputlen = snprintf(buf,MaxString,"%s",string);
					done = true;
					break;

				case sc_Escape:
				    inputlen = 0;
                    done = true;
					break;

				case sc_BackSpace:
					if (cursor)
					{
                        i = --cursor;

                        while ((string[i] = string[i + 1]) != '\0')
                            i++;

                        inputlen = -1;
					}

					cursormoved = true;
					break;

				case sc_Delete:
					if (string[cursor])
					{
                        i = cursor;

                        while ((string[i] = string[i + 1]) != '\0')
                            i++;

                        inputlen = -1;
					}

					cursormoved = true;
					break;
			}

			if ((unsigned)maxchars > MaxString - 1)
                maxchars = MaxString - 1;

			for (text = textinput; *text; text++)
			{
				if (isprint(*text) && inputlen < maxchars)
                {
                    VW_MeasurePropString (string,&s,'\0');

                    width = fontsegs[fontnumber]->width[*text];

                    if (x + s.width + width < WindowX + WindowW)
                    {
                        for (i = inputlen + 1; i > cursor; i--)
                            string[i] = string[i - 1];

                        string[cursor++] = *text;
                        inputlen = -1;
                    }
                }
			}

			IN_ClearTextInput ();
		}

		if (inputlen < 0)
		{
            //
            // erase old string and draw new
            //
			temp = fontcolor;
			fontcolor = backcolor;

			px = x;
			py = y;
			VW_DrawPropString (oldstring);

			fontcolor = temp;

			px = x;
			py = y;
			VW_DrawPropString (string);

			inputlen = snprintf(oldstring,sizeof(oldstring),"%s",string);
		}

		if (cursormoved)
		{
			cursorvis = false;
			lasttime = curtime - TickBase;

			cursormoved = false;
		}

		if (curtime - lasttime > TickBase / 2)    // 500 ms
		{
			lasttime = curtime;

			cursorvis ^= true;
		}
		else
            SDL_Delay (5);

		if (cursorvis)
			USL_XORICursor (x,y,string,cursor);

		VW_UpdateScreen ();
	}

	if (cursorvis)
		USL_XORICursor (x,y,string,cursor);

	IN_ClearKeysDown ();

	return inputlen;
}


/*
=====================
=
= US_WindowInput
=
= Generates a window and prints a string in it,
= and waits for input at the end of the string
=
=====================
*/

int US_WindowInput (char *buf, const char *def, int maxchars)
{
    if (def)
    {
        def = US_GenerateWindowFromString(def,maxchars);

        US_Print (def);
    }
    else
        Quit ("US_WindowInput: No input string to generate window!");

    return US_LineInput(px,py,buf,NULL,maxchars);
}


///////////////////////////////////////////////////////////////////////////
//
// US_InitRndT - Initializes the pseudo random number generator.
//      If randomize is true, the seed will be initialized depending on the
//      current time
//
///////////////////////////////////////////////////////////////////////////
void US_InitRndT(int randomize)
{
    if(randomize)
        rndindex = (SDL_GetTicks() >> 4) & 0xff;
    else
        rndindex = 0;
}

///////////////////////////////////////////////////////////////////////////
//
// US_RndT - Returns the next 8-bit pseudo random number
//
///////////////////////////////////////////////////////////////////////////
int US_RndT()
{
    rndindex = (rndindex+1)&0xff;
    return rndtable[rndindex];
}
