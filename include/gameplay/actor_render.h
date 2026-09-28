#ifndef GAMEPLAY_ACTOR_RENDER_H
#define GAMEPLAY_ACTOR_RENDER_H

#include "main/coord.h"
#include "main/display_types.h"

void Gp_UpdateCoord(GpCoord* arg0);

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GpuOtBuf* arg0);

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GpuOtBuf* arg0);

#endif // GAMEPLAY_ACTOR_RENDER_H
