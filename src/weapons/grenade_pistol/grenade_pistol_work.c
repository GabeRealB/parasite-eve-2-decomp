#include "common.h"

#include <psyq/libgte.h>

#include "weapons/grenade_pistol.h"
#include "weapons/weapons_shared_8011d3a0.h"

/// The shared spawn state's tables sit in the middle of this package's
/// trailing data, so they are their own unit: splat lists an object in the
/// linker script at its first subsegment, and these have to link between the
/// two runs of split data around them.
SVECTOR WeaponsShared8011d3a0Vecs[2] = {
    { 0, 0x1E0, 0x80, 0 },
    { 0, 0x220, 0x80, 0 },
};

/// Impact clip id per attachment, indexed by `sfx - 0xA`.
u16 D_grenade_pistol_8012B430[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

/// Per-ammo launch speed.
u8 WeaponsShared8011d3a0Speeds[4] = { 0x0C, 0x08, 0, 0 };
