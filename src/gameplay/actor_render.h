#ifndef GAMEPLAY_PRIVATE_ACTOR_RENDER_H
#define GAMEPLAY_PRIVATE_ACTOR_RENDER_H

#include "main/coord.h"

/// Composes a node relative to `root`, excluding that ancestor's transform.
///
/// For a parentless node, composes through the top of the chain instead and
/// converts `coord` into the current view node's local space. Parent links
/// remain borrowed, and a supplied root must lie on the chain.
void Gp_UpdateCoordEx(GfxCoord* coord, GfxCoord* root);

#endif // GAMEPLAY_PRIVATE_ACTOR_RENDER_H
