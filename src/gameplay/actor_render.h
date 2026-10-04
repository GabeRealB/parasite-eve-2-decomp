#ifndef GAMEPLAY_PRIVATE_ACTOR_RENDER_H
#define GAMEPLAY_PRIVATE_ACTOR_RENDER_H

#include "main/coord.h"

/// Refreshes a coordinate below an excluded ancestor, with view rebasing for parentless nodes.
///
/// For a parented node, `excludedAncestor` is NULL to include the complete chain or a
/// strict ancestor whose transform is excluded. Excluding `gGfxViewCoord`
/// composes world-space matrices for room lights. At the excluded ancestor,
/// a dirty node copies its local matrix; a nonzero rebuild stamp keeps its
/// existing cache. Reusing nodes composed in another space requires
/// invalidating affected caches: the stamp does not identify the root.
///
/// For a parentless node, ignores `excludedAncestor`, composes the node's cache, then
/// writes `coord->coord` relative to the already composed `gGfxViewCoord.workm`.
/// Leaves `workm`, the resulting stamp and the NULL parent intact. The view
/// reference must be current and have an orthonormal rotation for rebasing.
///
/// `coord` must be non-NULL; it and its borrowed ancestors must remain live
/// and writable during the call. The parent chain must be acyclic. Clear
/// `composeStamp` when changing a local matrix or parent. Uses the current
/// pass's stamp and parity without advancing it. Matrix coefficients have
/// 12 fractional bits; translations use signed coordinate units. Stored
/// Euler angles are not applied, and GTE working registers are clobbered.
void actorRenderComposeCoordRelative(GfxCoord* coord, GfxCoord* excludedAncestor);

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
