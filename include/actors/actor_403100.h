#ifndef ACTOR_403100_H
#define ACTOR_403100_H

#include "common.h"

#include "gameplay/D4.h"
#include "gameplay/3A34.h"
#include "gameplay/1BC.h"
#include "main/task.h"

void func_actor_403100_80132064(Task* arg0, SVECTOR* first, SVECTOR* second, s32 arg3);
void func_actor_403100_8013B5E0(Task* arg0, s16 arg1);
void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3);
void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_actor_403100_8013D06C(void);

typedef struct Actor403100QuadEntry {
    /* 0x00 */ u16 tpage;
    /* 0x02 */ u16 clut;
    /* 0x04 */ u16 w;
    /* 0x06 */ u16 h;
    /* 0x08 */ u16 x;
    /* 0x0A */ u16 y;
    /* 0x0C */ u16 depth;
    /* 0x0E */ u8  u;
    /* 0x0F */ u8  v;
    /* 0x10 */ u32 field_10;
} Actor403100QuadEntry;
STATIC_ASSERT_SIZEOF(Actor403100QuadEntry, 0x14);

typedef struct Actor403100RectEntry {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
    /* 0x08 */ s32 value;
} Actor403100RectEntry;
STATIC_ASSERT_SIZEOF(Actor403100RectEntry, 0xC);

typedef union Actor403100Flags {
    u32 word;
    u16 half;
    struct {
        u16 low;
        s16 high;
    } h;
} Actor403100Flags;
STATIC_ASSERT_SIZEOF(Actor403100Flags, 0x4);

typedef struct Actor403100Light {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } field_4;
} Actor403100Light;
STATIC_ASSERT_SIZEOF(Actor403100Light, 0x64);

