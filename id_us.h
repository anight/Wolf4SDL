//
//	ID Engine
//	ID_US.h - Header file for the User Manager
//	v1.0d1
//	By Jason Blochowiak
//

#ifndef	__ID_US_H_
#define	__ID_US_H_

#ifdef	__DEBUG__
#define	__DEBUG_UserMgr__
#endif

//#define	HELPTEXTLINKED

#define	MaxX	320
#define	MaxY	160

#define	MaxHelpLines	500

#define	MaxHighName	57
#define	MaxScores	7
typedef	struct
{
    char	name[MaxHighName + 1];
    int32_t	score;
    word	completed,episode;
} HighScore;

#define	MaxGameName		32
#define	MaxString	128	// Maximum input string size

typedef	struct
{
    int	x,y;
    int w,h;
    int px,py;
} WindowRec;	// Record used to save & restore screen windows

extern  boolean ingame;		        // Set by game code if a game is in progress
extern  int     PrintX,PrintY;	    // Current printing location in the window
extern  int     WindowX,WindowY;    // Current location of window
extern  int     WindowW,WindowH;    // Current size of window

extern  HighScore   Scores[MaxScores];


void            US_Startup (void);
void            US_Shutdown (void);
void            US_DrawWindow (int x, int y, int w, int h);
void            US_CenterWindow (int w, int h);
void            US_SaveWindow (WindowRec *win);
void            US_RestoreWindow (WindowRec *win);
void            US_ClearWindow (void);
const char      *US_GenerateWindowFromString (const char *string, int maxchars);
void            US_Print (const char *string);
void            US_PrintWindow (const char *string);
void            US_Printf (const char *formatStr, ...);
void            US_PrintfWindow (const char *formatStr, ...);
int             US_WindowInput (char *buf, const char *def, int maxchars);
int             US_LineInput (int x, int y, char *buf, const char *def, int maxchars);

void            US_InitRndT (int randomize);
int             US_RndT (void);

#endif
