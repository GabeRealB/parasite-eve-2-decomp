#ifndef ROOMS_DRYFIELD_CELLAR_H
#define ROOMS_DRYFIELD_CELLAR_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Draws one glow sprite at `arg1`, a point in `arg0`'s local space; `arg2`
/// picks the texture column and clut, `arg3` is the half-extent.
void func_dryfield_cellar_8017D7DC(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

#endif // ROOMS_DRYFIELD_CELLAR_H
