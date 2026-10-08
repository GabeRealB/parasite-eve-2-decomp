#ifndef INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
#define INCLUDE_ROOMS_NEO_ARK_BRIDGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_bridge_801820BC[4];

extern SVECTOR D_neo_ark_bridge_801820CC[9];

extern s16 D_neo_ark_bridge_80181FF8;

extern TaskDesc D_neo_ark_bridge_80181F18;

extern AreaVariant D_neo_ark_bridge_80184A50[13];

// neo_ark_bridge
extern u8* D_neo_ark_bridge_80181F80[];

extern ViewCount D_neo_ark_bridge_80181F84[];

extern DirectionWarpEntry D_neo_ark_bridge_80181F88[];

extern WorldCollisionGrid D_neo_ark_bridge_80182814;

extern ViewCamera D_neo_ark_bridge_80182838[];

extern SpriteView D_neo_ark_bridge_80184564[];

extern WorldCoordRoomLights D_neo_ark_bridge_8018470C;

extern WorldCollisionTrigger D_neo_ark_bridge_80184724[];

extern WorldCollisionTrigger D_neo_ark_bridge_80184AB8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_bridge_80184BD4[];

/// Installs bridge effect variants and updates its side-view smoke and red marker.
///
/// State 0 selects the room's five effect IDs, then state 1 updates mapped views
/// 5 and 6. Running effects independently try each smoke site with probability
/// 1/4 per tick; other controls suppress smoke. The red marker still draws in
/// both views, pulsing by 1536 angle units per animation frame (4096 per turn)
/// with radius scale 192. The task stays alive and uses no body or spawn payload.
/// Requires this overlay, room-effect state, view matrices, initialized scratch
/// stack and frame primitive space. Exported to gameplay's room-effect table.
void neoArkBridgeAmbientEffectsTask(Task* task);

/// Advances and draws an expanding, fading water-surface ripple for the bridge.
///
/// Requires a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units (0..4095); higher bits are ignored.
/// Each running update grows it by 32, draws, then dims by 2, for 32 draws at
/// brightness 64..2. The first update sets a random yaw, composed on the next.
/// Non-running controls redraw retained values; cancellation draws once before
/// retiring. Retirement frees the work and coordinate body and decrements the
/// effect count. The room overlay and water textures must stay loaded, with
/// initialized scratch storage and frame packet space available for drawing.
void neoArkBridgeWaterRippleTask(Task* task);

/// Advances and draws an eight-cell water-spray particle for the bridge.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and animation index 0. `spawnArg1` bits
/// 0..11 give perspective size, 12..15 updates per cell (0 selects 1), and
/// 16..23 launch speed in parent-space coordinate units per update (0 selects 64).
/// Bits 24..27 choose direction (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 spawn-offset direction; others leave it zero).
/// A supplied nonzero `EffectWork::move` bypasses direction generation and
/// scaling. Nonzero bits 28..31 select an upright tile; otherwise the sprite
/// retains a random angle at 4096 units per turn.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to signed-halfword Y velocity, retaining its
/// low 16 bits; stationary particles skip movement and gravity. Cells 0..7
/// each last the decoded period. Suspended controls redraw retained state;
/// cancellation retires without drawing. Retirement frees the work and body
/// and decrements the effect count. The room overlay and water textures must
/// stay loaded, with initialized scratch storage and frame packet space.
void neoArkBridgeWaterDriftTask(Task* task);

/// Runs the bridge's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer` and initial state 0. `spawnArg1.value` starts as a
/// positive charge duration in running updates and is consumed as a countdown.
/// Nonzero room effect controls pause without drawing; controls 4 and above,
/// state 3 or fade completion retire the work, task and body and decrement
/// the effect count. The room overlay must remain loaded, with scratch and
/// frame packet storage available while drawing.
void neoArkBridgeRoomVisualEffectsFlashTask(Task* task);

/// Draws a fading blue beam between two offsets on the bridge effect's parent.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, initial state 0 and null `Task::work`. Its borrowed
/// `EffectWork::parent` coordinate and the room overlay must remain live.
/// Allocates eight world-space snapshots per endpoint in `Task::work`;
/// allocation failure resets age and retries on the next active update.
/// `spawnArg1.value` is the release age in active updates (2..32767 before
/// signed-halfword age wraps; 0 leaves lifetime to external teardown).
/// Initialization advances age without drawing or testing expiry. Control
/// values below 2 advance the trail; values 2 and above freeze without drawing
/// or canceling it. Teardown frees both histories, effect work and the body.
/// Drawing requires initialized scratch storage and frame packet space.
void neoArkBridgeRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the bridge's impact flash followed by smoke or rings and bouncing sparks.
///
/// Requires a counted effect with a coordinate body, owned zero-aged `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. Nonzero `spawnArg1.value` selects
/// smoke; zero selects two sparks and fading orange rings. Running updates enter
/// release at age 7 and retire at age 8, freeing work, body and task. Child effects
/// live independently. Room control 1..3 pauses and 4 or above cancels immediately.
/// Keep the room overlay, effect resources, scratch and frame packet space live.
void neoArkBridgeRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the Neo Ark bridge's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void neoArkBridgeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_BRIDGE_H
