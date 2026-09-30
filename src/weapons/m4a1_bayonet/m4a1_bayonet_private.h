#ifndef SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H
#define SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H

#include "main/coord.h"

/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `gGfxViewCoord`. The sweep state overwrites slot
/// `EffectWork::age & 7` each frame and the ribbon is drawn between the
/// two rings.
extern GfxCoord gBladeTrailBase[8];

extern GfxCoord gBladeTrailTip[8];

#endif // SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H
