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
extern AnimationSet D_acropolis_plaza_80191E7C;

extern AnimationSet D_acropolis_plaza_80192C74;

extern AnimationSet D_acropolis_plaza_80192E24;

extern AnimationSet D_acropolis_plaza_80193288;

extern AnimationSet D_acropolis_plaza_80193538;

extern AnimationSet D_acropolis_plaza_801937EC;

extern AnimationSet D_acropolis_plaza_801939FC;

extern AnimationSet D_acropolis_plaza_80193BD0;

extern AnimationSet D_acropolis_plaza_80193EBC;

extern AnimationSet D_acropolis_plaza_801942C0;

extern AnimationSet D_acropolis_plaza_801944A4;

extern AnimationSet D_acropolis_plaza_80194680;

extern AnimationSet D_acropolis_plaza_80194858;

extern AnimationSet D_acropolis_plaza_80194B0C;

extern AnimationSet D_acropolis_plaza_80194DD8;

extern AnimationSet D_acropolis_plaza_80194FC0;

extern AnimationSet D_acropolis_plaza_80195228;

extern AnimationSet D_acropolis_plaza_80195414;

extern AnimationSet D_acropolis_plaza_801955B4;

extern AnimationSet D_acropolis_plaza_8019578C;

extern AnimationSet D_acropolis_plaza_80195978;

extern AnimationSet D_acropolis_plaza_80195BD4;

extern AnimationSet D_acropolis_plaza_80195EC4;

extern AnimationSet D_acropolis_plaza_801963A0;

extern AnimationSet D_acropolis_plaza_80196558;

extern AnimationSet D_acropolis_plaza_80196BD0;

extern AnimationSet D_acropolis_plaza_80197098;

extern AnimationSet D_acropolis_plaza_80197324;

extern AnimationSet D_acropolis_plaza_8019753C;

extern AnimationSet D_acropolis_plaza_80197AB8;

extern AnimationSet D_acropolis_plaza_80197D44;

extern AnimationSet D_acropolis_plaza_8019806C;

extern AnimationSet D_acropolis_plaza_801983C4;

extern AnimationSet D_acropolis_plaza_80198564;

extern AnimationSet D_acropolis_plaza_801987B8;

extern TaskDesc D_acropolis_plaza_80183824[12];

// acropolis_plaza
extern GpRoomObjRec D_acropolis_plaza_801988B8[];

extern u8* D_acropolis_plaza_801988C8[];

extern GpViewCountRec D_acropolis_plaza_801988CC[];

extern WorldCoordRoomLighting D_acropolis_plaza_801988D0[];

extern GpViewRec D_acropolis_plaza_801988D8[];

extern SpriteView D_acropolis_plaza_80198A08[];

extern GpWarpRec D_acropolis_plaza_80198A68[];

extern WorldCollisionSurfaceProperties* D_acropolis_plaza_80199F28[];

extern GpAreaVariant D_acropolis_plaza_80199390[3];

void func_acropolis_plaza_8018251C(Task* task);

void func_acropolis_plaza_80182054(Task* task);

void func_acropolis_plaza_801802C0(Task* task);

void func_acropolis_plaza_801811D0(Task* task);

void func_acropolis_plaza_8017D6D4(void);

#endif // INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
