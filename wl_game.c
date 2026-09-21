// WL_GAME.C

#include "wl_def.h"
#include "wl_cloudsky.h"
#include "wl_shade.h"

/*
=============================================================================

                             LOCAL CONSTANTS

=============================================================================
*/


/*
=============================================================================

                             GLOBAL VARIABLES

=============================================================================
*/

boolean         ingame;
gametype        gamestate;
byte            bordercol=VIEWCOLOR;        // color of the Change View/Ingame border

#ifdef SPEAR
int32_t         spearx,speary;
unsigned        spearangle;
boolean         spearflag;
#endif

#ifdef USE_FEATUREFLAGS
int ffDataTopLeft, ffDataTopRight, ffDataBottomLeft, ffDataBottomRight;
#endif

//
// ELEVATOR BACK MAPS - REMEMBER (-1)!!
//
int ElevatorBackTo[]={1,1,7,3,5,3};


/*
=============================================================================

                             LOCAL VARIABLES

=============================================================================
*/



//===========================================================================
//===========================================================================


/*
==========================
=
= SetSoundLoc - Given the location of an object (in terms of global
=       coordinates, held in globalsoundx and globalsoundy), munges the values
=       for an approximate distance from the left and right ear, and puts
=       those values into leftchannel and rightchannel.
=
= JAB
=
==========================
*/

int leftchannel, rightchannel;
#define ATABLEMAX 15
byte righttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 6, 0, 0, 0, 0, 0, 1, 3, 5, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 4, 0, 0, 0, 0, 0, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 4, 1, 0, 0, 0, 1, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 5, 4, 2, 1, 0, 1, 2, 3, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 5, 4, 3, 2, 2, 3, 3, 5, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 4, 4, 4, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 5, 5, 6, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};
byte lefttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 8, 5, 3, 1, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 0, 0, 0, 0, 0, 4, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 1, 0, 0, 0, 1, 4, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 3, 2, 1, 0, 1, 2, 4, 5, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 5, 3, 3, 2, 2, 3, 4, 5, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 5, 4, 4, 4, 4, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 6, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};

void
SetSoundLoc(fixed gx,fixed gy)
{
    fixed   xt,yt;
    int     x,y;

//
// translate point to view centered coordinates
//
    gx -= viewx;
    gy -= viewy;

//
// calculate newx
//
    xt = FixedMul(gx,viewcos);
    yt = FixedMul(gy,viewsin);
    x = (xt - yt) >> TILESHIFT;

//
// calculate newy
//
    xt = FixedMul(gx,viewsin);
    yt = FixedMul(gy,viewcos);
    y = (yt + xt) >> TILESHIFT;

    if (y >= ATABLEMAX)
        y = ATABLEMAX - 1;
    else if (y <= -ATABLEMAX)
        y = -ATABLEMAX;
    if (x < 0)
        x = -x;
    if (x >= ATABLEMAX)
        x = ATABLEMAX - 1;
    leftchannel  =  lefttable[x][y + ATABLEMAX];
    rightchannel = righttable[x][y + ATABLEMAX];

#if 0
    CenterWindow(8,1);
    US_PrintSigned(leftchannel);
    US_Print(",");
    US_PrintSigned(rightchannel);
    VW_UpdateScreen();
#endif
}

/*
==========================
=
= SetSoundLocGlobal - Sets up globalsoundx & globalsoundy and then calls
=       UpdateSoundLoc() to transform that into relative channel volumes. Those
=       values are then passed to the Sound Manager so that they'll be used for
=       the next sound played (if possible).
=
= JAB
=
==========================
*/
void PlaySoundLocGlobal(int s,fixed gx,fixed gy)
{
    SetSoundLoc(gx, gy);
    SD_PositionSound(leftchannel, rightchannel);

    int channel = SD_PlaySound(s);
    if(channel)
    {
        channelSoundPos[channel - 1].globalsoundx = gx;
        channelSoundPos[channel - 1].globalsoundy = gy;
        channelSoundPos[channel - 1].valid = 1;
    }
}

void UpdateSoundLoc(void)
{
    int i;

/*    if (SoundPositioned)
    {
        SetSoundLoc(globalsoundx,globalsoundy);
        SD_SetPosition(leftchannel,rightchannel);
    }*/

    for(i = 0; i < SD_CHANNELS; i++)
    {
        if(channelSoundPos[i].valid)
        {
            SetSoundLoc(channelSoundPos[i].globalsoundx,
                channelSoundPos[i].globalsoundy);
            SD_SetPosition(i, leftchannel, rightchannel);
        }
    }
}

/*
**      JAB End
*/

