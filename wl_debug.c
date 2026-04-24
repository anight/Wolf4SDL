// WL_DEBUG.C

#ifdef _WIN32
	#include <io.h>
#else
	#include <unistd.h>
#endif

#include "wl_def.h"

#ifdef USE_CLOUDSKY
#include "wl_cloudsky.h"
#endif

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

#ifdef DEBUGKEYS

int DebugKeys (void);


/*
==================
=
= CountObjects
=
==================
*/

void CountObjects (void)
{
    int     i;
    int     totalstatics,staticsactive;
    int     actorsactive,actorsinactive;
    objtype *obj;

    actorsactive = actorsinactive = 0;

    for (obj = player->next; obj; obj = obj->next)
    {
        if (obj->active)
            actorsactive++;
        else
            actorsinactive++;
    }

    totalstatics = (int)(laststatobj - &statobjlist[0]);
    staticsactive = 0;

    for (i = 0; i < totalstatics; i++)
    {
        if (statobjlist[i].shapenum != -1)
            staticsactive++;
    }

    US_PrintfWindow (
        "Total statics :%d"
        "\nlaststatobj=%p"
        "\nIn use statics:%d"
        "\nDoors         :%d"
        "\nTotal actors  :%d"
        "\nActive actors :%d",
        totalstatics,
        laststatobj,
        staticsactive,
        doornum,
        actorsactive + actorsinactive,
        actorsactive);

    VW_UpdateScreen();
    IN_Ack ();
}


//===========================================================================

/*
===================
=
= PictureGrabber
=
===================
*/

void PictureGrabber (void)
{
    int i;
    FILE *file;
    static char fname[] = "WSHOT000.BMP";

    for(i = 0; i < 1000; i++)
    {
        fname[7] = i % 10 + '0';
        fname[6] = (i / 10) % 10 + '0';
        fname[5] = i / 100 + '0';

        file = fopen(fname,"rb");

        if (!file)
            break;       // file does not exist, so use that filename

        fclose (file);
    }

    // overwrites WSHOT999.BMP if all wshot files exist

    SDL_SaveBMP(screen.buffer, fname);

    US_PrintWindow ("\v\tScreenshot taken");
    VW_UpdateScreen();
    IN_Ack();
}


#ifndef VIEWMAP

/*
===================
=
= BasicOverhead
=
===================
*/

void BasicOverhead (void)
{
    int       x,y;
    int       zoom,temp;
    int       offx,offy;
    uintptr_t tile;
    unsigned  offset;
    int       color;

    //
    // KS: this may not always be correct as it
    // assumes the maps are square and might look
    // bad for maps larger than 128x128
    //
    zoom = 128 / mapwidth;
    offx = 160;
    offy = (160 - (mapwidth * zoom)) / 2;

#ifdef MAPBORDER
    temp = viewsize;
    NewViewSize (16);
    DrawPlayBorder ();
#endif

    //
    // right side (raw)
    //
    for (y = 0; y < mapheight; y++)
    {
        for (x = 0; x < mapwidth; x++)
        {
            tile = (uintptr_t)actorat[mapylookup[y] + x];

            VW_Bar ((x * zoom) + offx,(y * zoom) + offy,zoom,zoom,tile);
        }
    }

    //
    // left side (filtered)
    //
    offx -= 128;

    for (y = 0; y < mapheight; y++)
    {
        for (x = 0; x < mapwidth; x++)
        {
            offset = mapylookup[y] + x;

            tile = (uintptr_t)actorat[offset];

            if (ISPOINTER(tile) && ((objtype *)tile)->flags & FL_SHOOTABLE)
                color = 72;
            else if (!tile || ISPOINTER(tile))
            {
                if (spotvis[offset])
                    color = 111;
                else
                    color = 0;      // nothing
            }
            else if (mapsegs[1][offset] == PUSHABLETILE)
                color = 171;
            else if (tile == BIT_WALL)
                color = 158;
            else if (tile < BIT_DOOR)
                color = 154;
            else if (tile < BIT_ALLTILES)
                color = 146;

            VW_Bar ((x * zoom) + offx,(y * zoom) + offy,zoom,zoom,color);
        }
    }

    VW_Bar ((player->tilex * zoom) + offx,(player->tiley * zoom) + offy,zoom,zoom,15);

    VW_UpdateScreen ();
    IN_Ack ();

#ifdef MAPBORDER
    NewViewSize (temp);
    DrawPlayBorder ();
#endif
}

