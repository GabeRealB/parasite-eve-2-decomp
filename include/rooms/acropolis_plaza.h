#ifndef INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
#define INCLUDE_ROOMS_ACROPOLIS_PLAZA_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Native animation sets shared with the companion actor overlay.
extern AnimationSet gAcropolisPlazaAnimation148BC;

extern AnimationSet gAcropolisPlazaAnimation156B4;

extern AnimationSet gAcropolisPlazaAnimation15864;

extern AnimationSet gAcropolisPlazaAnimation15CC8;

extern AnimationSet gAcropolisPlazaAnimation15F78;

extern AnimationSet gAcropolisPlazaAnimation1622C;

extern AnimationSet gAcropolisPlazaAnimation1643C;

extern AnimationSet gAcropolisPlazaAnimation16610;

extern AnimationSet gAcropolisPlazaAnimation168FC;

extern AnimationSet gAcropolisPlazaAnimation16D00;

extern AnimationSet gAcropolisPlazaAnimation16EE4;

extern AnimationSet gAcropolisPlazaAnimation170C0;

extern AnimationSet gAcropolisPlazaAnimation17298;

extern AnimationSet gAcropolisPlazaAnimation1754C;

extern AnimationSet gAcropolisPlazaAnimation17818;

extern AnimationSet gAcropolisPlazaAnimation17A00;

extern AnimationSet gAcropolisPlazaAnimation17C68;

extern AnimationSet gAcropolisPlazaAnimation17E54;

extern AnimationSet gAcropolisPlazaAnimation17FF4;

extern AnimationSet gAcropolisPlazaAnimation181CC;

extern AnimationSet gAcropolisPlazaAnimation183B8;

extern AnimationSet gAcropolisPlazaAnimation18614;

extern AnimationSet gAcropolisPlazaAnimation18904;

extern AnimationSet gAcropolisPlazaAnimation18DE0;

extern AnimationSet gAcropolisPlazaAnimation18F98;

extern AnimationSet gAcropolisPlazaAnimation19610;

extern AnimationSet gAcropolisPlazaAnimation19AD8;

extern AnimationSet gAcropolisPlazaAnimation19D64;

extern AnimationSet gAcropolisPlazaAnimation19F7C;

extern AnimationSet gAcropolisPlazaAnimation1A4F8;

extern AnimationSet gAcropolisPlazaAnimation1A784;

extern AnimationSet gAcropolisPlazaAnimation1AAAC;

extern AnimationSet gAcropolisPlazaAnimation1AE04;

extern AnimationSet gAcropolisPlazaAnimation1AFA4;

extern AnimationSet gAcropolisPlazaAnimation1B1F8;

extern TaskDesc D_acropolis_plaza_80183824[12];

// acropolis_plaza
extern WorldCollisionRoomResources D_acropolis_plaza_801988B8[];

extern u8* D_acropolis_plaza_801988C8[];

extern ViewCount D_acropolis_plaza_801988CC[];

extern WorldCoordRoomLighting D_acropolis_plaza_801988D0[];

extern ViewCamera D_acropolis_plaza_801988D8[];

extern SpriteView D_acropolis_plaza_80198A08[];

extern DirectionWarpEntry D_acropolis_plaza_80198A68[];

extern WorldCollisionSurfaceProperties* D_acropolis_plaza_80199F28[];

extern AreaVariant D_acropolis_plaza_80199390[3];

void func_acropolis_plaza_8018251C(Task* task);

/// Draws a stationary flickering or pulsing additive glow at a placed light.
///
/// Bank-6 effect 0x096 requires a coordinate body and the current view, scratch
/// stack, primitive arena and depth ordering table. `spawnArg1.value` is the
/// placement slot: the plaza passes 12..18; below 16 the glow flickers yellow,
/// otherwise it pulses red. Each visible tick queues eight Gouraud wedges and
/// their blend commands. The callback keeps no pointer into scratch storage.
void acropolisPlazaLightGlowTask(Task* task);

/// Sweeps a siren beam, its point light and its near and far additive glows.
///
/// Bank-6 effect 0x098 requires a coordinate body and `effectSpawn`'s live
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is the placement slot
/// (the plaza passes 1..6): below 5 is blue, otherwise red; its low three bits
/// select the transient point-light slot. Parity seeds opposite yaw phases.
/// `scale` is yaw in 4096 units per turn, decreasing by 128 per tick; `angle`
/// is the selected 512/2048-unit reach and `period` the point-light falloff
/// width. Requires current view, scratch, packet and ordering-table storage;
/// nothing in scratch survives the tick. Effect teardown owns the work block.
void acropolisPlazaSirenLightTask(Task* task);

/// Sweeps a pulsing four-ray flare and a flickering glow ahead of a placed light.
///
/// Bank-6 effect 0x099 requires a coordinate body and `effectSpawn`'s live
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is the placement slot
/// (the plaza passes 7..10): below 9 is blue, otherwise red. Parity seeds
/// opposite yaw phases; `scale` advances by -128 in 4096 units per turn.
/// `angle` gates the far glow with a 512/2048-unit reach selection; `period`
/// is untouched. Requires current view, scratch, packet and ordering-table
/// storage. Scratch is released each tick; effect teardown owns the work block.
void acropolisPlazaLightFlareTask(Task* task);

void func_acropolis_plaza_8017D6D4(void);

#endif // INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
