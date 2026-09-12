#ifndef ACTORS_SHARED_80143374_H
#define ACTORS_SHARED_80143374_H

#include "common.h"

/// Raises the floor for one collision quad and the quad after it: writes the
/// `vy` of the eight `GpGridParams.field_8` corners starting at `arg1 * 4`,
/// giving each quad a 500 / 500 / 800 / 800 height profile.
void ActorsShared80143374(s32 arg0, s16 arg1);

#endif
