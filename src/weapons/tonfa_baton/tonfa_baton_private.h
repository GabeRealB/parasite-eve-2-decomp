#ifndef SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H
#define SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H

#include "main/coord.h"

/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `gGfxViewCoord`.
extern GfxCoord gBladeTrailBase[8];

extern GfxCoord gBladeTrailTip[8];

#endif // SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H
