#ifndef INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
#define INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_observatory_80180DBC[2];

extern AreaVariant D_neo_ark_observatory_8018786C[13];

// neo_ark_observatory
extern WorldCollisionRoomResources D_neo_ark_observatory_80181594[];

/// Lighting for room configurations 1 and 2, indexed by the room ID minus one.
///
/// Both use 17 point lights and the default ambient minima. The light records
/// stay writable for coordinate composition and attenuation queries; all
/// borrowed pointers remain valid only while this room overlay is loaded.
extern WorldCoordRoomLighting gNeoArkObservatoryRoomLighting[2];

/// Logical-view maps for room configurations 1 and 2, indexed by the room ID minus one.
///
/// Each has 21 logical views, addressed at view minus one. Configuration 1
/// maps views 4..6 to camera/image/sprite indices 16..18; configuration 2 uses
/// the gameplay identity map. Room-owned pointers require this overlay loaded.
extern u8* gNeoArkObservatoryViewMaps[2];

extern ViewCount D_neo_ark_observatory_801815E4[];

extern DirectionWarpEntry D_neo_ark_observatory_801815E8[];

extern ViewCamera D_neo_ark_observatory_80181FC8[];

extern SpriteView D_neo_ark_observatory_801860E8[];

/// Collision and footstep properties indexed by the room-local surface class (0..7).
///
/// Every class blocks probes, enables weapon impacts and applies pushback.
/// Classes 1..3 have their own walking, running and scripted-jump cue bases;
/// classes 0 and 4..7 share a record without footstep cues. Borrowed records
/// and sound tables remain valid only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkObservatorySurfaceProperties[8];

/// Sets the base grey intensity of the observatory's light beams.
///
/// Stores the signed low halfword without clamping; room entry and scenes use
/// 0..160. The drawer adds a four-frame pulse of -4..4 and skips negative
/// results. This changes beams only; the view's glow discs retain their colours.
/// The room overlay must be loaded. The glow task resets the value on its first tick.
void neoArkObservatorySetLightBeamIntensity(s32 intensity);

/// Updates the companion obstacle reserved at the start of the room collision grid.
///
/// Uses the companion's model root, falling back to the player's. At least one
/// must be a live model task with a root mapping local geometry into room space.
/// Applies the room-axis offset (0, 0, -200) while a companion exists and
/// `GAME_FLAG_0D7` is nonzero; otherwise raises it by 10000 room units. The grid's
/// cell lists and remaining geometry stay intact. Requires the observatory
/// overlay; changes GTE state. The event-script argument is unused.
void neoArkObservatoryUpdateCompanionObstacle(s32 unusedEventArg);

/// Draws the observatory's glow discs and light beams for the mapped camera view.
///
/// Effect-table slot 0x14E resets beam intensity once in state 0, then draws
/// each tick in state 1. Requires the room overlay, composed view matrices,
/// initialized scratch stack, and the current frame's packet arena and ordering
/// table. It retains no pointers into those frame resources.
void neoArkObservatoryGlowTask(Task* effectTask);

/// Runs the observatory's room-message task and companion presentation.
///
/// Requires a live task in state 0..2: initialize the receiver, companion
/// obstacle and light beam; update companion visibility each tick; then
/// teardown. The room overlay must remain loaded through dispatch.
void neoArkObservatoryRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
