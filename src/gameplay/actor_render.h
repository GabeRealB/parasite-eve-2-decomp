#ifndef GAMEPLAY_PRIVATE_ACTOR_RENDER_H
#define GAMEPLAY_PRIVATE_ACTOR_RENDER_H

#include "main/coord.h"

/// Composes a node relative to `root`, excluding that ancestor's transform.
///
/// For a parentless node, composes through the top of the chain instead and
/// converts `coord` into the current view node's local space. Parent links
/// remain borrowed, and a supplied root must lie on the chain.
void Gp_UpdateCoordEx(GfxCoord* coord, GfxCoord* root);

/// Composes `coord` and its ancestors, stopping before `root`.
///
/// `stamp` is the composition pass counter's low 31 bits, in `0..0x7FFFFFFF`,
/// and `parity` is that counter's low bit (0 even, 1 odd). Ancestors are
/// composed first. A node rebuilds `workm` from its parent's cache and its
/// local matrix when its rebuild stamp is older than its parent's. At the
/// excluded ancestor, a clear stamp copies the local matrix into `workm`; any
/// other stamp keeps the cached matrix. An unchanged node keeps its matrix
/// and its rebuild stamp. Every visit stores `parity` in bit 31 of
/// `composeStamp`.
///
/// `root == NULL` includes the whole chain, through the parentless node. A
/// non-NULL `root` must lie on `coord`'s parent chain, and the chain must be
/// acyclic. Parent links are borrowed and must stay live for the walk. Clear
/// `composeStamp` after changing a local matrix or parent. Stored Euler
/// angles are not applied here.
void actorRenderComposeCoordChain(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

#endif // GAMEPLAY_PRIVATE_ACTOR_RENDER_H
