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

void func_acropolis_security_room_801805A4(Task* task);

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

void func_acropolis_security_room_8017ED68(Task* task);

void func_acropolis_security_room_80180294(Task* task);

void func_acropolis_security_room_8017D984(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SECURITY_ROOM_H
