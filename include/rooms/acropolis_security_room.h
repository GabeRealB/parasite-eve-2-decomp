#ifndef INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H
#define INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_acropolis_security_room_80184088[4];

extern TmdSource gAcropolisSecurityRoomAcropolisSanctuaryModel090F0;

// acropolis_security_room
extern WorldCollisionRoomResources D_acropolis_security_room_801839D0[];

extern u8* D_acropolis_security_room_801839E0[];

extern ViewCount D_acropolis_security_room_801839E4[];

extern WorldCoordRoomLighting D_acropolis_security_room_801839E8[];

extern DirectionWarpEntry D_acropolis_security_room_801839F0[];

extern SpriteView D_acropolis_security_room_80184C50[];

extern ViewCamera D_acropolis_security_room_80184D10[];

extern WorldCollisionSurfaceProperties* D_acropolis_security_room_80184FA0[];

/// Updates the security monitor's four feed palettes and draws their pictures or glows.
///
/// Effect slot 0x48 requires a coordinate body and cleared `EffectWork` in
/// `spawnArg2.pointer`. State 0 seeds all four 256-colour CLUTs from the off
/// palette; state 1 selects lit feeds from the two lock-release bits (0..3).
/// In mapped view 6, `scale`, `angle`, `period` and `step` are feed 0..3's
/// brightness weights in 1/4096 units, rising by 512 to an alternating
/// 4096/3584 limit, or zero for an unlit feed. Other views draw active glows,
/// except views 8 and 16. Route progress below 3 also enables the sweep line.
/// Retains the work for subsequent frames; this callback does not release it.
void acropolisSecurityRoomMonitorFeedsTask(Task* task);

/// Draws one 128x128 security-camera picture and retires the counted effect.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`.
/// `spawnArg1`'s low two bits select one of four feed placements and palettes;
/// the picture uses raw additive 8-bit texturing at sorting depth 48. Consumes
/// one frame-arena quad, then releases the work and task before returning.
void acropolisSecurityRoomMonitorFeedTask(Task* task);

/// Draws and tumbles an opaque textured square until it settles in room view 15.
///
/// Effect-table slot 0x7B requires a coordinate body and a counted `EffectWork`
/// cleared at spawn. The local XZ square has half-size 32 coordinate units.
/// X/Z tumble rates use 4096 units per turn; move is parent-space units per
/// frame. At parent Y >= -419 motion stops, retaining the pose. The quad keeps
/// drawing until view 15 is left, then frees its work and task. Each draw needs
/// one quad packet and a temporary `EffectQuadCornersScratch` block.
void acropolisSecurityRoomFallingQuadTask(Task* task);

/// Draws one flickering additive monitor glow and retires the counted effect.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`.
/// `spawnArg1` bit 1 enables red and bit 0 green. A shared random brightness
/// (64..176 in steps of 16) lights two Gouraud wedges and two crossing lines.
/// Half-extent is 3072 / (camera Z / 4), in screen pixels; depths below 17 are
/// rejected. Releases its scratch, counted work and task on every path.
void acropolisSecurityRoomMonitorGlowTask(Task* task);

/// Runs the security monitor's camera-selection and brightness panel.
///
/// Starts at state 0; states 0..6 allocate panel work, arm/scan the cursor,
/// open/handle hotspot commands, close, or scan the restricted panel.
/// Owns its work and cursor child until the opener polls its result 0.
/// The security-room overlay must remain loaded throughout the task's lifetime.
void acropolisSecurityRoomMonitorTask(Task* task);

/// Runs the power-supply panel's lock prompts and unlock scenes.
///
/// Starts at state 0; states 0..5 set up and operate the panel, states 6..9
/// release the blue-key left lock and states 10..15 the red-key right lock.
/// Owns its work and cursor/scene children until the opener polls result 0.
/// The security-room overlay must remain loaded throughout the task's lifetime.
void acropolisSecurityRoomPowerSupplyTask(Task* task);

/// Keeps the security room's transition, action and key-item messages available.
///
/// States 0..2 register the room receiver, idle and retire it. Starts at 0;
/// the Acropolis map selects this callback for stage 1, area 6. The room
/// overlay must remain loaded while the message task is alive.
void acropolisSecurityRoomMessageTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H
