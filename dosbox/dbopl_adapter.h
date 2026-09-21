//
// A C-callable face for DOSBox's DBOPL.
//
// id_sd.c is C, and reaches the OPL through three functions shaped like
// mame/fmopl.h's - YM3812Init, YM3812Write, YM3812UpdateOne - so that the two
// emulators are interchangeable at the call site and alOut() does not care
// which is built.  DBOPL is C++: a chip is a class and dbopl.h opens a
// namespace, neither of which a C compiler can read.  Declaring those wrappers
// inside id_sd.c, as was done before, meant the GPL build asked $(CC) to
// compile C++ and failed on the first `::`.
//
// This header is the wall between the two.  Everything C++ stays on the other
// side of it, in dbopl_adapter.cpp.
//
#ifndef __DBOPL_ADAPTER_H_
#define __DBOPL_ADAPTER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// There is one chip, so `which` is accepted and ignored; it exists because
// MAME's interface has it and id_sd.c passes oplChip to both.  numChips and
// clock are likewise MAME's: DBOPL fixes its own oscillator and takes only the
// output rate.
//
int  YM3812Init      (int numChips, int clock, int rate);
int  YM3812Write     (int which, int reg, int val);
void YM3812UpdateOne (int which, int16_t *stream, int length);

#ifdef __cplusplus
}
#endif

#endif
