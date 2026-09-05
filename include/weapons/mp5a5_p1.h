#ifndef WEAPONS_MP5A5_P1_H
#define WEAPONS_MP5A5_P1_H

#include "common.h"

#include <psyq/libgte.h>

/// Muzzle offset of the MP5A5(+1), in the firing hand's coordinate frame.
extern SVECTOR D_mp5a5_p1_8011E128;

/// The four flash angles rolled on the frame the shot goes off, one per
/// `WeaponsShared8011d864` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
extern s16 D_mp5a5_p1_8012B508[4];

#endif
