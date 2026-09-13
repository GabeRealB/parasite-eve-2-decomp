#ifndef ACTOR_100300_H
#define ACTOR_100300_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/* Byte access view; preserves struct-store ordering in GCC 2.8.1. */
typedef struct Actor00300ByteView {
    s8 value;
} Actor00300ByteView;

typedef struct Actor100300ScratchStack {
    u32 sp;
} Actor100300ScratchStack;

typedef struct Actor00300Light {
    /* 0x00 */ s32 mode;
    /* 0x04 */ union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } data;
} Actor00300Light;
STATIC_ASSERT_SIZEOF(Actor00300Light, 0x64);

extern Actor00300Light D_80114FF8;
extern s32             D_80070F70;

typedef struct Actor00300GroundScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy0;
    /* 0x24 */ DVECTOR sxy1;
    /* 0x28 */ DVECTOR sxy2;
    /* 0x2C */ DVECTOR sxy3;
} Actor00300GroundScratch;
STATIC_ASSERT_SIZEOF(Actor00300GroundScratch, 0x30);

extern MATRIX Gfx_ViewWorldMtx;

typedef struct Actor00300InitScratch {
    /* 0x00 */ SVECTOR offset;
    /* 0x08 */ VECTOR  result;
} Actor00300InitScratch;
STATIC_ASSERT_SIZEOF(Actor00300InitScratch, 0x18);

typedef struct Actor00300InitWork {
    /* 0x00 */ GpObj        obj0;
    /* 0x20 */ GpRec18      rec20;
    /* 0x38 */ GpObj        obj38;
    /* 0x58 */ GpActorD4Rec pose;
    /* 0x70 */ GpRec18      rec70;
    /* 0x88 */ s16          timer;
    /* 0x8A */ s16          pad8A;
} Actor00300InitWork;
STATIC_ASSERT_SIZEOF(Actor00300InitWork, 0x8C);

typedef struct Actor00300MainWork {
    /* 0x000 */ GpAnimCtx      anim;
    /* 0x014 */ GpAnimSlot     field_14[19];
    /* 0x30C */ u8             field_30C[0x130];
    /* 0x43C */ Task*          field_43C;
    /* 0x440 */ u8             field_440[32];
    /* 0x460 */ u8             field_460[32];
    /* 0x480 */ GpObj          obj480;
    /* 0x4A0 */ GpActorD4Rec   pose4A0;
    /* 0x4B8 */ GpRec18        rec4B8;
    /* 0x4D0 */ GpObj          obj4D0;
    /* 0x4F0 */ GpRec18        rec4F0[3];
    /* 0x538 */ GpObj          obj538;
    /* 0x558 */ GpRec18        rec558[4];
    /* 0x5B8 */ GpObj          obj5B8;
    /* 0x5D8 */ GpRec18        rec5D8;
    /* 0x5F0 */ GsCOORDINATE2* field_5F0;
    /* 0x5F4 */ s16            field_5F4;
    /* 0x5F6 */ s16            field_5F6;
    /* 0x5F8 */ u8             pad_5F8[0x10];
    /* 0x608 */ MATRIX         matrix608;
    /* 0x628 */ u8             pad_628[0x20];
    /* 0x648 */ s32            field_648;
    /* 0x64C */ u8             pad_64C[0x1A];
    /* 0x666 */ s16            field_666;
    /* 0x668 */ u8             pad_668[0x20];
    /* 0x688 */ s16            field_688;
    /* 0x68A */ u8             pad_68A[0x2];
    /* 0x68C */ u16            field_68C;
    /* 0x68E */ u8             pad_68E[0xA];
    /* 0x698 */ s16            field_698;
    /* 0x69A */ u8             pad_69A[0xA];
} Actor00300MainWork;
STATIC_ASSERT_SIZEOF(Actor00300MainWork, 0x6A4);
typedef struct Actor00300SpawnArgs {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor00300SpawnArgs;
STATIC_ASSERT_SIZEOF(Actor00300SpawnArgs, 4);
typedef struct Actor00300AreaConfig {
    /* 0x0 */ s16 id;
    /* 0x2 */ s16 area;
    /* 0x4 */ s16 room;
    /* 0x6 */ u16 value;
} Actor00300AreaConfig;
STATIC_ASSERT_SIZEOF(Actor00300AreaConfig, 8);

typedef struct Actor100300RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor100300RotScratch;
STATIC_ASSERT_SIZEOF(Actor100300RotScratch, 0x18);

typedef struct Actor100300Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
    /* 0x0E */ byte           pad_E[0xE];
    /* 0x1C */ void*          field_1C;
    /* 0x20 */ void*          field_20;
} Actor100300Obj2C;