/// Rotation matrix with aligned word stores for identity initialization.
typedef union Actor403100Matrix {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor403100Matrix;
STATIC_ASSERT_SIZEOF(Actor403100Matrix, 0x20);

extern GpPairSrcE D_actor_403100_8014762C;

typedef struct Actor403100Entry {
    /* 0x00 */ s16   active;
    /* 0x02 */ u8    pad_2[0x6E];
    /* 0x70 */ GpObj obj;
    /* 0x90 */ u8    pad_90[0x60];
} Actor403100Entry;
STATIC_ASSERT_SIZEOF(Actor403100Entry, 0xF0);

extern GpAreaApplyRec   D_8018F2CC;
extern s16              D_actor_403100_80155810;
extern Actor403100Entry D_actor_403100_80155814[28];

/// Eight animation-set pointers passed to slot 3 in message 0x3FF.
/// Entry 4 is replaced by entry 7 of the selected player animation block.
typedef struct Actor403100AnimTable {
    /* 0x00 */ GpAnimSet* sets[8];
} Actor403100AnimTable;
STATIC_ASSERT_SIZEOF(Actor403100AnimTable, 0x20);

/// Position and Euler rotation payload sent to slot 3 as message 0x3E9.
typedef struct Actor403100MsgPos {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor403100MsgPos;
STATIC_ASSERT_SIZEOF(Actor403100MsgPos, 0x18);

/// The word at `Actor403100Work::field_664`, which the overlay reads both as a
/// whole word and as four separate bytes: `func_actor_403100_8013D2A0` gates a
/// new request on `word & 0xFFFF00` (the two bytes at 0x665 / 0x666) being
/// clear, then sets those two bytes, while `func_actor_403100_8013CDC0`
/// switches on the byte at 0x666 alone. Both views are modelled explicitly.
typedef union Actor403100Req {
    /* 0x0 */ s32 word;
    struct {
        /* 0x0 */ s8 field_664;
        /* 0x1 */ s8 field_665;
        /* 0x2 */ u8 field_666; // 4-state machine ticked by func_actor_403100_8013CDC0
        /* 0x3 */ s8 field_667;
    } b;
} Actor403100Req;
STATIC_ASSERT_SIZEOF(Actor403100Req, 0x4);

/// Per-actor work block for the `actor_403100` overlay.
///
/// `func_actor_403100_80136610` allocates it with `Mem_Calloc(0x678, 0)` and
/// stores the same pointer twice: into the `Task::idMap` slot (0x1C), which an
/// enemy actor reuses for its own work block, and into the overlay-wide
/// `D_actor_403100_80155808`. It also hands `work + 0x20` and `work` to the
/// `TmdObject` at `Task::extra` (`field_1C` / `field_20`). Most of the overlay
/// reaches it through the global rather than through the task.
typedef struct Actor403100Work {
    /* 0x000 */ union {
        byte pad_0[0x80];
        struct {
            MATRIX color;
            MATRIX light;
            MATRIX coord;
        } matrices;
    } field_0;
    /* 0x080 */ s16     field_80;
    /* 0x082 */ s16     field_82; // facing angle, stepped towards field_B2
    /* 0x084 */ s16     field_84;
    /* 0x086 */ byte    pad_86[0x2];
    /* 0x088 */ SVECTOR savedRotation;
    /* 0x090 */ s16     field_90;
    /* 0x092 */ s16     field_92;
    /* 0x094 */ s16     field_94;
    /* 0x096 */ byte    pad_96[2];
    /* 0x098 */ s16     field_98;
    /* 0x09A */ s16     field_9A;
    /* 0x09C */ s16     field_9C;
    /* 0x09E */ byte    pad_9E[2];
    /* 0x0A0 */ s16     field_A0;
    /* 0x0A2 */ s16     field_A2;
    /* 0x0A4 */ s16     field_A4;
    /* 0x0A6 */ byte    pad_A6[0x2];
    /* 0x0A8 */ s16     field_A8;
    /* 0x0AA */ s16     field_AA;
    /* 0x0AC */ s16     field_AC;
    /* 0x0AE */ byte    pad_AE[0x2];
    /* 0x0B0 */ s16     field_B0;
    /* 0x0B2 */ s16     field_B2; // target angle
    /* 0x0B4 */ s16     field_B4; // angle offset, decayed towards 0
    /* 0x0B6 */ byte    pad_B6[2];
    /* 0x0B8 */ union {
        struct {
            GpAnimCtx  anim;
            GpAnimSlot slots[15];
        } animation;
        struct {
            byte             pad_B8[0x4C];
            Actor403100Flags flags_104;
            byte             pad_108[0x24];
            u16              flags_12C;
            byte             pad_12E[0x2E6];
        } legacy;
    } field_B8;
    /* 0x414 */ GpObj            field_414; // display nodes unlinked on death
    /* 0x434 */ byte             pad_434[0x48];
    /* 0x47C */ GpObj            field_47C;
    /* 0x49C */ byte             pad_49C[0xC0];
    /* 0x55C */ GpObj            field_55C;
    /* 0x57C */ byte             pad_57C[0x18];
    /* 0x594 */ GpObj            field_594;
    /* 0x5B4 */ byte             pad_5B4[0x1C];
    /* 0x5D0 */ s32              field_5D0;
    /* 0x5D4 */ s32              field_5D4;
    /* 0x5D8 */ s16              field_5D8;
    /* 0x5DA */ s16              field_5DA; // animation request kind
    /* 0x5DC */ u16              field_5DC;
    /* 0x5DE */ s16              field_5DE; // animation id
    /* 0x5E0 */ u16              field_5E0;
    /* 0x5E2 */ s16              field_5E2;
    /* 0x5E4 */ byte             pad_5E4[0x2];
    /* 0x5E6 */ s16              field_5E6;
    /* 0x5E8 */ s16              field_5E8;
    /* 0x5EA */ s16              field_5EA;
    /* 0x5EC */ u16              field_5EC; // per-state frame counter
    /* 0x5EE */ s16              field_5EE;
    /* 0x5F0 */ s16              field_5F0;
    /* 0x5F2 */ s16              field_5F2;
    /* 0x5F4 */ s16              field_5F4;
    /* 0x5F6 */ s16              field_5F6;
    /* 0x5F8 */ u16              field_5F8; // state index
    /* 0x5FA */ u16              field_5FA; // sub-state index
    /* 0x5FC */ s16              field_5FC;
    /* 0x5FE */ s16              field_5FE;
    /* 0x600 */ s16              field_600;
    /* 0x602 */ s16              field_602;
    /* 0x604 */ s16              field_604;
    /* 0x606 */ s16              field_606;
    /* 0x608 */ s16              field_608;
    /* 0x60A */ s16              field_60A;
    /* 0x60C */ s16              field_60C;
    /* 0x60E */ s16              field_60E;
    /* 0x610 */ s16              field_610;
    /* 0x612 */ s16              field_612[3];
    /* 0x618 */ s16              field_618;
    /* 0x61A */ byte             pad_61A[0x2];
    /* 0x61C */ s16              field_61C;
    /* 0x61E */ s16              field_61E;
    /* 0x620 */ s16              field_620;
    /* 0x622 */ s16              field_622;
    /* 0x624 */ byte             pad_624[2];
    /* 0x626 */ s16              field_626;
    /* 0x628 */ s16              field_628;
    /* 0x62A */ s16              field_62A;
    /* 0x62C */ s16              field_62C;
    /* 0x62E */ s16              field_62E;
    /* 0x630 */ s16              field_630;
    /* 0x632 */ s16              field_632;
    /* 0x634 */ Actor403100Flags flags_634;
    /* 0x638 */ s16              field_638;
    /* 0x63A */ s16              field_63A;
    /* 0x63C */ union {
        s16 regionFlags[9];
        struct {
            s16  field_63C;
            s16  field_63E;
            byte pad_640[0xE];
        } fields;
    } regions;
    /* 0x64E */ byte           pad_64E[6];
    /* 0x654 */ s16            field_654;
    /* 0x656 */ u16            field_656;
    /* 0x658 */ s16            field_658;
    /* 0x65A */ s16            field_65A;
    /* 0x65C */ u8             field_65C;
    /* 0x65D */ s8             field_65D;
    /* 0x65E */ byte           pad_65E[0x1];
    /* 0x65F */ s8             field_65F;
    /* 0x660 */ byte           pad_660[0x4];
    /* 0x664 */ Actor403100Req field_664;
    /* 0x668 */ union {
        u16 flags;
        struct {
            s8 field_668;
            s8 field_669;
        } b;
    } field_668;
    /* 0x66A */ byte pad_66A[0x5];
    /* 0x66F */ s8   field_66F;
    /* 0x670 */ byte pad_670[0x8];
} Actor403100Work;
STATIC_ASSERT_SIZEOF(Actor403100Work, 0x678);

void func_actor_403100_8013480C(Task* arg0, s32 arg1);

typedef struct {
    void (*funcs[11])(Task*);
} TaskFuncTable11;
STATIC_ASSERT_SIZEOF(TaskFuncTable11, 0x2C);

void       func_actor_403100_80133C94(void);
void       func_actor_403100_80133D88(Task* arg0);
void       func_actor_403100_80133E88(Task* arg0);
void       func_actor_403100_8013E5FC(void);
void       func_actor_403100_8013E624(Task* arg0);
void       func_8004BFF8(s16 angle, MATRIX* matrix);
extern s32 D_80070F70;

extern TaskFuncTable3  D_actor_403100_80131EB0;
extern TaskFuncTable3  D_actor_403100_80131EBC;
extern TaskFuncTable4  D_actor_403100_80131EC8;
extern TaskFuncTable3  D_actor_403100_80131ED8;
extern TaskFuncTable3  D_actor_403100_80131EE4;
extern TaskFuncTable4  D_actor_403100_80131EF0;
extern TaskFuncTable4  D_actor_403100_80131F00;
extern TaskFuncTable3  D_actor_403100_80131F10;
extern TaskFuncTable3  D_actor_403100_80131F60;
extern TaskFuncTable6  D_actor_403100_80131F84;
extern TaskFuncTable5  D_actor_403100_80131F9C;
extern TaskFuncTable9  D_actor_403100_80131FB0;
extern TaskFuncTable11 D_actor_403100_80131FD4;
void                   func_actor_403100_80132C3C(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4);
extern TaskFuncTable5  D_actor_403100_80132000;
extern TaskFuncTable4  D_actor_403100_80132014;
extern TaskFuncTable3  D_actor_403100_80132024;

/* This routine ignores its incoming arguments; callers use both forms. */
void func_actor_403100_801327CC();
void func_actor_403100_801328DC(Task* arg0);
void func_actor_403100_8013B128(Task* arg0);
void func_actor_403100_8013B3C4(Task* arg0);
void func_actor_403100_8013D11C(Task* arg0);
void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_403100_8013D1B8(s16 arg0, s16 arg1);
void func_actor_403100_8013D24C(void);
s32  func_actor_403100_8013D2F4(GsCOORDINATE2* coord, MATRIX* matrix);
s32  func_actor_403100_8013E33C(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2);
void func_actor_403100_8013D770(Task* arg0);
void func_actor_403100_8013E02C(s16 arg0, s16 arg1, s16 arg2);
void func_actor_403100_8013F610(void);
void func_actor_403100_8013F658(void);
void func_actor_403100_8013B5E0(Task* arg0, s16 arg1);

extern GpImgRec  D_actor_403100_801555EC;
extern GpU16Pair D_actor_403100_80147614;
void             func_actor_403100_801342B4(Task* arg0);
void             func_actor_403100_8013C7B4(Task* arg0);

extern u8 D_actor_403100_801557A8[];

void func_8018257C(void);
void func_8017E250(s32 arg0, s32 arg1);

extern TaskDesc D_actor_403100_8015560C;
extern s16      D_actor_403100_80155794[5][2];

void func_actor_403100_8013E964(void);
void func_actor_403100_8013E96C(Task* arg0);
void func_actor_403100_8013E9D8(Task* arg0);
void func_actor_403100_8013EA60(Task* arg0);
void func_actor_403100_8013EAD4(Task* arg0);
void func_actor_403100_8013EB68(Task* arg0);
void func_actor_403100_8013EBC8(Task* arg0);
void func_actor_403100_8013EC4C(Task* arg0);
void func_actor_403100_8013ECD0(Task* arg0);
void func_actor_403100_8013ED48(void);

/* Per-frame actor dispatcher tables and region query. */
typedef struct {
    void (*funcs[4])(void);
} Actor403100VoidTable4;
extern Actor403100VoidTable4 D_actor_403100_80131E24;
extern TaskFuncTable11       D_actor_403100_80131F34;
extern Actor403100RectEntry  D_actor_403100_80155638[];
extern s32                   D_80166098;
s32                          func_actor_403100_8013D9C4(s16 x, s16 z, Actor403100RectEntry* regions);

extern u8 D_actor_403100_801557B0[2][16];

#endif // ACTOR_403100_H
