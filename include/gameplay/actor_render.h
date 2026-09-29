#ifndef GAMEPLAY_ACTOR_RENDER_H
#define GAMEPLAY_ACTOR_RENDER_H

#include "main/coord.h"
#include "main/display_types.h"

/// Composes a node and its ancestors through the top of their chain.
///
/// Clear `composeStamp` before changing a node's local matrix or parent.
/// Model nodes beneath `gGfxViewCoord` produce a local-to-view `workm`.
void Gp_UpdateCoord(GfxCoord* coord);

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GpuOtBuf* arg0);

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GpuOtBuf* arg0);

#endif // GAMEPLAY_ACTOR_RENDER_H