/*
==========================
=
= ScanInfoPlane
=
= Spawn all actors and mark down special places
=
==========================
*/

static void ScanInfoPlane(void)
{
    unsigned x,y;
    int      tile;
    word     *start;

    start = mapsegs[1];
    for (y=0;y<mapheight;y++)
    {
        for (x=0;x<mapwidth;x++)
        {
            tile = *start++;
            if (!tile)
                continue;

            switch (tile)
            {
                case 19:
                case 20:
                case 21:
                case 22:
                    SpawnPlayer(x,y,NORTH+tile-19);
                    break;

                case 23:
                case 24:
                case 25:
                case 26:
                case 27:
                case 28:
                case 29:
                case 30:

                case 31:
                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:

                case 39:
                case 40:
                case 41:
                case 42:
                case 43:
                case 44:
                case 45:
                case 46:

                case 47:
                case 48:
                case 49:
                case 50:
                case 51:
                case 52:
                case 53:
                case 54:

                case 55:
                case 56:
                case 57:
                case 58:
                case 59:
                case 60:
                case 61:
                case 62:

                case 63:
                case 64:
                case 65:
                case 66:
                case 67:
                case 68:
                case 69:
                case 70:
                case 71:
                case 72:
#ifdef SPEAR
                case 73:                        // TRUCK AND SPEAR!
                case 74:
#elif defined(USE_DIR3DSPR)                     // just for the example
                case 73:
#endif
                    SpawnStatic(x,y,tile-23);
                    break;

//
// P wall
//
                case 98:
                    if (!loadedgame)
                        gamestate.secrettotal++;
                    break;

//
// guard
//
                case 180:
                case 181:
                case 182:
                case 183:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 144:
                case 145:
                case 146:
                case 147:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 108:
                case 109:
                case 110:
                case 111:
                    SpawnStand(en_guard,x,y,tile-108);
                    break;


                case 184:
                case 185:
                case 186:
                case 187:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 148:
                case 149:
                case 150:
                case 151:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 112:
                case 113:
                case 114:
                case 115:
                    SpawnPatrol(en_guard,x,y,tile-112);
                    break;

                case 124:
                    SpawnDeadGuard (x,y);
                    break;
//
// officer
//
                case 188:
                case 189:
                case 190:
                case 191:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 152:
                case 153:
                case 154:
                case 155:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 116:
                case 117:
                case 118:
                case 119:
                    SpawnStand(en_officer,x,y,tile-116);
                    break;


                case 192:
                case 193:
                case 194:
                case 195:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 156:
                case 157:
                case 158:
                case 159:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 120:
                case 121:
                case 122:
                case 123:
                    SpawnPatrol(en_officer,x,y,tile-120);
                    break;


//
// ss
//
                case 198:
                case 199:
                case 200:
                case 201:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 162:
                case 163:
                case 164:
                case 165:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 126:
                case 127:
                case 128:
                case 129:
                    SpawnStand(en_ss,x,y,tile-126);
                    break;


                case 202:
                case 203:
                case 204:
                case 205:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 166:
                case 167:
                case 168:
                case 169:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 130:
                case 131:
                case 132:
                case 133:
                    SpawnPatrol(en_ss,x,y,tile-130);
                    break;

//
// dogs
//
                case 206:
                case 207:
                case 208:
                case 209:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 170:
                case 171:
                case 172:
                case 173:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 134:
                case 135:
                case 136:
                case 137:
                    SpawnStand(en_dog,x,y,tile-134);
                    break;


                case 210:
                case 211:
                case 212:
                case 213:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 36;
                case 174:
                case 175:
                case 176:
                case 177:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 36;
                case 138:
                case 139:
                case 140:
                case 141:
                    SpawnPatrol(en_dog,x,y,tile-138);
                    break;

//
// boss
//
#ifndef SPEAR
                case 214:
                    SpawnBoss (x,y);
                    break;
                case 197:
                    SpawnGretel (x,y);
                    break;
                case 215:
                    SpawnGift (x,y);
                    break;
                case 179:
                    SpawnFat (x,y);
                    break;
                case 196:
                    SpawnSchabbs (x,y);
                    break;
                case 160:
                    SpawnFakeHitler (x,y);
                    break;
                case 178:
                    SpawnHitler (x,y);
                    break;
#else
                case 106:
                    SpawnSpectre (x,y);
                    break;
                case 107:
                    SpawnAngel (x,y);
                    break;
                case 125:
                    SpawnTrans (x,y);
                    break;
                case 142:
                    SpawnUber (x,y);
                    break;
                case 143:
                    SpawnWill (x,y);
                    break;
                case 161:
                    SpawnDeath (x,y);
                    break;

#endif

//
// mutants
//
                case 252:
                case 253:
                case 254:
                case 255:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 18;
                case 234:
                case 235:
                case 236:
                case 237:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 18;
                case 216:
                case 217:
                case 218:
                case 219:
                    SpawnStand(en_mutant,x,y,tile-216);
                    break;

                case 256:
                case 257:
                case 258:
                case 259:
                    if (gamestate.difficulty<gd_hard)
                        break;
                    tile -= 18;
                case 238:
                case 239:
                case 240:
                case 241:
                    if (gamestate.difficulty<gd_medium)
                        break;
                    tile -= 18;
                case 220:
                case 221:
                case 222:
                case 223:
                    SpawnPatrol(en_mutant,x,y,tile-220);
                    break;

//
// ghosts
//
#ifndef SPEAR
                case 224:
                    SpawnGhosts (en_blinky,x,y);
                    break;
                case 225:
                    SpawnGhosts (en_clyde,x,y);
                    break;
                case 226:
                    SpawnGhosts (en_pinky,x,y);
                    break;
                case 227:
                    SpawnGhosts (en_inky,x,y);
                    break;
#endif
            }
        }
    }
}

