//
// DBOPL behind the C interface declared in dbopl_adapter.h.  The body of
// YM3812UpdateOne is what id_sd.c used to carry inline.
//
#include "dbopl.h"
#include "dbopl_adapter.h"

//
// One chip, constructed at load time.  DBOPL::Chip's constructor is what calls
// InitTables(), which is guarded against running twice, so there is nothing
// else to initialise.
//
static DBOPL::Chip oplchip;

int YM3812Init (int numChips, int clock, int rate)
{
	(void)numChips;
	(void)clock;

	oplchip.Setup((Bit32u)rate);

	return 0;
}

int YM3812Write (int which, int reg, int val)
{
	(void)which;

	oplchip.WriteReg((Bit32u)reg, (Bit8u)val);

	return 0;
}

void YM3812UpdateOne (int which, int16_t *stream, int length)
{
	Bit32s buffer[512 * 2];
	int    i;

	(void)which;

	// length is at maximum samplesPerMusicTick = param_samplerate / 700
	// so 512 is sufficient for a sample rate of 358.4 kHz (default 44.1 kHz)
	if(length > 512)
		length = 512;

	if(oplchip.opl3Active)
	{
		oplchip.GenerateBlock3(length, buffer);

		// GenerateBlock3 generates a number of "length" 32-bit stereo samples
		// so we only need to convert them to 16-bit samples
		for(i = 0; i < length * 2; i++)  // * 2 for left/right channel
		{
			// Multiply by 4 to match loudness of MAME emulator.
			Bit32s sample = buffer[i] << 2;
			if(sample > 32767) sample = 32767;
			else if(sample < -32768) sample = -32768;
			stream[i] = (int16_t) sample;
		}
	}
	else
	{
		oplchip.GenerateBlock2(length, buffer);

		// GenerateBlock2 generates a number of "length" 32-bit mono samples
		// so we need to convert them to 16-bit stereo samples
		for(i = 0; i < length; i++)
		{
			// Multiply by 4 to match loudness of MAME emulator.
			// Then upconvert to stereo.
			Bit32s sample = buffer[i] << 2;
			if(sample > 32767) sample = 32767;
			else if(sample < -32768) sample = -32768;
			stream[i * 2] = stream[i * 2 + 1] = (int16_t) sample;
		}
	}
}
