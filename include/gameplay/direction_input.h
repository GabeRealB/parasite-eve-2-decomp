#ifndef GAMEPLAY_DIRECTION_INPUT_H
#define GAMEPLAY_DIRECTION_INPUT_H

#include "types.h"

extern u8 D_80114CF8;

/// The bytes 1 to 50 in order. Room data uses it as the view list of a
/// location whose views are numbered in their own order, so the rooms share
/// this one instead of carrying a copy each.
extern u8 D_8010CAF8[];

extern s16 D_80114D08;

#endif // GAMEPLAY_DIRECTION_INPUT_H