//==========================================================================

/*
==================
=
= SetupGameLevel
=
==================
*/

void SetupGameLevel (void)
{
    int  x,y;
    int  mapnum;
    word *map;
    word tile;

    if (!loadedgame)
    {
        gamestate.TimeCount =
        gamestate.secrettotal =
        gamestate.killtotal =
        gamestate.treasuretotal =
        gamestate.secretcount =
        gamestate.killcount =
        gamestate.treasurecount =
        gamestate.attackframe =
        gamestate.attackcount =
        gamestate.weaponframe =
        pwallstate = pwallpos = facetimes = 0;
        LastAttacker = NULL;
    }

    if (demoplayback || demorecord)
        US_InitRndT (false);
    else
        US_InitRndT (true);

//
// load the level
//
    mapnum = gamestate.mapon+10*gamestate.episode;

    CA_CacheMap (mapnum);

#ifdef USE_FEATUREFLAGS
    //
    // Read feature flags data from map corners
    // and overwrite corners with adjacent tiles
    //
    map = mapsegs[0];
    ffDataTopLeft = *map;
    *map = *(map + 1);

    map = &MAPSPOT(mapwidth - 1,0,0);
    ffDataTopRight = *map;
    *map = *(map + mapwidth);

    map = &MAPSPOT(mapwidth - 1,mapheight - 1,0);
    ffDataBottomRight = *map;
    *map = *(map - 1);

    map = &MAPSPOT(0,mapheight - 1,0);
    ffDataBottomLeft = *map;
    *map = *(map - mapwidth);
#endif

//
// copy the wall data to a data segment array
//
    memset (tilemap,0,sizeof(tilemap));
    memset (actorat,0,sizeof(actorat));
#ifdef REVEALMAP
    memset (mapseen,0,sizeof(mapseen));
#endif
    map = mapsegs[0];
    for (y=0;y<mapheight;y++)
    {
        for (x=0;x<mapwidth;x++)
        {
            tile = *map++;

            if (tile < AMBUSHTILE)
            {
                // solid wall
                tilemap[x][y] = tile;
                actorat[x][y] = (objtype *)(uintptr_t) tile;
            }
        }
    }

//
// spawn doors
//
    InitActorList ();                       // start spawning things with a clean slate
    InitDoorList ();
    InitStaticList ();

    map = mapsegs[0];
    for (y=0;y<mapheight;y++)
    {
        for (x=0;x<mapwidth;x++)
        {
            tile = *map++;
            if (tile >= 90 && tile <= 101)
            {
                // door
                switch (tile)
                {
                    case 90:
                    case 92:
                    case 94:
                    case 96:
                    case 98:
                    case 100:
                        SpawnDoor (x,y,1,(tile-90)/2);
                        break;
                    case 91:
                    case 93:
                    case 95:
                    case 97:
                    case 99:
                    case 101:
                        SpawnDoor (x,y,0,(tile-91)/2);
                        break;
                }
            }
        }
    }

//
// spawn actors
//
    ScanInfoPlane ();


//
// take out the ambush markers
//
    map = mapsegs[0];
    for (y=0;y<mapheight;y++)
    {
        for (x=0;x<mapwidth;x++)
        {
            tile = *map;

            if (tile == AMBUSHTILE)
            {
                if (VALIDAREA(*(map + 1)))
                    tile = *map;
                if (VALIDAREA(*(map - mapwidth)))
                    tile = *(map - mapwidth);
                if (VALIDAREA(*(map + mapwidth)))
                    tile = *(map + mapwidth);
                if (VALIDAREA(*(map - 1)))
                    tile = *(map - 1);

                *map = tile;
            }

            map++;
        }
    }

#if defined(USE_FEATUREFLAGS) && defined(USE_CLOUDSKY)
    if (GetFeatureFlags() & FF_CLOUDSKY)
        InitSky ();
#endif

#ifdef USE_SHADING
    InitLevelShadeTable ();
#endif

//
// load floor/ceiling textures
//
#if defined(USE_FLOORCEILINGTEX) && !defined(USE_MULTIFLATS)
    GetFlatTextures ();
#endif

#ifdef USE_PARALLAX
    SetParallaxStartTexture ();
#endif
}