#endif


/*
================
=
= ShapeTest
=
================
*/

void ShapeTest (void)
{
    boolean    done;
    ScanCode   scan;
    int        i,j,k,x;
    int        v2;
    int        oldviewheight;
    longword   l;
    byte       v;
    byte       *addr;
    int        sound;

    US_CenterWindow (20,16);

    i = 0;
    done = false;

    while (!done)
    {
        US_ClearWindow ();
        sound = -1;

        US_Printf (" Page #%d",i);

        if (i < PMSpriteStart)
            US_Print (" (Wall)");
        else if (i < PMSoundStart)
            US_Print (" (Sprite)");
        else if (i == ChunksInFile - 1)
            US_Print (" (Sound Info)");
        else
            US_Print (" (Sound)");

        addr = PM_GetPage(i);

        US_Printf ("\n Address: %p",addr);

        if (addr)
        {
            if (i < PMSpriteStart)
            {
                //
                // draw the wall
                //
                vbuf = VW_LockSurface(screen.buffer);

                if (!vbuf)
                    Quit ("ShapeTest: Unable to create surface for walls!");

                postx = (screen.width / 2) - ((TEXTURESIZE / 2) * screen.scale);
                postsource = addr;

                centery = screen.height / 2;
                oldviewheight = viewheight;
                viewheight = 0x7fff;            // quick hack to skip clipping

                for (x = 0, j = 0; x < TEXTURESIZE * screen.scale; x++, j++, postx++)
                {
                    wallheight[postx] = 256 * screen.scale;
                    ScalePost ();

                    if (j == screen.scale)
                    {
                        j = 0;
                        postsource += TEXTURESIZE;
                    }
                }

                viewheight = oldviewheight;
                centery = viewheight / 2;

                VW_UnlockSurface (screen.buffer);
                vbuf = NULL;
            }
            else if (i < PMSoundStart)
            {
                //
                // draw the sprite
                //
                vbuf = VW_LockSurface(screen.buffer);

                if (!vbuf)
                    Quit ("ShapeTest: Unable to create surface for sprites!");

                centery = screen.height / 2;
                oldviewheight = viewheight;
                viewheight = 0x7fff;            // quick hack to skip clipping

                SimpleScaleShape (screen.width / 2,i - PMSpriteStart,64 * screen.scale);

                viewheight = oldviewheight;
                centery = viewheight / 2;

                VW_UnlockSurface(screen.buffer);
                vbuf = NULL;
            }
            else if (i == ChunksInFile - 1)
            {
                //
                // display sound info
                //
                US_Printf ("\n\n Number of sounds: %d",NumDigi);

				for (l = j = 0; j < NumDigi; j++)
					l += DigiList[j].length;

                US_Printf ("\n Total bytes: %d",l);
                US_Printf ("\n Total pages: %d",ChunksInFile - PMSoundStart - 1);
            }
            else
            {
                //
                // display sounds
                //
                for (j = 0; j < NumDigi; j++)
                {
                    if (j == NumDigi - 1)
                        k = ChunksInFile - 1;    // don't let it overflow
                    else
                        k = DigiList[j + 1].startpage;

                    if (i >= PMSoundStart + DigiList[j].startpage && i < PMSoundStart + k)
                        break;
                }

                if (j < NumDigi)
                {
                    sound = j;

                    US_Printf ("\n Sound #%d",j);
                    US_Printf ("\n Segment #%d",i - PMSoundStart - DigiList[j].startpage);
                }

                for (j = 0; j < pageLengths[i]; j += 32)
                {
                    v = addr[j];
                    v2 = (unsigned)v;
                    v2 -= 128;
                    v2 /= 4;

                    if (v2 < 0)
                        VW_Vlin (WindowY + WindowH - 32 + v2,
                                  WindowY + WindowH - 32,
                                  WindowX + 8 + (j / 32),BLACK);
                    else
                        VW_Vlin (WindowY + WindowH - 32,
                                  WindowY + WindowH - 32 + v2,
                                  WindowX + 8 + (j / 32),BLACK);
                }
            }
        }

        VW_UpdateScreen();

        IN_Ack ();
        scan = LastScan;

        IN_ClearKey (scan);

        switch (scan)
        {
            case sc_LeftArrow:
                if (i)
                    i--;
                break;

            case sc_RightArrow:
                if (++i >= ChunksInFile)
                    i--;
                break;

            case sc_W:      // Walls
                i = 0;
                break;

            case sc_S:      // Sprites
                i = PMSpriteStart;
                break;

            case sc_D:      // Digitized
                i = PMSoundStart;
                break;

            case sc_I:      // Digitized info
                i = ChunksInFile - 1;
                break;

            case sc_P:
                if (sound != -1)
                    SD_PlayDigitized (sound,8,8);
                break;

            case sc_Escape:
                done = true;
                break;
        }
    }

    SD_StopDigitized ();
}


