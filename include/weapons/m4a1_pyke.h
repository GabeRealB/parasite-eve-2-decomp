#ifndef M4A1_PYKE_H
#define M4A1_PYKE_H

#include "common.h"
#include <psyq/libgte.h>

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
extern SVECTOR D_m4a1_pyke_8011E90C;

/// Handwritten GTE routine. Projects the world-space point `pos` through
/// `GsWSMATRIX` and links one frame of the beam sprite there; `frame` picks the
/// sprite and `brightness` scales its colour.
void func_m4a1_pyke_8011D548(VECTOR3* pos, u16 frame, s32 brightness);

#endif