//==========================================================================


/*
===================
=
= DrawPlayBorderSides
=
= To fix window overwrites
=
===================
*/

void DrawPlayBorderSides (void)
{
    if (viewsize == 21)
        return;

	const int sw = screen.basewidth;
	const int sh = screen.baseheight;
	const int vw = baseviewwidth;
	const int vh = baseviewheight;

	const int h = sh - STATUSLINES;
	const int xl = (sw / 2) - (vw / 2);
	const int yl = (h - vh) / 2;

    if (xl)
    {
	    VW_Bar (0,           0, xl - 1, h, bordercol);                 // left side
	    VW_Bar (xl + vw + 1, 0, xl - 2, h, bordercol);                 // right side
    }

    if (yl)
    {
	    VW_Bar (0, 0,           sw, yl - 1, bordercol);                    // upper side
	    VW_Bar (0, yl + vh + 1, sw, yl - 1, bordercol);                    // lower side
    }

    if (xl)
    {
        // Paint game view border lines
	    VW_Bar (xl - 1,  yl - 1,  vw + 1, 1,      BLACK);                  // upper border
	    VW_Bar (xl,      yl + vh, vw + 1, 1,      bordercol - 2);          // lower border
	    VW_Bar (xl - 1,  yl - 1,  1,      vh + 1, BLACK);                  // left border
	    VW_Bar (xl + vw, yl - 1,  1,      vh + 2, bordercol - 2);          // right border
	    VW_Bar (xl - 1,  yl + vh, 1,      1,      bordercol - 3);          // lower left highlight
    }
    else
    {
        // Just paint a lower border line
        VW_Bar (0, yl + vh, vw, 1, bordercol - 2);       // lower border
    }
}


/*
===================
=
= DrawStatusBorder
=
===================
*/

void DrawStatusBorder (byte color)
{
    int sw = screen.basewidth;
    int sh = screen.baseheight;

    VW_Bar (0,0,sw,sh - (STATUSLINES - 3),color);
    VW_Bar (0,sh - (STATUSLINES - 3),8,STATUSLINES - 4,color);
    VW_Bar (0,sh - 2,sw,2,color);
    VW_Bar (sw - 8,sh - (STATUSLINES - 3),8,STATUSLINES - 4,color);

    VW_Bar (9,sh - 3,97,1,color - 1);
    VW_Bar (106,sh - 3,161,1,color - 2);
    VW_Bar (267,sh - 3,44,1,color - 3);
    VW_Bar (sw - 9,sh - (STATUSLINES - 4),1,20,color - 2);
    VW_Bar (sw - 9,sh - ((STATUSLINES / 2) - 4),1,14,color - 3);
}


/*
===================
=
= DrawPlayBorder
=
===================
*/

void DrawPlayBorder (void)
{
    const int h = screen.baseheight - STATUSLINES;
    const int vw = baseviewwidth;
    const int vh = baseviewheight;
    const int xl = (screen.basewidth / 2) - (vw / 2);
    const int yl = (h - vh) / 2;

    if (bordercol != VIEWCOLOR)
        DrawStatusBorder (bordercol);
    else
    {
        VW_Bar (0,h,8,STATUSLINES,bordercol);
        VW_Bar (screen.basewidth - 8,h,8,STATUSLINES,bordercol);
    }

    if (viewheight == screen.height)
        return;

    VW_Bar (0,0,screen.basewidth,h,bordercol);
    VW_Bar (xl,yl,vw,vh,BLACK);

    if (xl)
    {
        // Paint game view border lines
        VW_Bar (xl - 1,  yl - 1,  vw + 1, 1,      BLACK);          // upper border
        VW_Bar (xl,      yl + vh, vw + 1, 1,      bordercol - 2);  // lower border
        VW_Bar (xl - 1,  yl - 1,  1,      vh + 1, BLACK);          // left border
        VW_Bar (xl + vw, yl - 1,  1,      vh + 2, bordercol - 2);  // right border
        VW_Bar (xl - 1,  yl + vh, 1,      1,      bordercol - 3);  // lower left highlight
    }
    else
    {
        // Just paint a lower border line
        VW_Bar (0, yl + vh, vw, 1, bordercol - 2);       // lower border
    }
}


