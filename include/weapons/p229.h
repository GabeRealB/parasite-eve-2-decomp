#ifndef WEAPONS_P229_H
#define WEAPONS_P229_H

#include "common.h"

#include <psyq/libgte.h>

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
extern SVECTOR D_p229_8011E0F0;

/// The four flash angles rolled on the frame the shot goes off, one per
/// `WeaponsShared8011d864` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
extern s16 D_p229_8012B658[4];

#endif
