//
//      ID Engine
//      ID_SD.c - Sound Manager for Wolfenstein 3D
//      v1.2
//      By Jason Blochowiak
//

//
//      This module handles dealing with generating sound on the appropriate
//              hardware
//
//      Depends on: User Mgr (for parm checking)
//
//      Globals:
//              For User Mgr:
//                      SoundBlasterPresent - SoundBlaster card present?
//                      AdLibPresent - AdLib card present?
//                      SoundMode - What device is used for sound effects
//                              (Use SM_SetSoundMode() to set)
//                      MusicMode - What device is used for music
//                              (Use SM_SetMusicMode() to set)
//                      DigiMode - What device is used for digitized sound effects
//                              (Use SM_SetDigiDevice() to set)
//
//              For Cache Mgr:
//                      NeedsDigitized - load digitized sounds?
//                      NeedsMusic - load music?
//

#include "wl_def.h"
#include "sd_mixer.h"
#if defined(GP2X_940)
#include "gp2x/fmopl.h"
#else
#ifdef USE_GPL
#include "dosbox/dbopl_adapter.h"
#else
#include "mame/fmopl.h"
#endif
#endif

#define ORIGSAMPLERATE 7042

//
// Where each digitised sound lives.  Pointers into the page file, which
// PM_Startup() makes resident for the life of the program and never moves, so
// a voice can hold one for as long as it plays.
//
static const byte *DigiSound[STARTMUSIC - STARTDIGISOUNDS];
static int         DigiLength[STARTMUSIC - STARTDIGISOUNDS];

//
// A voice.  `pos` and `step` are 32.32 source frames; `left` and `right` are
// 0..255 the way Mix_SetPanning took them.  `started` orders the pool for
// stealing: the lowest wins, which is the oldest.
//
typedef struct
{
    const byte *samples;
    int         length;
    uint64_t    pos,step;
    int         left,right;
    uint32_t    started;
    boolean     active;
} sdvoice_t;

static sdvoice_t Voices[SD_CHANNELS];
static uint32_t  VoiceSeq;

void SD_ChannelFinished(int channel);

static SDL_AudioSpec AudioSpec;

globalsoundpos channelSoundPos[SD_CHANNELS];

//      Global variables
        boolean         AdLibPresent,
                        SoundBlasterPresent,SBProPresent,
                        SoundPositioned;
        int             SoundMode;
        int             MusicMode;
        int             DigiMode;
static  byte          **SoundTable;
        int             DigiMap[LASTSOUND];
        int             DigiChannel[STARTMUSIC - STARTDIGISOUNDS];

//      Internal variables
static  boolean                 SD_Started;
static  boolean                 nextsoundpos;
static  int                     SoundNumber;
static  int                     DigiNumber;
static  word                    SoundPriority;
static  word                    DigiPriority;
static  int                     LeftPosition;
static  int                     RightPosition;

        word                    NumDigi;
        digiinfo                *DigiList;
static  boolean                 DigiPlaying;

//      PC Sound variables
static  volatile byte           pcLastSample;
static  byte * volatile         pcSound;
static  longword                pcLengthLeft;

//      AdLib variables
static  byte * volatile         alSound;
static  byte                    alBlock;
static  longword                alLengthLeft;
static  longword                alTimeCount;
static  Instrument              alZeroInst;

//      Sequencer variables
static  volatile boolean        sqActive;
static  word                   *sqHack;
static  word                   *sqHackPtr;
static  int32_t                 sqHackLen;
static  int32_t                 sqHackSeqLen;
static  longword                sqHackTime;

//
// Both emulators are reached through the same three functions, so this is the
// chip index MAME wants and DBOPL ignores.  There is only ever one.
//
static const int oplChip = 0;

void Delay (int32_t wolfticks)
{
    if (wolfticks > 0)
        SDL_Delay ((wolfticks * 100) / 7);
}


