#ifndef INCLUDE_WEAPONS_M4A1_BAYONET_H
#define INCLUDE_WEAPONS_M4A1_BAYONET_H

#include "main/task_types.h"

/// Records the M4A1 bayonet's thrust and draws its fading blue-white ribbon.
///
/// Requires the bayonet overlay loaded, a live single-coordinate task body,
/// and its owned `EffectWork` in `spawnArg2.pointer`. The work's borrowed
/// parent is the weapon coordinate and must remain live. State 0 seeds both
/// eight-frame histories; state 1 records and draws seven ribbon segments.
/// Only running room effects advance age; paused or hidden effects retain
/// the task without drawing. Age 13 or cancellation frees its work and kills the task.
/// The histories belong to the overlay, so simultaneous instances share them.
/// Requires a current view, frame-arena room for seven quads and blend commands,
/// and an initialized scratch stack with 48 free bytes, released before return.
void m4a1BayonetTrailTask(Task* task);

#endif // INCLUDE_WEAPONS_M4A1_BAYONET_H
