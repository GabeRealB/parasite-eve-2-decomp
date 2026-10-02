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

extern GpAreaVariant D_acropolis_plaza_80199390[3];

void func_acropolis_plaza_8018251C(Task* task);

void func_acropolis_plaza_80182054(Task* task);

void func_acropolis_plaza_801802C0(Task* task);

void func_acropolis_plaza_801811D0(Task* task);

void func_acropolis_plaza_8017D6D4(void);

#endif // INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