/*
===================
=
= DrawPlayScreen
=
===================
*/

void DrawPlayScreen (void)
{
    VW_DrawPic (0,screen.baseheight - STATUSLINES,STATUSBARPIC);
    DrawPlayBorder ();

    DrawFace ();
    DrawHealth ();
    DrawLives ();
    DrawLevel ();
    DrawAmmo ();
    DrawKeys ();
    DrawWeapon ();
    DrawScore ();
}


/*
===================
=
= ShowStatusBar
=
= Draw status bar without borders
=
===================
*/

void ShowStatusBar (void)
{
    const int width = pictable[STATUSBARPIC - STARTPICS].width;
    const int height = pictable[STATUSBARPIC - STARTPICS].height;

    VW_SegToScreen (grsegs[STATUSBARPIC],width,9,4,9,screen.baseheight - (height - 4),width - 18,height - 7);

    ingame = false;
    DrawFace ();
    DrawHealth ();
    DrawLives ();
    DrawLevel ();
    DrawAmmo ();
    DrawKeys ();
    DrawWeapon ();
    DrawScore ();
    ingame = true;
}


//==========================================================================

/*
==================
=
= PatchDemoChunk
=
==================
*/

//
// One demo at a time, whether it is being played or recorded - PlayDemo runs
// the title loop and RecordDemo is entered from the debug menu, never both.
//
// It used to be allocated, and in the DEMOSEXTERN build - which is this one -
// the patched copy was never freed: the free is on the other side of that
// #ifdef, because there it releases a file buffer rather than a graphics
// chunk.  So the title loop leaked up to 13 KB per demo, for ever, through
// four demos in rotation.  On a desktop that is a leak.  On a board whose
// whole heap is a few kilobytes it is a Quit() before the title screen.
//
#define MAXDEMOSIZE     16384

static int8_t DemoBuffer[MAXDEMOSIZE];

int8_t *PatchDemoChunk (int8_t *chunk)
{
    int    level,length,newlength;
    int    cx,cy,turnx,buttons;
    int8_t *newdemo,*work;

    level = *chunk++;
    length = ReadShort(chunk) - 4;
    chunk += 3;

    newlength = 8 + length + ((length / 3) * 4);

    if (newlength > (int32_t)sizeof(DemoBuffer))
        Quit ("PatchDemoChunk: demo expands to %d, the buffer is %d",
              newlength,(int)sizeof(DemoBuffer));

    newdemo = DemoBuffer;
    work = newdemo + 8;     // leave space for header

    while (length)
    {
        buttons = *chunk++;
        cx = *chunk++;
        cy = *chunk++;

        //
        // write buttons byte followed by 3 zero bytes
        //
        *work++ = buttons;
        *work++ = 0;
        *work++ = 0;
        *work++ = 0;

        //
        // if we're not strafing, swap turnx with cx
        //
        if (buttons & (1 << bt_strafe))
            turnx = 0;
        else
        {
            turnx = cx;
            cx = 0;
        }

        *work++ = cx;
        *work++ = cy;
        *work++ = turnx;

        length -= 3;
    }

    if ((int)work - (int)newdemo != newlength)
        Quit ("New demo length does not match!");

    work = newdemo;

    //
    // header:
    // 4 zero bytes to mark a new format
    // 1 byte for the level
    // 2 bytes for the length
    // 1 unused zero byte
    //
    *work++ = 0;
    *work++ = 0;
    *work++ = 0;
    *work++ = 0;
    *work++ = level;
    *work++ = (int8_t)newlength;
    *work++ = (int8_t)(newlength >> 8);
    *work++ = 0;

    return newdemo;
}


char    demoname[13] = "DEMO?.";

#ifndef REMDEBUG

/*
==================
=
= FinishDemoRecord
=
==================
*/

void FinishDemoRecord (int8_t *demobuffer)
{
    int    length,level;

    length = (int)demoptr - (int)demobuffer;

    demoptr = demobuffer + 5;
    *demoptr++ = (int8_t)length;
    *demoptr++ = (int8_t)(length >> 8);

    CenterWindow(24,3);
    PrintY+=6;
    fontnumber=0;
    SETFONTCOLOR(0,15);
    US_Print(" Demo number (0-9): ");
    VW_UpdateScreen();

    if (US_LineInput (px,py,str,NULL,true,1,0))
    {
        level = atoi (str);
        if (level>=0 && level<=9)
        {
            demoname[4] = (char)('0'+level);
            CA_WriteFile (demoname,demobuffer,length);
        }
    }
}

