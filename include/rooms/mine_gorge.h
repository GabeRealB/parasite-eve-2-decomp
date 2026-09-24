#ifndef ROOMS_MINE_GORGE_H
#define ROOMS_MINE_GORGE_H

#include "common.h"

#include <psyq/libgte.h>

/// Draws one semi-transparent textured quad centred on the projection of the
/// world-space point `pos`; `column` picks the texture column and `size` is a
/// signed half-extent scaled by depth.
void func_mine_gorge_8017DB88(SVECTOR* pos, s32 column, s32 size);

#endif // ROOMS_MINE_GORGE_H
