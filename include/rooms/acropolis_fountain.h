#ifndef INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
#define INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_fountain_8017FC9C[11];

// acropolis_fountain
extern WorldCollisionRoomResources D_acropolis_fountain_8017E814[];

extern u8* D_acropolis_fountain_8017E84C[];

extern ViewCount D_acropolis_fountain_8017E854[];

extern WorldCoordRoomLighting D_acropolis_fountain_8017E858[];

extern DirectionWarpEntry D_acropolis_fountain_8017E868[];

extern SpriteView D_acropolis_fountain_8018375C[];

extern ViewCamera D_acropolis_fountain_80183864[];

extern WorldCollisionSurfaceProperties* D_acropolis_fountain_80183B90[];

/// Draws the fountain spray as a flickering, semitransparent textured quad.
///
/// Requires a live coordinate body, the room's texture and palette, and space
/// for one `POLY_FT4` in the frame arena. Draws in raw camera views 7, 8, 15 and
/// 21 while room effects are active. Reserves and releases scratch storage and
/// consumes a packet even when projected depth (SZ3 / 4) is below 17.
void acropolisFountainSprayTask(Task* task);

/// Queues the player's fountain climb from the direction-action dispatch.
///
/// Both callback arguments are ignored. Requires the loaded fountain overlay
/// and live player through climb completion. Task-allocation failure is ignored.
void acropolisFountainStartClimb(s32 unusedActionId, s32 unusedControl);

/// Spawns the fountain spray and replaces its movie task when the mapped view changes.
///
/// Effect-bank entry 0x026 starts in state 0 with a live coordinate body and
/// effect-owned `EffectWork` in `spawnArg2.pointer`. Its `scale` halfword latches
/// the 1-based mapped camera index, 1..22 for this room. Indices 2..8 and 17
/// start the movie; changing away from any of them queues its stop callback,
/// then starts the new view on the next update. Logical views sharing an index
/// do not restart it. The task draws nothing and has no task-work allocation.
/// Requires loaded room/view/movie resources. Only one controller may run,
/// and each queued stop requires the published movie-task handle still live.
/// Spawn failures are unchecked and the stop callback leaves that handle set.
void acropolisFountainViewEffectsTask(Task* task);

/// Runs the player's turn, one-step ascent and walk to the room's fixed destination.
///
/// Spawned from task bank 2, slot 14 with state zero. The player task must remain
/// live through the six phases (0 turn, 1 await turn, 2 ascend, 3 await ascent,
/// 4 walk, 5 await arrival). Completion restores player control and kills this
/// task. Its state is an unchecked index maintained only by these callbacks.
void acropolisFountainClimbTask(Task* task);

/// Runs the fountain's room-message receiver and restores its climb interaction.
///
/// State 0 registers the receiver and restores the saved climb trigger;
/// state 1 idles with messages available; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisFountainRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
