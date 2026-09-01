#ifndef ACTOR_100400_H
#define ACTOR_100400_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"

/// Object the actor pushes damage and hit flags into (`Actor00400_Fn01B90`).
typedef struct Actor100400Obj {
    /* 0x00 */ byte pad_0[0x10];
    /* 0x10 */ byte field_10[0x30];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[0x13];
} Actor100400Obj;

typedef struct Actor100400Work {
    /* 0x000 */ byte     pad_0[0x39C];
    /* 0x39C */ GpRec18  field_39C[6];
    /* 0x42C */ byte     pad_42C[0x20];
    /* 0x44C */ GpRec18  field_44C[6];
    /* 0x4DC */ byte     pad_4DC[0x70];
    /* 0x54C */ s16      field_54C;
    /* 0x54E */ byte     pad_54E[2];
    /* 0x550 */ s16      field_550;
    /* 0x552 */ byte     pad_552[0x12];
    /* 0x564 */ s16      field_564;
    /* 0x566 */ byte     pad_566[2];
    /* 0x568 */ s16      field_568;
    /* 0x56A */ byte     pad_56A[0x72];
    /* 0x5DC */ GpEffArg field_5DC;
    /* 0x5E4 */ byte     pad_5E4[0x2C];
    /* 0x610 */ s32      field_610;
    /* 0x614 */ byte     pad_614[8];
    /* 0x61C */ s16      field_61C;
    /* 0x61E */ byte     pad_61E[0x1A];
    /* 0x638 */ s16      field_638;
    /* 0x63A */ u16      field_63A;
    /* 0x63C */ byte     pad_63C[4];
    /* 0x640 */ s16      field_640;
    /* 0x642 */ s16      field_642;
    /* 0x644 */ s16      field_644;
    /* 0x646 */ byte     pad_646[0x15];
    /* 0x65B */ u8       field_65B;
    /* 0x65C */ byte     pad_65C[1];
    /* 0x65D */ u8       field_65D;
    /* 0x65E */ byte     pad_65E[6];
    /* 0x664 */ u8       field_664;
} Actor100400Work;

typedef struct Actor100400Ctx {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor100400Ctx;

typedef struct Actor100400 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor100400Work* field_1C;
    /* 0x20 */ Actor100400Obj*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor100400Ctx*  field_2C;
    /* 0x30 */ s32              field_30;
} Actor100400;

#endif
