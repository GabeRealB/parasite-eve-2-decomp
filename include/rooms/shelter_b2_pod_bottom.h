#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB2PodBottomModel0A2A0;

extern TmdSource gShelterB2PodBottomModel0A68C;

extern TmdSource gShelterB2PodBottomModel0AA08;

extern TmdSource gShelterB2PodBottomModel0AE48;

extern AreaVariant D_shelter_b2_pod_bottom_80187678[11];

// shelter_b2_pod_bottom
extern WorldCollisionRoomResources D_shelter_b2_pod_bottom_80181D14[];

extern WorldCoordRoomLighting D_shelter_b2_pod_bottom_80181D24[];

extern u8* D_shelter_b2_pod_bottom_80181D2C[];

extern ViewCount D_shelter_b2_pod_bottom_80181D30[];

extern DirectionWarpEntry D_shelter_b2_pod_bottom_80181D34[];

extern ViewCamera D_shelter_b2_pod_bottom_80182B80[];

extern SpriteView D_shelter_b2_pod_bottom_80185904[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_bottom_80188770[];

void func_shelter_b2_pod_bottom_8017D708(Task* task);

void func_shelter_b2_pod_bottom_80181A48(Task* task);

void func_shelter_b2_pod_bottom_80181940(Task* arg0);

/// Advances the pod bottom's drifting animated sprite and releases it at completion.
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
void shelterB2PodBottomEffectSpriteDriftTask(Task* task);

void func_shelter_b2_pod_bottom_80181B48(Task* arg0);

void func_shelter_b2_pod_bottom_8018016C(Task* task);

void shelterB2PodBottomEffectSpriteRiseTask(Task* task);

void func_shelter_b2_pod_bottom_80180F10(Task* arg0);

void func_shelter_b2_pod_bottom_8017D760(Task* task);

void func_shelter_b2_pod_bottom_8017EC78(Task* task);

void func_shelter_b2_pod_bottom_8017F448(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