//===========================================================================


/*
================
=
= DebugKeys
=
================
*/

int DebugKeys (void)
{
    int level;
    objtype *spot;
    unsigned offset;
    const char *isOn[] = {"OFF","ON"};

    if (Keyboard[sc_B])             // B = border color
    {
        if (US_WindowInput(str,"\v\t Border color (0-56): ",2))
        {
            level = atoi (str);
            if (level>=0 && level<=99)
            {
                if (level<30) level += 31;
                else
                {
                    if (level > 56) level=31;
                    else level -= 26;
                }

                bordercol=level*4+3;

                if (bordercol == VIEWCOLOR)
                    DrawStatusBorder(bordercol);

                return 1;
            }
        }
        return 1;
    }
    if (Keyboard[sc_C])             // C = count objects
    {
        CountObjects();
        return 1;
    }
    if (Keyboard[sc_D])             // D = Darkone's FPS counter
    {
        fpscounter ^= 1;

        US_PrintfWindow ("\v\tDarkone's FPS Counter %s",isOn[fpscounter != 0]);
        VW_UpdateScreen();
        IN_Ack();

        return 1;
    }
    if (Keyboard[sc_E])             // E = quit level
        playstate = ex_completed;

    if (Keyboard[sc_F])             // F = facing spot
    {
        offset = mapylookup[player->tiley] + player->tilex;

        spot = actorat[offset];

        US_PrintfWindow (
            "X: %d (%d)\n"
            "Y: %d (%d)\n"
            "A: %d\n"
            "TileX: %u\n"
            "TileY: %u\n"
            "1: %u"
            " 2:%p\n"
            "f 1: %u"
            " 2: %u"
            " 3: %u\n",
            player->x,
            player->x % TILEGLOBAL,
            player->y,
            player->y % TILEGLOBAL,
            player->angle,
            player->tilex,
            player->tiley,
            tilemap[offset],
            spot,
            player->areanumber,
            mapsegs[1][offset],
            !ISPOINTER(spot) ? spotvis[offset] : spot->flags);

        VW_UpdateScreen();
        IN_Ack();
        return 1;
    }

    if (Keyboard[sc_G])             // G = god mode
    {
        if (godmode != 2)
            godmode++;
        else
            godmode = 0;

        if (godmode < 2)
            US_PrintfWindow ("\v\tGod mode %s",isOn[godmode != 0]);
        else
            US_PrintfWindow ("\v\tGod (no flash)");

        VW_UpdateScreen();
        IN_Ack();

        return 1;
    }
    if (Keyboard[sc_H])             // H = hurt self
    {
        IN_ClearKeysDown ();
        TakeDamage (16,NULL);
    }
    else if (Keyboard[sc_I])        // I = item cheat
    {
        US_PrintWindow ("\v\tFree items!");
        VW_UpdateScreen();
        GivePoints (100000);
        HealSelf (99);
        if (gamestate.bestweapon<wp_chaingun)
            GiveWeapon (gamestate.bestweapon+1);
        gamestate.ammo += 50;
        if (gamestate.ammo > 99)
            gamestate.ammo = 99;
        DrawAmmo ();
        IN_Ack ();
        return 1;
    }
    else if (Keyboard[sc_K])        // K = give keys
    {
        if (US_WindowInput(str,"\v\t  Give Key (1-4): ",1))
        {
            level = atoi (str);
            if (level>0 && level<5)
                GiveKey(level-1);
        }
        return 1;
    }
    else if (Keyboard[sc_L])        // L = level ratios
    {
        //
        // KS: make a window like ShapeTest for this
        //
        level = gamestate.mapon;

        if (level >= LRpack)
            level = LRpack - 1;

        US_PrintfWindow (
            "\v\t%2d %02d:%02d "
            "%3d%% %3d%% %3d%% ",
            level + 1,
            LevelRatios[level].time / 60,
            LevelRatios[level].time % 60,
            LevelRatios[level].kill,
            LevelRatios[level].secret,
            LevelRatios[level].treasure);

        VW_UpdateScreen();
        IN_Ack();

        return 1;
    }
#ifdef REVEALMAP
    else if (Keyboard[sc_M])        // M = Map reveal
    {
        mapreveal ^= true;

        US_PrintfWindow ("\v\tMap reveal %s",isOn[mapreveal != 0]);

        VW_UpdateScreen();
        IN_Ack ();
        return 1;
    }
#endif
    else if (Keyboard[sc_N])        // N = no clip
    {
        noclip ^= 1;

        US_PrintfWindow ("\v\tNo clipping %s",isOn[noclip != 0]);

        VW_UpdateScreen();
        IN_Ack ();
        return 1;
    }
#ifndef VIEWMAP
    else if (Keyboard[sc_O])        // O = basic overhead
    {
        BasicOverhead();
        return 1;
    }
#endif
    else if(Keyboard[sc_P])         // P = Ripper's picture grabber
    {
        PictureGrabber();
        return 1;
    }
    else if (Keyboard[sc_Q])        // Q = fast quit
        Quit (NULL);
    else if (Keyboard[sc_S])        // S = slow motion
    {
        if (US_WindowInput(str,"\v\t Slow Motion steps (0-50): ",2))
        {
            level = atoi (str);
            if (level>=0 && level<=50)
                singlestep = level;
        }

        return 1;
    }
    else if (Keyboard[sc_T])        // T = shape test
    {
        ShapeTest ();
        return 1;
    }
    else if (Keyboard[sc_V])        // V = extra VBLs
    {
        if (US_WindowInput(str,"\v\t  Add how many extra VBLs(0-8): ",1))
        {
            level = atoi (str);
            if (level>=0 && level<=8)
                extravbls = level;
        }
        return 1;
    }
    else if (Keyboard[sc_W])        // W = warp to level
    {
#ifndef SPEAR
        if (US_WindowInput(str,"\v\tWarp to which level(1-10): ",2))
#else
        if (US_WindowInput(str,"\v\tWarp to which level(1-21): ",2))
#endif
        {
            level = atoi (str);
#ifndef SPEAR
            if (level>0 && level<11)
#else
            if (level>0 && level<22)
#endif
            {
                gamestate.mapon = level-1;
                playstate = ex_warped;
            }
        }
        return 1;
    }
    else if (Keyboard[sc_X])        // X = item cheat
    {
        US_PrintWindow ("\v\tExtra stuff!");
        VW_UpdateScreen();
        // DEBUG: put stuff here
        IN_Ack ();
        return 1;
    }
#ifdef USE_CLOUDSKY
    else if(Keyboard[sc_Z] && curSky)
    {
        char defstr[15];
// KS: sort out this mess...
        US_Print("  Recalculate sky with seed: ");
        int seedpx = px, seedpy = py;
        US_PrintUnsigned(curSky->seed);
        US_Print("\n  Use color map (0-");
        US_PrintUnsigned(numColorMaps - 1);
        US_Print("): ");
        int mappx = px, mappy = py;
        US_PrintUnsigned(curSky->colorMapIndex);
        VW_UpdateScreen();

        snprintf (defstr,sizeof(defstr), "%u", curSky->seed);
        esc = !US_LineInput(seedpx, seedpy, str, defstr, true, 10, 0);
        if(esc) return 1;
        curSky->seed = (uint32_t) atoi(str);

        snprintf (defstr,sizeof(defstr), "%u", curSky->colorMapIndex);
        esc = !US_LineInput(mappx, mappy, str, defstr, true, 10, 0);
        if(esc) return 1;
        uint32_t newInd = (uint32_t) atoi(str);
        if(newInd < (uint32_t) numColorMaps)
        {
            curSky->colorMapIndex = newInd;
            InitSky();
        }
        else
        {
            US_PrintWindow ("\v\tIllegal color map!");
            VW_UpdateScreen();
            IN_Ack ();
        }
    }
#endif

    return 0;
}