typedef struct Actor100300Work {
    /* 0x000 */ GpObj               obj0;
    /* 0x020 */ u16                 field_20;
    /* 0x022 */ byte                pad_22[0x16];
    /* 0x038 */ GpObj               obj38;
    /* 0x058 */ byte                pad_58[0x18];
    /* 0x070 */ GpRec18             field_70;
    /* 0x088 */ u16                 field_88;
    /* 0x08A */ s16                 field_8A;
    /* 0x08C */ byte                pad_8C[0x3B0];
    /* 0x43C */ struct Actor100300* field_43C;
    /* 0x440 */ byte                field_440[0x20];
    /* 0x460 */ byte                field_460[0x20];
    /* 0x480 */ GpObj               obj480;
    /* 0x4A0 */ byte                pad_4A0[0x30];
    /* 0x4D0 */ GpObj               obj4D0;
    /* 0x4F0 */ byte                pad_4F0[0x48];
    /* 0x538 */ GpObj               obj538;
    /* 0x558 */ byte                pad_558[0x60];
    /* 0x5B8 */ GpObj               obj5B8;
    /* 0x5D8 */ byte                pad_5D8[0x20];
    /* 0x5F8 */ s32                 field_5F8;
    /* 0x5FC */ s32                 field_5FC;
    /* 0x600 */ s32                 field_600;
    /* 0x604 */ byte                pad_604[4];
    /* 0x608 */ MATRIX              field_608;
    /* 0x628 */ MATRIX              field_628;
    /* 0x648 */ s32                 field_648;
    /* 0x64C */ byte                pad_64C[0x8];
    /* 0x654 */ struct _GpEffWork*  field_654;
    /* 0x658 */ s32                 field_658;
    /* 0x65C */ SVECTOR             field_65C;
    /* 0x664 */ s16                 field_664;
    /* 0x666 */ u16                 field_666;
    /* 0x668 */ byte                pad_668[0x2];
    /* 0x66A */ s16                 field_66A;
    /* 0x66C */ byte                pad_66C[0x2];
    /* 0x66E */ s16                 field_66E;
    /* 0x670 */ s16                 field_670;
    /* 0x672 */ u16                 field_672;
    /* 0x674 */ s16                 field_674;
    /* 0x676 */ s16                 field_676;
    /* 0x678 */ u16                 field_678;
    /* 0x67A */ s16                 field_67A;
    /* 0x67C */ s16                 field_67C;
    /* 0x67E */ s16                 field_67E;
    /* 0x680 */ u16                 field_680;
    /* 0x682 */ s16                 field_682;
    /* 0x684 */ s16                 field_684;
    /* 0x686 */ s16                 field_686;
    /* 0x688 */ s16                 field_688;
    /* 0x68A */ s16                 field_68A;
    /* 0x68C */ s16                 field_68C;
    /* 0x68E */ s16                 field_68E;
    /* 0x690 */ s16                 field_690;
    /* 0x692 */ s16                 field_692;
    /* 0x694 */ s16                 field_694;
    /* 0x696 */ u16                 field_696;
    /* 0x698 */ s16                 field_698;
    /* 0x69A */ s16                 field_69A;
    /* 0x69C */ s16                 field_69C;
    /* 0x69E */ s16                 field_69E;
    /* 0x6A0 */ s16                 field_6A0;
    /* 0x6A2 */ s16                 field_6A2;
} Actor100300Work;

typedef struct Actor100300 {
    /* 0x00 */ byte                pad_0[8];
    /* 0x08 */ struct Actor100300* field_8;
    /* 0x0C */ byte                pad_C[0x10];
    /* 0x1C */ Actor100300Work*    field_1C;
    /* 0x20 */ GpEnemy*            field_20;
    /* 0x24 */ byte                pad_24[0x8];
    /* 0x2C */ Actor100300Obj2C*   field_2C;
    /* 0x30 */ s32                 field_30;
} Actor100300;

typedef struct Actor100300Ctx {
    /* 0x00 */ byte  pad_0[0x10];
    /* 0x10 */ void* next;
    /* 0x14 */ u8    field_14;
    /* 0x15 */ byte  pad_15[0x37];
    /* 0x4C */ u8    field_4C;
    /* 0x4D */ byte  pad_4D[7];
    /* 0x54 */ void* field_54;
} Actor100300Ctx;

typedef struct Actor100300AnimArgs {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[2];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
} Actor100300AnimArgs;

typedef struct Actor100300DestroyArgs {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u16  field_2;
} Actor100300DestroyArgs;

typedef struct Actor100300StateFuncTable3 {
    void (*funcs[3])(Actor100300Ctx*, Actor100300*);
} Actor100300StateFuncTable3;

extern Actor100300StateFuncTable3 Actor00300_D00004;
extern Actor100300StateFuncTable3 Actor00300_D0003C;
extern Actor100300StateFuncTable3 Actor00300_D00048;
extern s16                        Actor00300_D16394[];
extern u16                        Actor00300_D15FEC;
extern s16                        Actor00300_D16000[];
extern s16                        Actor00300_D15FF8[];

void Actor00300_Fn048D4(Actor100300Ctx* arg0, Actor100300* arg1);
void Actor00300_Fn04958(Actor100300Ctx* arg0, Actor100300* arg1);

#endif
