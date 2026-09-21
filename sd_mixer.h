//
// The digitised-sound mixer.
//
// This is what SDL_mixer used to do for this game, and the only thing it did:
// the OPL music and the PC speaker were always Wolf's own code, running in a
// Mix_HookMusic and a Mix_SetPostMix callback respectively.  What is left when
// those are lifted out is a handful of voices, panned and summed - so that is
// what this is.
//
// Nothing here allocates and nothing here converts ahead of time.  A digitised
// sound stays exactly as VSWAP holds it, 8-bit unsigned mono at 7042 Hz, and a
// fixed-point cursor walks it at source_rate/output_rate while the voice plays.
// SD_PrepareSound() used to expand every sound to the device rate in 16-bit
// stereo: 391,666 bytes of source became 4.68 MiB at 44.1 kHz, which is nine
// times the RAM the target board has in total.
//
#ifndef __SD_MIXER_H_
#define __SD_MIXER_H_

#include <stdint.h>

//
// Voices.  Eight, because that is what SDL_mixer defaulted to and what
// channelSoundPos[] and DigiChannel[] are sized against: the first two are
// reserved for the player's and the boss's weapons, the rest are a pool.
//
#define SD_CHANNELS     8

//
// The rate VSWAP's digitised sounds were sampled at.
//
#define SD_DIGIRATE     7042

//
// 32.32 source frames per output frame.
//
// Not 16.16, for the reason the cursor is accumulated rather than recomputed:
// a truncated 16.16 step is wrong by up to one part in 65536 *per output
// frame*, and over a three-second sound that compounds into an audible drift.
// On a Cortex-M33 the 64-bit add is a pair of instructions.
//
static inline uint64_t SD_ResampleStep (unsigned sourcerate, unsigned outrate)
{
    if (!outrate)
        return 0;

    return ((uint64_t) sourcerate << 32) / outrate;
}

//
// One output sample for cursor `pos`, linearly interpolated, or 0 past the end.
//
// The unsigned byte is centred and scaled exactly as the float path it
// replaces did - (v - 128) * 256 - so silence is silence.  Widening it as
// (v | v<<8) - 32768 instead, which is the other obvious way to fill the
// 16-bit range, puts a constant +128 on every voice: inaudible alone, but it
// is DC, it is per voice, and there is nothing downstream to remove it.
//
// The fraction uses all 32 bits and rounds to nearest: taking only the top 16
// and truncating costs up to 2 LSB, which is worse than what it replaces.
//
static inline int SD_ResampleSample (const uint8_t *src, int count, uint64_t pos)
{
    uint64_t idx = pos >> 32;
    int      s0,s1;
    uint32_t frac;
    int64_t  delta;

    if (src == NULL || (int64_t) idx >= count)
        return 0;

    s0 = ((int) src[idx] - 128) * 256;

    if ((int64_t) idx + 1 >= count)
        return s0;

    s1 = ((int) src[idx + 1] - 128) * 256;

    frac  = (uint32_t) (pos & 0xffffffffu);
    delta = (((int64_t) (s1 - s0) * (int64_t) frac) + ((int64_t) 1 << 31)) >> 32;

    return s0 + (int) delta;
}

//
// Saturating accumulate.
//
// Every source that reaches the stream is individually clamped, so any one of
// them alone is safe; their sum is not.  Music under a loud effect exceeds
// 32767, and a bare `+=` on an int16_t wraps it to a large negative number - a
// click, or over a sustained passage continuous harsh noise.  On a desktop the
// mixer downstream has headroom to hide that.  On the board these samples go
// almost straight to the DAC.
//
static inline void SD_MixSample (int16_t *dest, int value)
{
    int sum = (int) *dest + value;

    if (sum > 32767)
        sum = 32767;
    else if (sum < -32768)
        sum = -32768;

    *dest = (int16_t) sum;
}

#endif