#endif

/*
=============================================================================

                                 OVERHEAD MAP

=============================================================================
*/

#ifdef VIEWMAP

#define COL_FLOOR   0x19                // empty area color
#define COL_SECRET  WHITE               // pushwall color


int16_t maporgx,maporgy;
int16_t viewtilex,viewtiley;
int16_t tilemapratio,tilewallratio;
int16_t tilesize;


/*
===================
=
= DrawMapFloor
=
===================
*/

void DrawMapFloor (int16_t sx, int16_t sy, byte color)
{
    int x,y;

    for (x = 0; x < tilesize; x++)
    {
        for (y = 0; y < tilesize; y++)
            *(vbuf + ylookup[sy + y] + (sx + x)) = color;
    }
}


/*
===================
=
= DrawMapWall
=
===================
*/

void DrawMapWall (int16_t sx, int16_t sy, int16_t wallpic)
{
    int  x,y;
    byte *src;
    word texturemask;

    src = PM_GetPage(wallpic);

    texturemask = TEXTURESIZE * (tilewallratio - 1);

    for (x = 0; x < tilesize; x++, src += texturemask)
    {
        for (y = 0; y < tilesize; y++, src += tilewallratio)
            *(vbuf + ylookup[sy + y] + (sx + x)) = *src;
    }
}