static void SDL_SoundFinished(void)
{
	SoundNumber = 0;
	SoundPriority = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_PCPlaySound() - Plays the specified sound on the PC speaker
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_PCPlaySound(PCSound *sound)
{
        pcLastSample = (byte)-1;
        pcLengthLeft = sound->common.length;
        pcSound = sound->data;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_PCStopSound() - Stops the current sound playing on the PC Speaker
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_PCStopSound(void)
{
        pcSound = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutPC() - Turns off the pc speaker
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutPC(void)
{
        pcSound = 0;
}

// Adapted from Chocolate Doom (chocolate-doom/pcsound/pcsound_sdl.c)
#define SQUARE_WAVE_AMP 0x2000

static void SDL_PCMixCallback(void *udata, Uint8 *stream, int len)
{
    static int current_remaining = 0;
    static int current_freq = 0;
    static int phase_offset = 0;

    Sint16 *leftptr;
    Sint16 *rightptr;
    Sint16 this_value;
    int i;
    int nsamples;

    // Number of samples is quadrupled, because of 16-bit and stereo

    nsamples = len / 4;

    leftptr = (Sint16 *) stream;
    rightptr = ((Sint16 *) stream) + 1;

    // Fill the output buffer

    for (i=0; i<nsamples; ++i)
    {
        // Has this sound expired? If so, retrieve the next frequency

        while (current_remaining == 0)
        {
            phase_offset = 0;

            // Get the next frequency to play

            if(pcSound)
            {
                // The PC speaker sample rate is 140Hz (see SDL_t0SlowAsmService)
                current_remaining = param_samplerate / 140;

                if(*pcSound!=pcLastSample)
                {
                    pcLastSample=*pcSound;

                    if(pcLastSample)
                        // The PC PIC counts down at 1.193180MHz
                        // So pwm_freq = counter_freq / reload_value
                        // reload_value = pcLastSample * 60 (see SDL_DoFX)
                        current_freq = 1193180 / (pcLastSample * 60);
                    else
                        current_freq = 0;

                }
                pcSound++;
                pcLengthLeft--;
                if(!pcLengthLeft)
                {
                    pcSound=0;
                    SoundNumber=0;
                    SoundPriority=0;
                }
            }
            else
            {
                current_freq = 0;
                current_remaining = 1;
            }
        }

        // Set the value for this sample.

        if (current_freq == 0)
        {
            // Silence

            this_value = 0;
        }
        else
        {
            int frac;

            // Determine whether we are at a peak or trough in the current
            // sound.  Multiply by 2 so that frac % 2 will give 0 or 1
            // depending on whether we are at a peak or trough.

            frac = (phase_offset * current_freq * 2) / param_samplerate;

            if ((frac % 2) == 0)
            {
                this_value = SQUARE_WAVE_AMP;
            }
            else
            {
                this_value = -SQUARE_WAVE_AMP;
            }

            ++phase_offset;
        }

        --current_remaining;

        // Use the same value for the left and right channels.

        *leftptr += this_value;
        *rightptr += this_value;

        leftptr += 2;
        rightptr += 2;
    }
}

void
SD_StopDigitized(void)
{
    DigiPlaying = false;
    DigiNumber = 0;
    DigiPriority = 0;
    SoundPositioned = false;
    if ((DigiMode == sds_PC) && (SoundMode == sdm_PC))
        SDL_SoundFinished();

    switch (DigiMode)
    {
        case sds_PC:
            SDL_PCStopSound();
            break;
        case sds_SoundBlaster:
            SD_StopAllVoices();
            break;

        default:
            break;
    }
}

//
// Channels 0 and 1 are spoken for - the player's weapon and the boss's, named
// by DigiChannel[] out of wolfdigimap - and the rest are a pool.  A free one
// if there is one, otherwise the oldest, which is what Mix_GroupAvailable()
// then Mix_GroupOldest() did.
//
int SD_GetChannelForDigi(int which)
{
    int i,oldest;

    if(DigiChannel[which] != -1)
        return DigiChannel[which];

    for(i = SD_RESERVEDCHANNELS; i < SD_CHANNELS; i++)
        if(!Voices[i].active)
            return i;

    oldest = SD_RESERVEDCHANNELS;

    for(i = SD_RESERVEDCHANNELS + 1; i < SD_CHANNELS; i++)
        if(Voices[i].started < Voices[oldest].started)
            oldest = i;

    return oldest;
}

//
// Silence every voice.  Called from the game thread, so it takes the lock.
//
void SD_StopAllVoices(void)
{
    int i;

    SDL_LockAudio();

    for(i = 0; i < SD_CHANNELS; i++)
    {
        Voices[i].active = false;
        Voices[i].samples = NULL;
        channelSoundPos[i].valid = 0;
    }

    SDL_UnlockAudio();
}

//
// The mixer proper: every active voice, resampled, panned and summed into the
// block.  Runs on the audio thread and allocates nothing.
//
static void SD_MixVoices(int16_t *stream, int frames)
{
    int i,f;

    for(i = 0; i < SD_CHANNELS; i++)
    {
        sdvoice_t *v = &Voices[i];

        if(!v->active)
            continue;

        for(f = 0; f < frames; f++)
        {
            int sample;

            if((int64_t)(v->pos >> 32) >= v->length)
            {
                v->active = false;
                v->samples = NULL;
                SD_ChannelFinished(i);
                break;
            }

            sample = SD_ResampleSample(v->samples,v->length,v->pos);
            v->pos += v->step;

            SD_MixSample(&stream[f * 2],    (sample * v->left)  >> 8);
            SD_MixSample(&stream[f * 2 + 1],(sample * v->right) >> 8);
        }
    }
}

void SD_SetPosition(int channel, int leftpos, int rightpos)
{
    if((leftpos < 0) || (leftpos > 15) || (rightpos < 0) || (rightpos > 15)
            || ((leftpos == 15) && (rightpos == 15)))
        Quit("SD_SetPosition: Illegal position");

    switch (DigiMode)
    {
        case sds_SoundBlaster:
            //
            // leftpos/rightpos are 0..15 with 0 the loudest, which is the
            // conversion Mix_SetPanning() was handed.
            //
            if(channel >= 0 && channel < SD_CHANNELS)
            {
                Voices[channel].left  = 255 - (leftpos * 28);
                Voices[channel].right = 255 - (rightpos * 28);
            }
            break;

        default:
            break;
    }
}

//
// Note where a digitised sound is, and check it is inside the page file.  That
// is the whole of preparing one now: it is played from where it lies, at the
// rate it was sampled at, and the mixer does the resampling a frame at a time.
//
void SD_PrepareSound(int which)
{
    int         page,size;
    const byte *samples;

    if(DigiList == NULL)
        Quit("SD_PrepareSound(%i): DigiList not initialized!\n", which);

    page = DigiList[which].startpage;
    size = DigiList[which].length;

    samples = PM_GetSoundPage(page);

    if(samples + size >= PM_GetPageEnd())
        Quit("SD_PrepareSound(%i): Sound reaches out of page file!\n", which);

    DigiSound[which]  = samples;
    DigiLength[which] = size;
}

int SD_PlayDigitized(word which,int leftpos,int rightpos)
{
    if (!DigiMode)
        return 0;

    if (which >= NumDigi)
        Quit("SD_PlayDigitized: bad sound number %i", which);

    int        channel = SD_GetChannelForDigi(which);
    uint64_t   step = SD_ResampleStep(SD_DIGIRATE,(unsigned) AudioSpec.freq);
    sdvoice_t *v;

    if(DigiSound[which] == NULL)
    {
        printf("DigiSound[%i] is NULL!\n", which);
        return 0;
    }

    //
    // No device, no rate, no step - and a voice whose cursor never advances
    // would hold its first sample for ever rather than ending.
    //
    if(!step)
        return 0;

    DigiPlaying = true;

    //
    // The audio thread walks this voice, so the whole of starting it happens
    // under the lock rather than leaving a half-set voice to be mixed.
    //
    SDL_LockAudio();

    v = &Voices[channel];

    v->samples = DigiSound[which];
    v->length  = DigiLength[which];
    v->pos     = 0;
    v->step    = step;
    v->started = ++VoiceSeq;
    v->active  = true;

    SDL_UnlockAudio();

    SD_SetPosition(channel, leftpos,rightpos);

    return channel;
}

void SD_ChannelFinished(int channel)
{
    channelSoundPos[channel].valid = 0;
}

void
SD_SetDigiDevice(int mode)
{
    boolean devicenotpresent;

    if (mode == DigiMode)
        return;

    SD_StopDigitized();

    devicenotpresent = false;
    switch (mode)
    {
        case sds_SoundBlaster:
            if (!SoundBlasterPresent)
                devicenotpresent = true;
            break;

        default:
            break;
    }

    if (!devicenotpresent)
    {
        DigiMode = mode;
    }
}

void
SDL_SetupDigi(void)
{
    // Correct padding enforced by PM_Startup()
    word *soundInfoPage = (word *) (void *) PM_GetPage(ChunksInFile-1);
    NumDigi = (word) PM_GetPageSize(ChunksInFile - 1) / 4;

    DigiList = SafeMalloc(NumDigi * sizeof(*DigiList));
    int i,page;
    for(i = 0; i < NumDigi; i++)
    {
        // Calculate the size of the digi from the sizes of the pages between
        // the start page and the start page of the next sound

        DigiList[i].startpage = soundInfoPage[i * 2];
        if((int) DigiList[i].startpage >= ChunksInFile - 1)
        {
            NumDigi = i;
            break;
        }

        int lastPage;
        if(i < NumDigi - 1)
        {
            lastPage = soundInfoPage[i * 2 + 2];
            if(lastPage == 0 || lastPage + PMSoundStart > ChunksInFile - 1) lastPage = ChunksInFile - 1;
            else lastPage += PMSoundStart;
        }
        else lastPage = ChunksInFile - 1;

        int size = 0;
        for(page = PMSoundStart + DigiList[i].startpage; page < lastPage; page++)
            size += PM_GetPageSize(page);

        // Don't include padding of sound info page, if padding was added
        if(lastPage == ChunksInFile - 1 && PMSoundInfoPagePadded) size--;

        // Patch lower 16-bit of size with size from sound info page.
        // The original VSWAP contains padding which is included in the page size,
        // but not included in the 16-bit size. So we use the more precise value.
        if((size & 0xffff0000) != 0 && (size & 0xffff) < soundInfoPage[i * 2 + 1])
            size -= 0x10000;
        size = (size & 0xffff0000) | soundInfoPage[i * 2 + 1];

        DigiList[i].length = size;
    }

    for(i = 0; i < LASTSOUND; i++)
    {
        DigiMap[i] = -1;
        DigiChannel[i] = -1;
    }
}

//      AdLib Code

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ALStopSound() - Turns off any sound effects playing through the
//              AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALStopSound(void)
{
    alSound = 0;
    alOut(alFreqH + 0, 0);
}

static void
SDL_AlSetFXInst(Instrument *inst)
{
    byte c,m;

    m = 0;      // modulator cell for channel 0
    c = 3;      // carrier cell for channel 0
    alOut(m + alChar,inst->mChar);
    alOut(m + alScale,inst->mScale);
    alOut(m + alAttack,inst->mAttack);
    alOut(m + alSus,inst->mSus);
    alOut(m + alWave,inst->mWave);
    alOut(c + alChar,inst->cChar);
    alOut(c + alScale,inst->cScale);
    alOut(c + alAttack,inst->cAttack);
    alOut(c + alSus,inst->cSus);
    alOut(c + alWave,inst->cWave);

    // Note: Switch commenting on these lines for old MUSE compatibility
//    alOutInIRQ(alFeedCon,inst->nConn);
    alOut(alFeedCon,0);
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ALPlaySound() - Plays the specified sound on the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALPlaySound(AdLibSound *sound)
{
    Instrument      *inst;
    byte            *data;

    SDL_ALStopSound();

    alLengthLeft = sound->common.length;
    data = sound->data;
    alBlock = ((sound->block & 7) << 2) | 0x20;
    inst = &sound->inst;

    if (!(inst->mSus | inst->cSus))
    {
        Quit("SDL_ALPlaySound() - Bad instrument");
    }

    SDL_AlSetFXInst(inst);
    alSound = (byte *)data;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutAL() - Shuts down the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutAL(void)
{
    alSound = 0;
    alOut(alEffects,0);
    alOut(alFreqH + 0,0);
    SDL_AlSetFXInst(&alZeroInst);
}


///////////////////////////////////////////////////////////////////////////
//
//      SDL_StartAL() - Starts up the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartAL(void)
{
    alOut(alEffects, 0);
    SDL_AlSetFXInst(&alZeroInst);
}


////////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutDevice() - turns off whatever device was being used for sound fx
//
////////////////////////////////////////////////////////////////////////////
static void
SDL_ShutDevice(void)
{
    switch (SoundMode)
    {
        case sdm_PC:
            SDL_ShutPC();
            break;
        case sdm_AdLib:
            SDL_ShutAL();
            break;

        default:
            break;
    }
    SoundMode = sdm_Off;
}


///////////////////////////////////////////////////////////////////////////
//
//      SDL_StartDevice() - turns on whatever device is to be used for sound fx
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartDevice(void)
{
    switch (SoundMode)
    {
        case sdm_AdLib:
            SDL_StartAL();
            break;

        default:
            break;
    }
    SoundNumber = 0;
    SoundPriority = 0;
}

//      Public routines

///////////////////////////////////////////////////////////////////////////
//
//      SD_SetSoundMode() - Sets which sound hardware to use for sound effects
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetSoundMode(int mode)
{
    boolean result = false;
    word    tableoffset;

    SD_StopSound();

    if ((mode == sdm_AdLib) && !AdLibPresent)
        mode = sdm_PC;

    switch (mode)
    {
        case sdm_Off:
            tableoffset = STARTADLIBSOUNDS;
            result = true;
            break;
        case sdm_PC:
            tableoffset = STARTPCSOUNDS;
            result = true;
            break;
        case sdm_AdLib:
            tableoffset = STARTADLIBSOUNDS;
            if (AdLibPresent)
                result = true;
            break;
        default:
            Quit("SD_SetSoundMode: Invalid sound mode %i", mode);
            return false;
    }
    SoundTable = &audiosegs[tableoffset];

    if (result && (mode != SoundMode))
    {
        SDL_ShutDevice();
        SoundMode = mode;
        SDL_StartDevice();
    }

    return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_SetMusicMode() - sets the device to use for background music
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetMusicMode(int mode)
{
    boolean result = false;

    SD_FadeOutMusic();
    while (SD_MusicPlaying())
        SDL_Delay(5);

    switch (mode)
    {
        case smm_Off:
            result = true;
            break;
        case smm_AdLib:
            if (AdLibPresent)
                result = true;
            break;
    }

    if (result)
        MusicMode = mode;

    return(result);
}

int numreadysamples = 0;
byte *curAlSound = 0;
byte *curAlSoundPtr = 0;
longword curAlLengthLeft = 0;
int soundTimeCounter = 5;
int samplesPerMusicTick;

//
// Scratch for one piece of OPL output.  The emulator writes its block, and it
// is added to the stream rather than replacing it, because with a single
// callback the music shares the block with the digitised voices and the
// speaker.  Under Mix_HookMusic this function owned the stream and could write
// it; nothing owns it now.
//
// 512 frames because YM3812UpdateOne() clamps to that, and the pieces asked
// for here are samplesPerMusicTick - param_samplerate/700, so 63 at 44.1 kHz.
//
#define SD_OPLSCRATCHFRAMES 512

static int16_t OplScratch[SD_OPLSCRATCHFRAMES * 2];

static void SDL_OPLAdd(int16_t *stream, int frames)
{
    int i;

    while(frames > 0)
    {
        int piece = frames > SD_OPLSCRATCHFRAMES ? SD_OPLSCRATCHFRAMES : frames;

        YM3812UpdateOne(oplChip, OplScratch, piece);

        for(i = 0; i < piece * 2; i++)
            SD_MixSample(&stream[i],OplScratch[i]);

        stream += piece * 2;
        frames -= piece;
    }
}

void SDL_IMFMusicPlayer(void *udata, Uint8 *stream, int len)
{
    int stereolen = len>>1;
    int sampleslen = stereolen>>1;
    int16_t *stream16 = (int16_t *) (void *) stream;    // expect correct alignment

    while(1)
    {
        if(numreadysamples)
        {
            if(numreadysamples<sampleslen)
            {
                SDL_OPLAdd(stream16, numreadysamples);
                stream16 += numreadysamples*2;
                sampleslen -= numreadysamples;
            }
            else
            {
                SDL_OPLAdd(stream16, sampleslen);
                numreadysamples -= sampleslen;
                return;
            }
        }
        soundTimeCounter--;
        if(!soundTimeCounter)
        {
            soundTimeCounter = 5;
            if(curAlSound != alSound)
            {
                curAlSound = curAlSoundPtr = alSound;
                curAlLengthLeft = alLengthLeft;
            }
            if(curAlSound)
            {
                if(*curAlSoundPtr)
                {
                    alOut(alFreqL, *curAlSoundPtr);
                    alOut(alFreqH, alBlock);
                }
                else alOut(alFreqH, 0);
                curAlSoundPtr++;
                curAlLengthLeft--;
                if(!curAlLengthLeft)
                {
                    curAlSound = alSound = 0;
                    SoundNumber = 0;
                    SoundPriority = 0;
                    alOut(alFreqH, 0);
                }
            }
        }
        if(sqActive)
        {
            do
            {
                if(sqHackTime > alTimeCount) break;
                sqHackTime = alTimeCount + *(sqHackPtr+1);
                alOut(*(byte *) sqHackPtr, *(((byte *) sqHackPtr)+1));
                sqHackPtr += 2;
                sqHackLen -= 4;
            }
            while(sqHackLen>0);
            alTimeCount++;
            if(!sqHackLen)
            {
                sqHackPtr = sqHack;
                sqHackLen = sqHackSeqLen;
                sqHackTime = 0;
                alTimeCount = 0;
            }
        }
        numreadysamples = samplesPerMusicTick;
    }
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_Startup() - starts up the Sound Mgr
//              Detects all additional sound hardware and installs my ISR
//
///////////////////////////////////////////////////////////////////////////
//
// The one audio callback.
//
// SDL_mixer built this out of three pieces that ran in a fixed order - a music
// hook that wrote the block, the channel mixer that added to it, and a post-mix
// hook that added again.  The order is kept; what has gone is the library.
//
static void SD_AudioCallback(void *udata, Uint8 *stream, int len)
{
    int16_t *stream16 = (int16_t *) (void *) stream;
    int      frames = len / (2 * (int) sizeof(int16_t));

    (void)udata;

    memset(stream,0,len);

    SD_MixVoices(stream16, frames);
    SDL_IMFMusicPlayer(NULL, stream, len);
    SDL_PCMixCallback(NULL, stream, len);
}


void
SD_Startup(void)
{
    int     i;
    int chunksize;

    if (SD_Started)
        return;

    //
    // use a custom size audiobuffer or the largest power
    // of 2 <= the value calculated based on the samplerate
    //
    if (param_audiobuffer != DEFAULT_AUDIO_BUFFER_SIZE)
        chunksize = param_audiobuffer;
    else
    {
        if (!param_samplerate || param_samplerate > 44100)
            Quit ("Divide by zero caused by invalid samplerate!");

        chunksize = 1 << (int)log2(param_audiobuffer / (44100 / param_samplerate));
    }

    memset (&AudioSpec,0,sizeof(AudioSpec));

    AudioSpec.freq     = param_samplerate;
    AudioSpec.format   = AUDIO_S16SYS;
    AudioSpec.channels = 2;
    AudioSpec.samples  = chunksize;
    AudioSpec.callback = SD_AudioCallback;

    //
    // Take the obtained spec, not the desired one.  A backend is free to hand
    // back a different rate or block size - PicoSDL fixes the block size
    // outright - and everything downstream derives from it: the resampling
    // step for every voice, and samplesPerMusicTick below.
    //
    if (SDL_OpenAudio(&AudioSpec,&AudioSpec) < 0)
    {
        snprintf (str,sizeof(str),"Unable to open audio device: %s\n", SDL_GetError());
        Error (str);
        return;
    }

    param_samplerate = AudioSpec.freq;

    // Init music

    samplesPerMusicTick = param_samplerate / 700;    // SDL_t0FastAsmService played at 700Hz

    if(YM3812Init(1,3579545,param_samplerate))
    {
        printf("Unable to create virtual OPL!!\n");
    }

    for(i=1;i<0xf6;i++)
        YM3812Write(oplChip,i,0);

    YM3812Write(oplChip,1,0x20); // Set WSE=1
//    YM3812Write(0,8,0); // Set CSM=0 & SEL=0		 // already set in for statement

    AdLibPresent = true;
    SoundBlasterPresent = true;

    alTimeCount = 0;

    SDL_PauseAudio(0);

    SD_SetSoundMode(sdm_Off);
    SD_SetMusicMode(smm_Off);

    SDL_SetupDigi();

    if (savedsoundmode == -1)
        savedsoundmode = sdm_AdLib;

    if (savedmusicmode == -1)
        savedmusicmode = smm_AdLib;

    if (saveddigimode == -1)
        saveddigimode = sds_SoundBlaster;

    SD_SetMusicMode (savedmusicmode);
    SD_SetSoundMode (savedsoundmode);
    SD_SetDigiDevice (saveddigimode);

    SD_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_Shutdown() - shuts down the Sound Mgr
//              Removes sound ISR and turns off whatever sound hardware was active
//
///////////////////////////////////////////////////////////////////////////
void
SD_Shutdown(void)
{
    int i;

    if (!SD_Started)
        return;

    SD_MusicOff();
    SD_StopSound();

    SDL_CloseAudio();

    for(i = 0; i < STARTMUSIC - STARTDIGISOUNDS; i++)
    {
        DigiSound[i] = NULL;
        DigiLength[i] = 0;
    }

    SafeFree (DigiList);

    SD_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_PositionSound() - Sets up a stereo imaging location for the next
//              sound to be played. Each channel ranges from 0 to 15.
//
///////////////////////////////////////////////////////////////////////////
void
SD_PositionSound(int leftvol,int rightvol)
{
    LeftPosition = leftvol;
    RightPosition = rightvol;
    nextsoundpos = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_PlaySound() - plays the specified sound on the appropriate hardware
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_PlaySound(int sound)
{
    boolean         ispos;
    SoundCommon     *s;
    int             lp,rp;

    lp = LeftPosition;
    rp = RightPosition;
    LeftPosition = 0;
    RightPosition = 0;

    ispos = nextsoundpos;
    nextsoundpos = false;

    if (sound == -1 || (DigiMode == sds_Off && SoundMode == sdm_Off))
        return 0;

    s = (SoundCommon *) SoundTable[sound];

    if ((SoundMode != sdm_Off) && !s)
            Quit("SD_PlaySound() - Uncached sound");

    if ((DigiMode != sds_Off) && (DigiMap[sound] != -1))
    {
        if ((DigiMode == sds_PC) && (SoundMode == sdm_PC))
        {
            if (s->priority < SoundPriority)
                return 0;

            SDL_PCStopSound();

            SD_PlayDigitized(DigiMap[sound],lp,rp);
            SoundPositioned = ispos;
            SoundNumber = sound;
            SoundPriority = s->priority;
        }
        else
        {
#ifdef NOTYET
            if (s->priority < DigiPriority)
                return(false);
#endif

            int channel = SD_PlayDigitized(DigiMap[sound], lp, rp);
            SoundPositioned = ispos;
            DigiNumber = sound;
            DigiPriority = s->priority;
            return channel + 1;
        }

        return(true);
    }

    if (SoundMode == sdm_Off)
        return 0;

    if (!s->length)
        Quit("SD_PlaySound() - Zero length sound");
    if (s->priority < SoundPriority)
        return 0;

    switch (SoundMode)
    {
        case sdm_PC:
            SDL_PCPlaySound((PCSound *)s);
            break;
        case sdm_AdLib:
#ifdef ADDEDFIX // 2
            curAlSound = alSound = 0;                // Tricob
            alOut(alFreqH, 0);
#endif
            SDL_ALPlaySound((AdLibSound *)s);
            break;

        default:
            break;
    }

    SoundNumber = sound;
    SoundPriority = s->priority;

    return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_SoundPlaying() - returns the sound number that's playing, or 0 if
//              no sound is playing
//
///////////////////////////////////////////////////////////////////////////
word
SD_SoundPlaying(void)
{
    boolean result = false;

    switch (SoundMode)
    {
        case sdm_PC:
            result = pcSound? true : false;
            break;
        case sdm_AdLib:
            result = alSound? true : false;
            break;

        default:
            break;
    }

    if (result)
        return(SoundNumber);
    else
        return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////
void
SD_StopSound(void)
{
    if (DigiPlaying)
        SD_StopDigitized();

    switch (SoundMode)
    {
        case sdm_PC:
            SDL_PCStopSound();
            break;
        case sdm_AdLib:
            SDL_ALStopSound();
            break;

        default:
            break;
    }

    SoundPositioned = false;

    SDL_SoundFinished();
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////
void
SD_WaitSoundDone(void)
{
    while (SD_SoundPlaying())
        SDL_Delay(5);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////
void
SD_MusicOn(void)
{
    sqActive = true;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOff() - turns off the sequencer and any playing notes
//      returns the last music offset for music continue
//
///////////////////////////////////////////////////////////////////////////
int
SD_MusicOff(void)
{
    word    i;

    sqActive = false;
    switch (MusicMode)
    {
        case smm_AdLib:
            alOut(alEffects, 0);
            for (i = 0;i < sqMaxTracks;i++)
                alOut(alFreqH + i + 1, 0);
            break;

        default:
            break;
    }

    return (int) (sqHackPtr-sqHack);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////
void
SD_StartMusic(int chunk)
{
    SD_MusicOff();

    if (MusicMode == smm_AdLib)
    {
        sqHackLen = sqHackSeqLen = ReadLong(audiosegs[chunk]);
        sqHackPtr = sqHack = (word *)(audiosegs[chunk] + sizeof(sqHackLen));

        sqHackTime = 0;
        alTimeCount = 0;
        SD_MusicOn();
    }
}

void
SD_ContinueMusic(int chunk, int32_t startoffs)
{
    int32_t i;

    SD_MusicOff();

    if (MusicMode == smm_AdLib)
    {
        sqHackLen = sqHackSeqLen = ReadLong(audiosegs[chunk]);
        sqHackPtr = sqHack = (word *)(audiosegs[chunk] + sizeof(sqHackLen));

        if(startoffs >= sqHackLen)
        {
#ifdef ADDEDFIX // 7                     // Andy, improved by Chris Chokan
            startoffs = 0;
#else
            Quit("SD_StartMusic: Illegal startoffs provided!");
#endif
        }

        // fast forward to correct position
        // (needed to reconstruct the instruments)

        for(i = 0; i < startoffs; i += 2)
        {
            byte reg = *(byte *)sqHackPtr;
            byte val = *(((byte *)sqHackPtr) + 1);
            if(reg >= 0xb1 && reg <= 0xb8) val &= 0xdf;           // disable play note flag
            else if(reg == 0xbd) val &= 0xe0;                     // disable drum flags

            alOut(reg,val);
            sqHackPtr += 2;
            sqHackLen -= 4;
        }
        sqHackTime = 0;
        alTimeCount = 0;

        SD_MusicOn();
    }
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_FadeOutMusic() - starts fading out the music. Call SD_MusicPlaying()
//              to see if the fadeout is complete
//
///////////////////////////////////////////////////////////////////////////
void
SD_FadeOutMusic(void)
{
    switch (MusicMode)
    {
        case smm_AdLib:
            // DEBUG - quick hack to turn the music off
            SD_MusicOff();
            break;

        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicPlaying() - returns true if music is currently playing, false if
//              not
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_MusicPlaying(void)
{
    boolean result;

    switch (MusicMode)
    {
        case smm_AdLib:
            result = sqActive;
            break;
        default:
            result = false;
            break;
    }

    return(result);
}
