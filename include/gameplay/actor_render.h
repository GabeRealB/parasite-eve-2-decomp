#ifndef GAMEPLAY_ACTOR_RENDER_H
#define GAMEPLAY_ACTOR_RENDER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/coord.h"

/// Composes a node and its ancestors through the top of their chain.
///
/// Clear `composeStamp` before changing a node's local matrix or parent.
/// Model nodes beneath `gGfxViewCoord` produce a local-to-view `workm`.
void Gp_UpdateCoord(GfxCoord* coord);

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GsOT* arg0);

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GsOT* arg0);

#endif // GAMEPLAY_ACTOR_RENDER_H