/*
===================
=
= DrawMapDoor
=
===================
*/

void DrawMapDoor (int16_t sx, int16_t sy, int16_t doornum)
{
    int doorpage;

    switch (doorobjlist[doornum].lock)
    {
        case dr_normal:
            doorpage = DOORWALL;
            break;

        case dr_lock1:
        case dr_lock2:
        case dr_lock3:
        case dr_lock4:
            doorpage = DOORWALL + 6;
            break;

        case dr_elevator:
            doorpage = DOORWALL + 4;
            break;
    }

    DrawMapWall (sx,sy,doorpage);
}


/*
===================
=
= DrawMapSprite
=
===================
*/

void DrawMapSprite (int16_t sx, int16_t sy, int16_t shapenum)
{
    int         x;
    compshape_t *shape;
    byte        *linesrc,*linecmds;
    byte        *src;
    int16_t     end,start,top;

    linesrc = PM_GetSpritePage(shapenum);
    shape = (compshape_t *)linesrc;

    for (x = shape->leftpix; x <= shape->rightpix; x++)
    {
        //
        // reconstruct sprite and draw it
        //
        linecmds = &linesrc[shape->dataofs[x - shape->leftpix]];

        for (end = ReadShort(linecmds) >> 1; end; end = ReadShort(linecmds) >> 1)
        {
            top = ReadShort(linecmds + 2);
            start = ReadShort(linecmds + 4) >> 1;

            for (src = &linesrc[top + start]; start != end; start++, src++)
            {
                if (!(x % tilewallratio) && !(start % tilewallratio))
                    *(vbuf + ylookup[sy + (start / tilewallratio)] + (sx + (x / tilewallratio))) = *src;
            }

            linecmds += 6;                          // next segment list
        }
    }
}


/*
===================
=
= DrawMapBorder
=
===================
*/

void DrawMapBorder (void)
{
    int height;

    height = screen.height - ((screen.height / tilesize) * tilesize);

    vbuf += ylookup[screen.height - 1];

    while (height--)
    {
        memset (vbuf,BLACK,screen.width);

        vbuf -= screen.buffer->pitch;
    }
}


/*
===================
=
= OverheadRefresh
=
===================
*/