//==========================================================================

/*
==================
=
= RecordDemo
=
= Fades the screen out, then starts a demo.  Exits with the screen faded
=
==================
*/

void RecordDemo (void)
{
    int  level,maps;
    void *demobuffer;
    char buf[128];

    CenterWindow(26,3);
    PrintY+=6;
    fontnumber=0;
    SETFONTCOLOR(0,15);
#ifndef SPEAR
#ifdef UPLOAD
    maps = 10;
#else
    maps = 60;
#endif
#else
    maps = 21;
#endif
    VW_UpdateScreen();
    VW_FadeIn ();

    snprintf (buf,sizeof(buf),"  Demo which level(1-%d): ",maps);

    if (!US_LineInput(px,py,str,NULL,true,2,0))
        return;

    level = atoi (str);
    level--;

    if (level >= maps || level < 0)
        return;

    VW_FadeOut ();

#ifndef SPEAR
    NewGame (gd_hard,level/10);
    gamestate.mapon = level%10;
#else
    NewGame (gd_hard,0);
    gamestate.mapon = level;
#endif

    demobuffer = DemoBuffer;
    demoptr = demobuffer;
    lastdemoptr = &demoptr[MAXDEMOSIZE - 7];

    *demoptr++ = 0;
    *demoptr++ = 0;
    *demoptr++ = 0;
    *demoptr++ = 0;
    *demoptr++ = level;
    demoptr += 2;           // leave space for length
    *demoptr++ = 0;

    VW_SetBufferOffset (0);
    DrawPlayScreen ();
    VW_FadeIn ();

    startgame = false;
    demorecord = true;

    SetupGameLevel ();
    StartMusic ();

    screen.flags |= SC_FIZZLEIN;

    PlayLoop ();

    demorecord = false;

    StopMusic ();
    ClearMemory ();

    FinishDemoRecord (demobuffer);

    demobuffer = NULL;              // static storage; nothing to give back

    demoptr = NULL;
}
#else
void FinishDemoRecord (void) {return;}
void RecordDemo (void) {return;}
#endif



//==========================================================================

/*
==================
=
= PlayDemo
=
= Fades the screen out, then starts a demo.  Exits with the screen unfaded
=
==================
*/

int PlayDemo (int demonumber)
{
    int  i,length;
    void *demobuffer;
    FILE *file;
#ifdef DEMOSEXTERN
// debug: load chunk
#ifndef SPEARDEMO
    int dems[NUMDEMOS]={T_DEMO0,T_DEMO1,T_DEMO2,T_DEMO3};
#else
    int dems[1]={T_DEMO0};
#endif
    demobuffer = grsegs[dems[demonumber]];
#else
    for (i = 0; i < MAXDEMOS; i++)
    {
        demoname[4] = '0' + demonumber;

        file = fopen(demoname,"rb");

        if (file)
            break;

        if (++demonumber >= MAXDEMOS)
            demonumber = 0;
    }

    if (!file)
        return -1;      // no demo files

    length = CA_GetFileLength(file);

    demobuffer = SafeMalloc(length);

    fread (demobuffer,length,1,file);
    fclose (file);
#endif
    //
    // is this an old demo chunk?
    //
    if (ReadLong(demobuffer))
    {
        demoptr = demobuffer;     // save pointer so it can be freed

        demobuffer = PatchDemoChunk(demobuffer);
#ifndef DEMOSEXTERN
        SafeFree (demoptr);
#endif
    }

    demoptr = (int8_t *)demobuffer + 4;

    NewGame (1,0);
    gamestate.mapon = *demoptr++;
    gamestate.difficulty = gd_hard;
    length = ReadShort(demoptr);
    demoptr += 3;
    lastdemoptr = (int8_t *)demobuffer + length;

    VW_FadeOut ();

    SETFONTCOLOR(0,15);
    VW_SetBufferOffset (0);
    DrawPlayScreen ();

    startgame = false;
    demoplayback = true;

    SetupGameLevel ();
    StartMusic ();

    PlayLoop ();

#ifndef DEMOSEXTERN
    SafeFree (demobuffer);
#endif
    demoptr = NULL;

    demoplayback = false;

    StopMusic ();
    ClearMemory ();

    return demonumber + 1;
}

//==========================================================================

/*
==================
=
= Died
=
==================
*/

