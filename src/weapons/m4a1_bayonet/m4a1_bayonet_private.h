#ifndef SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H
#define SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H

#include "main/coord.h"

/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `gGfxViewCoord`. The sweep state overwrites slot
/// `GpEffWork::age & 7` each frame and the ribbon is drawn between the
/// two rings.
extern GpCoord D_m4a1_bayonet_8012D398[8];

extern GpCoord D_m4a1_bayonet_8012D618[8];

#endif // SRC_WEAPONS_M4A1_BAYONET_M4A1_BAYONET_PRIVATE_H