void OverheadRefresh (void)
{
    int       x,y;
    byte      rotate[9] = {6,5,4,3,2,1,0,7,0};
    int16_t   endx,endy;
    int16_t   sx,sy,shapenum;
    uintptr_t tile;
    unsigned  offset;
    statobj_t *statptr;
    objtype   *obj;

    vbuf = VW_LockSurface(screen.buffer);

    if (!vbuf)
        Quit ("OverheadRefresh: Unable to create surface!");

    endx = maporgx + viewtilex;
    endy = maporgy + viewtiley;

    for (y = maporgy; y < endy; y++)
    {
        for (x = maporgx; x < endx; x++)
        {
            sx = (x - maporgx) * tilesize;
            sy = (y - maporgy) * tilesize;
            offset = mapylookup[y] + x;
#ifdef REVEALMAP
            if (!mapseen[offset] && !mapreveal)
            {
                DrawMapFloor (sx,sy,BLACK);
                continue;
            }
#endif
            tile = (uintptr_t)actorat[offset];

            if (tile)
            {
                //
                // draw walls
                //
                if (tile < BIT_DOOR && tile != BIT_WALL)
                {
                    if (DebugOk && Keyboard[sc_P] && mapsegs[1][offset] == PUSHABLETILE)
                        DrawMapFloor (sx,sy,COL_SECRET);
                    else
                        DrawMapWall (sx,sy,horizwall[tile]);
                }
                else if (tile < BIT_ALLTILES && tile != BIT_WALL)
                    DrawMapDoor (sx,sy,tile & ~BIT_DOOR);
                else
                {
                    DrawMapFloor (sx,sy,COL_FLOOR);

                    //
                    // draw actors & static objects
                    //
                    if (DebugOk && ISPOINTER(tile))
                    {
                        obj = (objtype *)tile;

                        if (spotvis[mapylookup[obj->y >> TILESHIFT] + (obj->x >> TILESHIFT)])
                        {
                            shapenum = obj->state->shapenum;

                            if (obj->state->rotate)
                                shapenum += rotate[obj->dir];

                            DrawMapSprite (sx,sy,shapenum);
                        }
                    }
                    else if (tile == BIT_WALL)
                    {
                        for (statptr = &statobjlist[0]; statptr != laststatobj; statptr++)
                        {
                            if (statptr->tilex != x || statptr->tiley != y)
                                continue;

                            if (statptr->itemnumber == block)
                                DrawMapSprite (sx,sy,statptr->shapenum);
                        }
                    }
                }
            }
            else
                DrawMapFloor (sx,sy,COL_FLOOR);
        }
    }

    //
    // cover the empty bar at the bottom of the screen if necessary
    //
    if (screen.height != (viewtiley * tilesize))
        DrawMapBorder ();

    VW_WaitVBL (3);                // don't scroll too fast

    VW_UnlockSurface (screen.buffer);
    vbuf = NULL;

    VW_UpdateScreen ();
}


/*
===================
=
= SetupMapView
=
===================
*/

void SetupMapView (void)
{
    switch (screen.scale)
    {
        case 1: tilesize = 16; break;
        case 2: tilesize = 32; break;

        default: tilesize = TEXTURESIZE; break;
    }

#if TEXTURESHIFT == 8
    tilesize >>= 2;
#elif TEXTURESHIFT == 7
    tilesize >>= 1;
#endif
    tilemapratio = mapwidth / tilesize;
    tilewallratio = TEXTURESIZE / tilesize;

    viewtilex = screen.width / tilesize;
    viewtiley = screen.height / tilesize;

    if (viewtilex > mapwidth)
        viewtilex = mapwidth;
    if (viewtiley > mapheight)
        viewtiley = mapheight;

    maporgx = player->tilex - (viewtilex >> 1);
    maporgy = player->tiley - (viewtiley >> 1);

    if (maporgx < 0)
        maporgx = 0;
    if (maporgx > mapwidth - viewtilex)
        maporgx = mapwidth - viewtilex;

    if (maporgy < 0)
        maporgy = 0;
    if (maporgy > mapheight - viewtiley)
        maporgy = mapheight - viewtiley;
}


/*
===================
=
= ViewMap
=
===================
*/

void ViewMap (void)
{
    SetupMapView ();

    do
    {
        //
        // let user pan around
        //
        PollControls ();

        if ((controlx < 0 || controlturnx < 0) && maporgx > 0)
            maporgx--;
        if ((controlx > 0 || controlturnx > 0) && maporgx < mapwidth - viewtilex)
            maporgx++;
        if (controly < 0 && maporgy > 0)
            maporgy--;
        if (controly > 0 && maporgy < mapheight - viewtiley)
            maporgy++;

        OverheadRefresh ();

    } while (!Keyboard[sc_Escape]);

    IN_ClearKeysDown ();

    if (viewsize != 21)
        DrawPlayScreen ();
}

#endif