#define DEATHROTATE 2

void Died (void)
{
    float   fangle;
    int32_t dx,dy;
    int     iangle,curangle,clockwise,counter,change;

    if (screen.flags & SC_FADED)
    {
        ThreeDRefresh ();
        VW_FadeIn ();
    }

    gamestate.weapon = -1;                     // take away weapon
    SD_PlaySound (PLAYERDEATHSND);

    //
    // swing around to face attacker
    //
    if (LastAttacker)
    {
        dx = LastAttacker->x - player->x;
        dy = player->y - LastAttacker->y;

        fangle = (float) atan2((float) dy, (float) dx);     // returns -pi to pi
        if (fangle<0)
            fangle = (float) (M_PI*2+fangle);

        iangle = (int) (fangle/(M_PI*2)*ANGLES);
    }
    else
    {
        iangle = player->angle + ANGLES / 2;
        if(iangle >= ANGLES) iangle -= ANGLES;
    }

    if (player->angle > iangle)
    {
        counter = player->angle - iangle;
        clockwise = ANGLES-player->angle + iangle;
    }
    else
    {
        clockwise = iangle - player->angle;
        counter = player->angle + ANGLES-iangle;
    }

    curangle = player->angle;

    if (clockwise<counter)
    {
        //
        // rotate clockwise
        //
        if (curangle>iangle)
            curangle -= ANGLES;
        do
        {
            change = tics*DEATHROTATE;
            if (curangle + change > iangle)
                change = iangle-curangle;

            curangle += change;
            player->angle += change;
            if (player->angle >= ANGLES)
                player->angle -= ANGLES;

            ThreeDRefresh ();
            CalcTics ();
        } while (curangle != iangle);
    }
    else
    {
        //
        // rotate counterclockwise
        //
        if (curangle<iangle)
            curangle += ANGLES;
        do
        {
            change = -(int)tics*DEATHROTATE;
            if (curangle + change < iangle)
                change = iangle-curangle;

            curangle += change;
            player->angle += change;
            if (player->angle < 0)
                player->angle += ANGLES;

            ThreeDRefresh ();
            CalcTics ();
        } while (curangle != iangle);
    }

    //
    // fade to red
    //
    FinishPaletteShifts ();

    VW_FizzleFade (baseviewscreenx,baseviewscreeny,baseviewwidth,baseviewheight,4,70,false);

    IN_UserInput(100);
    SD_WaitSoundDone ();
    ClearMemory();

    gamestate.lives--;

    if (gamestate.lives > -1)
    {
        gamestate.health = 100;
        gamestate.weapon = gamestate.bestweapon
            = gamestate.chosenweapon = wp_pistol;
        gamestate.ammo = STARTAMMO;
        gamestate.keys = 0;
        pwallstate = pwallpos = 0;
        gamestate.attackframe = gamestate.attackcount =
            gamestate.weaponframe = 0;

        if(viewsize != 21)
        {
            DrawKeys ();
            DrawWeapon ();
            DrawAmmo ();
            DrawHealth ();
            DrawFace ();
            DrawLives ();
        }
    }
}

//==========================================================================

/*
===================
=
= GameLoop
=
===================
*/

