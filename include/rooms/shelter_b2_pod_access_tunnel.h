#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23];

// shelter_b2_pod_access_tunnel
extern WorldCollisionRoomResources D_shelter_b2_pod_access_tunnel_80183DEC[];

extern WorldCoordRoomLighting D_shelter_b2_pod_access_tunnel_80183E0C[];

extern u8* D_shelter_b2_pod_access_tunnel_80183E24[];

extern ViewCount D_shelter_b2_pod_access_tunnel_80183E2C[];

extern DirectionWarpEntry D_shelter_b2_pod_access_tunnel_80183E30[];

extern ViewCamera D_shelter_b2_pod_access_tunnel_801841D8[];

extern SpriteView D_shelter_b2_pod_access_tunnel_80184C6C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_access_tunnel_801856D8[];

void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task);

/// Advances the pod access tunnel's drifting animated sprite and releases it at completion.
///
/// `task` owns initialized `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body, normally supplied by `Gp_SpawnEff` with state 0 and cell index 0.
/// The first running update initializes without drawing; later updates draw
/// before moving, accelerating and advancing through 12 banked or 10 alternate
/// cells. Nonzero `RoomEffectState::effectControl` freezes updates and redraws
/// the current drawer and palette, including hidden control. Values >= 4 free
/// work and task after that final draw; borrowed work pointers then expire.
/// Drawing requires a composed coordinate, initialized scratch and packet space.
///
/// `spawnArg1.value` packs perspective size in bits 0..11, frames per cell in
/// 12..14, speed in 16..23 (0 means 64 coordinate units per running update),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// Frames per cell default to 1 only when bits 12..15 are all zero; otherwise
/// bits 12..14 must encode 1..7. Bit 15 alone encodes an invalid zero divisor.
/// Initial spin uses 4096 units per turn.
///
/// Existing nonzero velocity is retained. For initially zero velocity, kind 0
/// disables movement; kinds 1/2/3/6 select random upward/all-axis/narrow-upward/
/// planar directions and normalize to the selected speed. Kind 5 takes the
/// offset's Y/Z and the saved palette bits as X. Moving updates subtract 2/1
/// from banked/alternate Y velocity; kind 7 instead adds age/10. Signed-halfword
/// velocity components wrap on assignment; positions use coordinate units.
void shelterB2PodAccessTunnelEffectSpriteDriftTask(Task* task);

void func_shelter_b2_pod_access_tunnel_80181C2C(Task* arg0);

void func_shelter_b2_pod_access_tunnel_80182690(Task* task);

void func_shelter_b2_pod_access_tunnel_80182F78(Task* task);

void func_shelter_b2_pod_access_tunnel_8017F608(Task* task);

void func_shelter_b2_pod_access_tunnel_80180350(Task* arg0);

void func_shelter_b2_pod_access_tunnel_801806E8(Task* arg0);

void func_shelter_b2_pod_access_tunnel_80181AF8(Task* arg0);

void func_shelter_b2_pod_access_tunnel_8017DC6C(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
