#ifndef ACTOR_510900_H
#define ACTOR_510900_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// The actor's pair of `GsCOORDINATE2`s, 0x50 apart. The frame handler clears
/// both `flg` words before `Gp_UpdateCoord`; the ground-quad helper draws at
/// `field_50`'s x/z and `field_0`'s y.
typedef struct Actor510900Coord {
    /* 0x00 */ GsCOORDINATE2 field_0;
    /* 0x50 */ GsCOORDINATE2 field_50;
} Actor510900Coord;

typedef struct Actor510900Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor510900Coord* field_8;
    /* 0x0C */ s16               field_C; // model flag word, as TmdObject::field_C
} Actor510900Obj2C;

typedef struct Actor510900Work {
    /* 0x000 */ GpObj obj0;
    /* 0x020 */ byte  pad_20[0x18];
    /* 0x038 */ GpObj obj38;
    /* 0x058 */ byte  pad_58[0x264];
    /* 0x2BC */ GpObj obj2BC;
    /* 0x2DC */ byte  pad_2DC[0x18];
    /* 0x2F4 */ GpObj obj2F4;
    /* 0x314 */ byte  pad_314[0x168];
    /* 0x47C */ GpObj obj47C;
    /* 0x49C */ byte  pad_49C[0x48];
    /* 0x4E4 */ GpObj obj4E4;
    /* 0x504 */ GpObj obj504;
    /* 0x524 */ byte  pad_524[0x60];
    /* 0x584 */ s16   field_584;
    /* 0x586 */ byte  pad_586[8];
    /* 0x58E */ s16   field_58E;
    /* 0x590 */ byte  pad_590[2];
    /* 0x592 */ s16   field_592;
    /* 0x594 */ byte  pad_594[0x10];
    /* 0x5A4 */ s16   field_5A4;
    /* 0x5A6 */ byte  pad_5A6[0x16];
    /* 0x5BC */ s16   field_5BC;
} Actor510900Work;

typedef struct Actor510900 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor510900Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[0x8];
    /* 0x2C */ Actor510900Obj2C* field_2C;
} Actor510900;

typedef struct Actor510900Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor510900Ctx;

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B608(Actor510900* arg0);
void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B870(Actor510900* arg0);
s16  func_actor_510900_8013BE84(Actor510900* arg0);
void func_actor_510900_8013C380(Actor510900* arg0);
void func_actor_510900_8013C430(Actor510900* arg0);

#endif