void GameLoop (void)
{
    boolean died;
#ifdef MYPROFILE
    clock_t start,end;
#endif

    ClearMemory ();
    SETFONTCOLOR(0,15);
    VW_FadeOut();
    VW_SetBufferOffset (0);    // draw a full screen while in the 3D renderer
    DrawPlayScreen ();
    died = false;
    do
    {
        if (!loadedgame)
            gamestate.score = gamestate.oldscore;
        if(!died || viewsize != 21) DrawScore();

        startgame = false;
        if (!loadedgame)
            SetupGameLevel ();

#ifdef SPEAR
        if (gamestate.mapon == 20)      // give them the key allways
        {
            gamestate.keys |= 1;
            DrawKeys ();
        }
#endif

        DrawLevel ();                        // ADDEDFIX 5 -  Chris Chokan

        ingame = true;
        if(loadedgame)
        {
            ContinueMusic(lastgamemusicoffset);
            loadedgame = false;
        }
        else StartMusic ();

        if (!died)
            PreloadGraphics ();             // TODO: Let this do something useful!
        else
        {
            died = false;
            screen.flags |= SC_FIZZLEIN;
        }

//        DrawLevel ();                     // ADDEDFIX 5 - moved up  Chris Chokan

        PlayLoop ();

#ifdef SPEAR
        while (spearflag)
        {
            SD_StopSound();
            SD_PlaySound(GETSPEARSND);
            if (DigiMode != sds_Off)
            {
                Delay(150);
            }
            else
                SD_WaitSoundDone();

            ClearMemory ();
            gamestate.oldscore = gamestate.score;
            gamestate.mapon = 20;
            SetupGameLevel ();
            StartMusic ();
            player->x = spearx;
            player->y = speary;
            player->angle = (short)spearangle;
            spearflag = false;
            Thrust (0,0);
            PlayLoop ();
        }
#endif

        StopMusic ();
        ingame = false;

        if (startgame || loadedgame)
        {
            ClearMemory ();
            SETFONTCOLOR(0,15);
            VW_FadeOut();
            DrawPlayScreen ();
            died = false;
            continue;
        }

        switch (playstate)
        {
            case ex_completed:
            case ex_secretlevel:
                if(viewsize == 21) DrawPlayScreen();
                gamestate.keys = 0;
                DrawKeys ();
                VW_FadeOut ();

                ClearMemory ();

                LevelCompleted ();              // do the intermission
                if(viewsize == 21) DrawPlayScreen();

#ifdef SPEARDEMO
                if (gamestate.mapon == 1)
                {
                    died = true;                    // don't "get psyched!"

                    VW_FadeOut ();

                    ClearMemory ();

                    CheckHighScore (gamestate.score,gamestate.mapon+1);
#ifndef JAPAN
                    snprintf (MainMenu[viewscores].string,sizeof(MainMenu[viewscores].string),STR_VS);
#endif
                    MainMenu[viewscores].routine = CP_ViewScores;
                    return;
                }
#endif

#ifdef JAPDEMO
                if (gamestate.mapon == 3)
                {
                    died = true;                    // don't "get psyched!"

                    VW_FadeOut ();

                    ClearMemory ();

                    CheckHighScore (gamestate.score,gamestate.mapon+1);
#ifndef JAPAN
                    snprintf (MainMenu[viewscores].string,sizeof(MainMenu[viewscores].string),STR_VS);
#endif
                    MainMenu[viewscores].routine = CP_ViewScores;
                    return;
                }
#endif

                gamestate.oldscore = gamestate.score;

#ifndef SPEAR
                //
                // COMING BACK FROM SECRET LEVEL
                //
                if (gamestate.mapon == 9)
                    gamestate.mapon = ElevatorBackTo[gamestate.episode];    // back from secret
                else
                    //
                    // GOING TO SECRET LEVEL
                    //
                    if (playstate == ex_secretlevel)
                        gamestate.mapon = 9;
#else

#define FROMSECRET1             3
#define FROMSECRET2             11

                //
                // GOING TO SECRET LEVEL
                //
                if (playstate == ex_secretlevel)
                    switch(gamestate.mapon)
                {
                    case FROMSECRET1: gamestate.mapon = 18; break;
                    case FROMSECRET2: gamestate.mapon = 19; break;
                }
                else
                    //
                    // COMING BACK FROM SECRET LEVEL
                    //
                    if (gamestate.mapon == 18 || gamestate.mapon == 19)
                        switch(gamestate.mapon)
                    {
                        case 18: gamestate.mapon = FROMSECRET1+1; break;
                        case 19: gamestate.mapon = FROMSECRET2+1; break;
                    }
#endif
                    else
                        //
                        // GOING TO NEXT LEVEL
                        //
                        gamestate.mapon++;
                break;

            case ex_died:
                Died ();
                died = true;                    // don't "get psyched!"

                if (gamestate.lives > -1)
                    break;                          // more lives left

                VW_FadeOut ();
                ClearMenuBorders ();

#ifdef _arch_dreamcast
                DC_StatusClearLCD();
#endif

                ClearMemory ();

                CheckHighScore (gamestate.score,gamestate.mapon+1);
#ifndef JAPAN
                snprintf (MainMenu[viewscores].string,sizeof(MainMenu[viewscores].string),STR_VS);
#endif
                MainMenu[viewscores].routine = CP_ViewScores;
                return;

            case ex_victorious:
                if(viewsize == 21) DrawPlayScreen();
#ifndef SPEAR
                VW_FadeOut ();
#else
                VW_FadePaletteOut (0,17,17,300);
#endif
                ClearMemory ();

                Victory ();

                ClearMemory ();

                CheckHighScore (gamestate.score,gamestate.mapon+1);
#ifndef JAPAN
                snprintf (MainMenu[viewscores].string,sizeof(MainMenu[viewscores].string),STR_VS);
#endif
                MainMenu[viewscores].routine = CP_ViewScores;
                return;

            default:
                if(viewsize == 21) DrawPlayScreen();
                ClearMemory ();
                break;
        }
    } while (1);
}
