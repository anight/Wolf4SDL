// WL_UTILS.H

#ifndef __WL_UTILS_H_
#define __WL_UTILS_H_

#include "wl_def.h"

#define SafeMalloc(s)    safe_malloc ((s),__FILE__,__LINE__)
#define SafeRealloc(m,s) safe_realloc ((m),(s),__FILE__,__LINE__)
#define SafeFree(p)      safe_free ((void **)&(p))
#define FRACBITS         16

void     *safe_malloc (size_t size, const char *fname, uint32_t line);
void     *safe_realloc (void *mem, size_t size, const char *fname, uint32_t line);
void     safe_free (void **ptr);

fixed    FixedMul (fixed a, fixed b);
fixed    FixedDiv (fixed a, fixed b);

uint16_t ReadShort (void *ptr);
uint32_t ReadLong (void *ptr);

void     Error (const char *string);
void     Help (const char *string);

#endif
