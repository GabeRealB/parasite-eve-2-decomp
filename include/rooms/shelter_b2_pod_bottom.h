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

/// Runs the pod-bottom room's entry setup, idle or teardown state.
///
/// The map overlay spawns this bodyless task at state 0. State 0 registers it
/// for room messages and selects the entry scene or boss start; state 1 idles
/// and state 2 kills it. Only 0..2 are valid; spawn arguments are unused. Keep
/// the room and its scene/boss resources loaded until their tasks end.
void shelterB2PodBottomRoomTask(Task* task);

/// Spawns one or two rising energy sparks on a random model joint while running.
///
/// Requires a live TMD task with at least twenty coordinates; selects parts
/// 2..19 using the shared LCG's upper half. Both sparks share that joint, size
/// 1536 and random palettes. The second spawns when the next random upper-half
/// bit 0 is clear. Nonzero room-effect control leaves both RNG and effects alone.
/// Spawning snapshots the selected joint's placement; the sparks then move
/// independently in their own coordinates.
void shelterB2PodBottomSpawnJointEnergySparks(Task* task);

/// Spawns a flash and an optional fading spark on a random model joint while running.
///
/// Requires a live TMD task with at least twenty coordinates; selects parts
/// 2..19 using the shared LCG's upper half. Both effects share base size 768 and
/// palette 1. The fading spark spawns when the next upper-half bit 0 is set.
/// Nonzero room-effect control leaves both RNG and effects alone. Spawning
/// snapshots the joint's placement; these effects then draw independently.
void shelterB2PodBottomSpawnJointFlash(Task* task);

/// Advances the pod bottom's drifting animated sprite and releases it at completion.
///
/// `task` owns initialized `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body, normally supplied by `effectSpawn` with state 0 and cell index 0.
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

/// Expands and dims the pod bottom's blue shock ring over eight running updates.
///
/// Owns spawner-initialized `EffectWork` and a coordinate body. On state 0,
/// `spawnArg1.value` composes an X rotation in 4096-per-turn angle units; callers
/// use 0 or half a turn. Drawing uses the existing cache on that update.
/// Radius/brightness start at 384/192; each running update draws then adds 96
/// to radius and subtracts 24 from brightness, releasing work and task when
/// brightness falls below 24. Nonzero controls below 4 freeze ramps/age but
/// still draw. Controls >=4 release before drawing. Requires composed
/// coordinates, initialized scratch and packet capacity.
void shelterB2PodBottomShockRingTask(Task* task);

/// Charges a yellow starburst, then adds eight radial blades while it fades.
///
/// Owns spawner-initialized `EffectWork` and a coordinate body. `spawnArg1.value`
/// is a positive running-update countdown; integer 192/count sets brightness
/// growth. Initialization resets local rotation and rolls one angle in each
/// eighth turn into the room's shared blade table. A later burst can replace
/// those angles while an earlier burst is live. The growing phase draws the
/// starburst, a disc and a shrinking glow ring immediately. At zero countdown
/// brightness becomes 255 and fading updates add the blades, drawing before
/// dimming by 16. Brightness <=16 or controls >=4 release work and task.
/// Nonzero controls below 4 retain drawing, freeze growing age/countdown and
/// fading brightness, but fading age still advances. Requires composed
/// coordinates, initialized scratch and packet capacity.
void shelterB2PodBottomChargeBurstTask(Task* task);

/// Moves and draws the pod bottom's eight-cell rising sprite with a random palette.
///
/// Requires a counted coordinate-body task in state 0 with owned, zero-initialized
/// `EffectWork` in `spawnArg2.pointer` and a live `gRoomEffectState`. Bits 0..11 of
/// `spawnArg1.value` select the perspective size numerator; bit 16 reverses Y
/// motion, and other bits are ignored. The first running update chooses speed
/// 16..79 parent-coordinate units per update and a fixed screen angle in
/// 4096 units per turn. `move.vy` holds signed velocity, `scale` the angle,
/// and `angle` the size; these parameters retain signed halfword precision.
///
/// Running updates move along parent-space Y before drawing, dirty the transform
/// cache, and advance the cell every fourth update. Update 32 moves then releases
/// without drawing. Drawing uses the existing composed cache, requires scratch
/// and quad-packet capacity, and selects one of six palettes on every draw.
/// Controls 1..3 freeze initialization, motion and animation but retain drawing
/// and random consumption; controls >=4 release before drawing. Retirement frees
/// the work, decrements the effect count and tears down the task and body.
void shelterB2PodBottomEffectSpriteRiseTask(Task* task);

/// Moves a flickering light beam along local Y for sixteen running updates.
///
/// Owns spawner-initialized `EffectWork` and a coordinate body. `spawnArg1.value`
/// is signed Y displacement per running update (callers use +/-768 coordinate
/// units). Drawing uses the existing composed cache after dirtying the local
/// transform. Updates 1..7 use full brightness; 8..16 scale RGB nibbles by
/// twice the remaining updates, retaining odd-frame flicker even at zero.
/// Nonzero controls below 4 freeze age/motion and draw a random nibble tint at
/// full brightness, advancing the random generator. Controls >=4 cancel before
/// drawing; update 16 draws then releases the work and task. Requires composed
/// coordinates, initialized scratch and packet capacity.
void shelterB2PodBottomLightBeamTask(Task* task);

/// Initializes the arc flash's segment phases and selects this room's ground shadow.
///
/// State 0 seeds three 16-entry rows with phases 0..255 and disables ground traces.
/// Every update disables the shadow in mapped view 15 (low byte), selecting
/// unmodulated shadow shading elsewhere. This callback leaves its lifetime to
/// task teardown; it must initialize before arc flashes use the phase table.
void shelterB2PodBottomShadowTask(Task* task);

/// Advances three textured bands, three rising glow rings and an additive screen flash.
///
/// Owns spawner-initialized `EffectWork` and a coordinate body. State 0 resets
/// local rotation and starts brightness at 160, returning without drawing.
/// Running updates dim by 8 and expand the bands; brightness <=8 ends the task
/// on the next update. Nonzero controls below 4 freeze age and ramps but draw
/// every layer; controls >=4 release work and task before drawing.
/// The glow rings successively offset the cached Y translation, which normal
/// coordinate composition must restore before the next update. Requires the
/// room's initialized phase table, composed coordinates, scratch and packets.
void shelterB2PodBottomArcFlashTask(Task* task);

/// Charges and fades two tinted discs with a cycling outer glow ring.
///
/// Owns spawner-initialized `EffectWork` and a coordinate body. `spawnArg1.value`
/// is a positive running-update countdown; callers use 32. Integer 192/count
/// is the brightness increment. Local rotation is reset on the first update;
/// the growing phase draws immediately and rerolls one of 18 RGB shift rows.
/// At zero countdown brightness becomes 255; the next phase draws and dims by
/// 16 until <=16, then releases work and task. Nonzero controls below 4 freeze
/// the growing age/countdown or fading brightness while retaining drawing;
/// fading age still advances. Controls >=4 cancel before drawing.
/// Requires composed coordinates, initialized scratch and packet capacity.
void shelterB2PodBottomEnergyRingTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
