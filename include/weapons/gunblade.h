#ifndef WEAPONS_GUNBLADE_H
#define WEAPONS_GUNBLADE_H

#include "common.h"

/// The gunblade's work block, cached in `D_gunblade_8012E248` while the weapon
/// task runs. Two 16-bit counters lead the region this overlay touches.
typedef struct GunbladeWork {
    /* 0x00 */ byte pad_0[0x20];
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
} GunbladeWork;

/// The overlay's view of the owning `Task`, cached in `D_gunblade_8012E244`:
/// the work pointer it reads on entry and the field the callback writes back.
typedef struct GunbladeTask {
    /* 0x00 */ byte          pad_0[0x20];
    /* 0x20 */ GunbladeWork* field_20;
    /* 0x24 */ byte          pad_24[0x10];
    /* 0x34 */ s32           field_34;
} GunbladeTask;

extern GunbladeTask* D_gunblade_8012E244;
extern GunbladeWork* D_gunblade_8012E248;

void func_gunblade_8011E008(s32 arg0);

#endif
