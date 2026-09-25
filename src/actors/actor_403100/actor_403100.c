#include "common.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "actors/actor.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/session.h"
#include "main/gameflag.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

void func_actor_403100_80132064(Task* arg0, SVECTOR* first, SVECTOR* second, s32 arg3);
void func_actor_403100_8013B5E0(Task* arg0, s16 arg1);
void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3);
void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_actor_403100_8013D06C(void);
s32  func_actor_403100_8013D460(GpCoord* coord, SVECTOR* pos);

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

/// One entry of a region table: a rectangle on the floor from (`x`, `y`) -
/// `y` being the Z coordinate - spanning `w` along X and `h` along Z, and the
/// value a lookup returns for a point inside it. A table ends at an entry
/// whose `value` is -1.
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

extern GpPairSrcE D_actor_403100_8014762C;

typedef struct Actor403100Entry {
    /* 0x00 */ s16     active;
    /* 0x02 */ s16     age;
    /* 0x04 */ s16     frame;
    /* 0x06 */ SVECTOR delta;
    /* 0x0E */ SVECTOR position;
    /* 0x16 */ u8      pad_16[0xA];
    /* 0x20 */ GpCoord coord;
    /* 0x70 */ GpObj   obj;
    /* 0x90 */ GpRec18 records[4];
} Actor403100Entry;
STATIC_ASSERT_SIZEOF(Actor403100Entry, 0xF0);

extern GpAreaApplyRec   D_8018F2CC;
extern s16              D_actor_403100_80155810;
extern Actor403100Entry D_actor_403100_80155814[28];
extern GpCoord          D_actor_403100_80155834;
extern GpRec18          D_actor_403100_801558A4[];

void func_8017F6C8(s32 screen, s32 depth, s32 size, s32 frame);

/// Eight animation-set pointers passed to slot 3 in message 0x3FF.
/// Entry 4 is replaced by entry 7 of the selected player animation block.
typedef struct Actor403100AnimTable {
    /* 0x00 */ GpAnimSet* sets[8];
} Actor403100AnimTable;
STATIC_ASSERT_SIZEOF(Actor403100AnimTable, 0x20);

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
/// `func_actor_403100_80136610` allocates it with `memCalloc(0x678, 0)` and
/// stores the same pointer twice: into the `Task::work` slot (0x1C), which an
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
    /* 0x414 */ GpObj field_414; // display nodes unlinked on death
    /* 0x434 */ byte  pad_434[0x48];
    /* 0x47C */ GpObj field_47C;
    /// Contact records the actor's own body collects, under `field_414`:
    /// `func_actor_403100_8013335C` walks all eight for kind-2 hits and
    /// `Gp_ClearRec18Occupied` empties the table each frame.
    /* 0x49C */ GpRec18 pad_49C[8];
    /* 0x55C */ GpObj   field_55C;
    /// Contact records from the first attached collider (`field_55C`).
    /* 0x57C */ GpRec18 pad_57C[1];
    /* 0x594 */ GpObj   field_594;
    /// Contact records from the second attached collider (`field_594`).
    /* 0x5B4 */ GpRec18          pad_5B4[1];
    /* 0x5CC */ byte             pad_5CC[4];
    /* 0x5D0 */ s32              field_5D0;
    /* 0x5D4 */ s32              field_5D4;
    /* 0x5D8 */ s16              field_5D8;
    /* 0x5DA */ s16              field_5DA; // animation request kind
    /* 0x5DC */ u16              field_5DC;
    /* 0x5DE */ s16              field_5DE; // animation id
    /* 0x5E0 */ u16              field_5E0;
    /* 0x5E2 */ s16              field_5E2;
    /* 0x5E4 */ s16              field_5E4; // frames until the next hit is taken
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

void func_actor_403100_80133C94(void);
void func_actor_403100_80133D88(Task* arg0);
void func_actor_403100_80133E88(Task* arg0);
void func_actor_403100_8013E5FC(void);
void func_actor_403100_8013E624(Task* arg0);
void func_8004BFF8(s16 angle, MATRIX* matrix);

void func_actor_403100_80132C3C(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4);

/* This routine ignores its incoming arguments; callers use both forms. */
void func_actor_403100_801327CC();
void func_actor_403100_801328DC(Task* arg0);
void func_actor_403100_8013B128(Task* arg0);
void func_actor_403100_8013B3C4(Task* arg0);
void func_actor_403100_8013D11C(Task* arg0);
void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_403100_8013D1B8(s16 arg0, s16 arg1);
void func_actor_403100_8013D24C(void);
s32  func_actor_403100_8013D2F4(GpCoord* coord, MATRIX* matrix);
s32  func_actor_403100_8013E33C(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2);
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
extern Actor403100RectEntry D_actor_403100_80155638[];
extern s32                  D_80166098;
s32                         func_actor_403100_8013D9C4(s16 x, s16 z, Actor403100RectEntry* regions);

extern u8 D_actor_403100_801557B0[2][16];

extern Actor403100RectEntry D_actor_403100_80155698[];

static __inline__ s32 Actor403100_FindRegion(s16 x, s16 z)
{
    Actor403100RectEntry* region;
    for (region = D_actor_403100_80155698; region->value != -1; region++) {
        if (x >= region->x && x <= region->x + region->w &&
            z >= region->y && z <= region->y + region->h) {
            return region->value;
        }
    }
    return 0;
}

static __inline__ s32 Actor403100_FindEffectRegion(s16 x, s16 z)
{
    Actor403100RectEntry* region;
    for (region = D_actor_403100_80155638; region->value != -1; region++) {
        if (x >= region->x && x <= region->x + region->w && z >= region->y && z <= region->y + region->h)
            return region->value;
    }
    return 0;
}

/* Inline forms of this actor's rotation traversal helpers. */
static __inline__ s32 Actor403100_AccumulateRotation(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2)
{
    MATRIX   matrix;
    GpCoord* coord;

    coord = arg0->sub;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->sub;
    }
}

static __inline__ s32 Actor403100_LocalizeRotation(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2)
{
    MATRIX   matrix;
    MATRIX   normal;
    MATRIX   transposed;
    GpCoord* coord;

    coord = arg0->sub;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    matrix = coord->coord;
    while (1) {
        coord = coord->sub;
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            break;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    __asm__ volatile(
        "lhu $12, 0(%0);"
        "lhu $13, 6(%0);"
        "lhu $14, 12(%0);"
        "sh $12, 0(%1);"
        "sh $13, 2(%1);"
        "sh $14, 4(%1);"
        "lhu $12, 2(%0);"
        "lhu $13, 8(%0);"
        "lhu $14, 14(%0);"
        "sh $12, 6(%1);"
        "sh $13, 8(%1);"
        "sh $14, 10(%1);"
        "lhu $12, 4(%0);"
        "lhu $13, 10(%0);"
        "lhu $14, 16(%0);"
        "sh $12, 12(%1);"
        "sh $13, 14(%1);"
        "sh $14, 16(%1);"
        : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `field_8` is
/// this actor's `GpCoord`.
extern Actor403100Work* D_actor_403100_80155808;
extern GpEnemy*         D_actor_403100_8015580C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void                        func_8017E3C8(void);
void                        func_8017E4B8(void);
extern u16                  D_actor_403100_80147630;
extern GpCoord*             D_actor_403100_80155630;
extern u32                  D_actor_403100_801556EC[];
extern u32                  D_actor_403100_8015572C[];
extern Actor403100QuadEntry D_actor_403100_801557E0[2];

extern s8                   D_8007218A;
extern Actor403100AnimTable D_actor_403100_8015570C;
extern GpAnimBlk*           Gp_PlayerAnimBlkTbl[];
extern u16                  Gp_WeaponIdBase[];

extern u8 D_80165FC0;

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8017E128(s32 arg0);
void func_80182730(void);

extern MATRIX Gfx_ViewWorldMtx;

/* Defined later in this file. */
void func_actor_403100_801326DC(Actor403100Work* work);
void func_actor_403100_8013712C(Task* arg0);
void func_actor_403100_8013C008(s16 arg0, s16 arg1);
void func_actor_403100_8013D74C(Task* arg0);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);
void func_actor_403100_8013F12C(void);
void func_actor_403100_8013E16C(void);
void func_actor_403100_8013E174(void);
void func_actor_403100_8013E1E4(void);
void func_actor_403100_8013E2BC(void);
void func_actor_403100_8013BA64(Task* arg0);
void func_actor_403100_8013C214(Task* arg0);
void func_actor_403100_8013CBE0(Task* arg0);
void func_actor_403100_8013CDC0(void);
void func_actor_403100_8013D2A0(s16 arg0);
void func_actor_403100_8013D6B4(Task* arg0);
void func_actor_403100_8013D700(Task* arg0);
void func_actor_403100_80136830(Task* arg0);
void func_actor_403100_80137268(void);
void func_actor_403100_80137310(void);
void func_actor_403100_8013BB8C(Task* arg0);
void func_actor_403100_8013BDE4(Task* arg0);
void func_actor_403100_8013BEF0(Task* arg0);
void func_actor_403100_8013D88C(Task* arg0);
void func_actor_403100_8013D8F4(Task* arg0);
void func_actor_403100_8013DA6C(void);
void func_actor_403100_8013DAC4(Task* arg0);
void func_actor_403100_8013DB48(Task* arg0);
void func_actor_403100_8013DC18(Task* arg0);
void func_actor_403100_8013DCAC(Task* arg0);
void func_actor_403100_8013DD78(Task* arg0);
void func_actor_403100_8013DE0C(Task* arg0);
void func_actor_403100_8013DEA0(Task* arg0);
void func_actor_403100_8013DF0C(void);
void func_actor_403100_8013DF64(void);
void func_actor_403100_8013DFBC(Task* arg0);
void func_actor_403100_8013E6A0(Task* arg0);
void func_actor_403100_8013E784(Task* arg0);
void func_actor_403100_8013E7C8(Task* arg0);
void func_actor_403100_8013E88C(Task* arg0);
void func_actor_403100_8013E920(Task* arg0);
void func_actor_403100_8013ED50(Task* arg0);
void func_actor_403100_8013EDDC(void);
void func_actor_403100_8013EE28(Task* arg0);
void func_actor_403100_8013EEB0(void);
void func_actor_403100_8013EEB8(Task* arg0);
void func_actor_403100_8013EF24(void);
void func_actor_403100_8013EF2C(void);
void func_actor_403100_8013EF34(void);
void func_actor_403100_8013EF58(void);
void func_actor_403100_8013EF60(void);
void func_actor_403100_8013EFC0(void);
void func_actor_403100_8013EFC8(Task* arg0);
void func_actor_403100_8013F034(Task* arg0);
void func_actor_403100_8013F0A8(Task* arg0);
void func_actor_403100_8013F18C(void);
void func_actor_403100_8013F1D8(void);
void func_actor_403100_8013F230(void);
void func_actor_403100_8013F270(void);
void func_actor_403100_8013F2D8(void);
void func_actor_403100_8013F344(void);
void func_actor_403100_8013F3AC(void);
void func_actor_403100_8013F3EC(Task* arg0);
void func_actor_403100_8013F488(void);
void func_actor_403100_8013F4E0(void);
void func_actor_403100_8013F520(void);
void func_actor_403100_8013F588(void);
void func_actor_403100_8013F6B0(void);
void func_actor_403100_8013F6F4(void);
void func_actor_403100_8013F76C(void);
void func_actor_403100_8013F7AC(void);
void func_actor_403100_8013F7B4(void);
void func_actor_403100_8013F7BC(void);

static __inline__ s16 Actor403100_TestFlags(void)
{
    if (D_actor_403100_80155808->flags_634.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->flags_634.word & 0x102) {
        return 1;
    }
    return 0;
}

static __inline__ s16 Actor403100_TestFlags104(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.word & 0x102) {
        return 1;
    }
    return 0;
}

static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_12C & 0x100) {
        return 1;
    }
    return 0;
}

/// Per-frame hooks run before the behaviour mode, indexed by `field_5F2`.
const Actor403100VoidTable4 D_actor_403100_80131E24 = {
    {
        func_actor_403100_8013E16C,
        func_actor_403100_8013E174,
        func_actor_403100_8013E1E4,
        func_actor_403100_8013E2BC,
    },
};

void func_actor_403100_80132064(Task* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    SVECTOR     end;
    OverlayMat  matrix;
    u16         mode;
    OverlayMat* identity;
    GpCoord*    joint;
    s32         i;
    u32         random;
    GpRec18*    records;
    GpObj*      obj;
    GpCoord*    coord;

    i      = 0;
    mode   = arg3;
    joint  = &arg0->extra.tmd->coords[3];
    end.vx = arg1->vx + arg2->vx;
    end.vy = arg1->vy + arg2->vy;
    end.vz = arg1->vz + arg2->vz;
    for (; i < 0x1C; i++) {
        if (D_actor_403100_80155814[i].active == 0) {
            coord                             = &D_actor_403100_80155814[i].coord;
            D_actor_403100_80155814[i].active = 1;
            D_actor_403100_80155814[i].age    = 0;
            random                            = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState                       = random;
            D_actor_403100_80155814[i].frame  = (s16)((random >> 0x10) & 0xF);
            func_actor_403100_8013D460(joint, arg1);
            identity = &matrix;
            func_actor_403100_8013D460(joint, &end);
            D_actor_403100_80155814[i].delta.vx     = (s16)(end.vx - arg1->vx);
            D_actor_403100_80155814[i].delta.vy     = (s16)(end.vy - arg1->vy);
            D_actor_403100_80155814[i].delta.vz     = (s16)(end.vz - arg1->vz);
            D_actor_403100_80155814[i].position.vx  = (u16)arg1->vx;
            D_actor_403100_80155814[i].position.vy  = (u16)arg1->vy;
            D_actor_403100_80155814[i].position.vz  = (u16)arg1->vz;
            D_actor_403100_80155814[i].coord.sub    = &gGfxViewCoord;
            matrix.ident.m00_m01                    = 0x1000;
            matrix.ident.m02_m10                    = 0;
            identity->ident.m11_m12                 = 0x1000;
            matrix.ident.m20_m21                    = 0;
            identity->ident.m22                     = 0x1000;
            matrix.mat.t[0]                         = (s32)(s16)arg1->vx;
            matrix.mat.t[1]                         = (s32)(s16)arg1->vy;
            records                                 = D_actor_403100_80155814[i].records;
            matrix.mat.t[2]                         = (s32)(s16)arg1->vz;
            D_actor_403100_80155814[i].coord.coord  = matrix.mat;
            D_actor_403100_80155814[i].obj.coord    = coord;
            D_actor_403100_80155814[i].obj.ctx.recs = records;
            D_actor_403100_80155814[i].obj.pos.vx   = 0;
            D_actor_403100_80155814[i].obj.pos.vy   = 0;
            D_actor_403100_80155814[i].obj.pos.vz   = 0;
            D_actor_403100_80155814[i].obj.key      = Gp_PackPair(&D_actor_403100_80147614, 1);
            D_actor_403100_80155814[i].obj.radius   = 0x32;
            D_actor_403100_80155814[i].obj.flags    = 1;
            obj                                     = &D_actor_403100_80155814[i].obj;
            Gp_LinkObj(3, obj);
            Gp_InitRec18Table(records, 4, 0);
            if ((mode << 0x10) == 0) {
                obj->flags |= 0xC000;
            } else {
                obj->flags &= 0x3FFF;
            }
            Gp_UpdateCoord(&D_actor_403100_80155814[i].coord);
            D_actor_403100_80155814[i].coord.flg        = 0;
            D_actor_403100_80155814[i].coord.coord.t[0] = (s32)(s16)D_actor_403100_80155814[i].position.vx;
            D_actor_403100_80155814[i].coord.coord.t[1] = (s32)(s16)D_actor_403100_80155814[i].position.vy;
            D_actor_403100_80155814[i].coord.coord.t[2] = (s32)(s16)D_actor_403100_80155814[i].position.vz;
            break;
        }
    }
}

void func_actor_403100_80132320(Task* arg0)
{
    D_actor_403100_80155808->field_47C.coord    = &arg0->extra.tmd->coords[3];
    D_actor_403100_80155808->field_47C.ctx.recs = (GpRec18*)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155808->field_47C.pos.vz   = 0x300;
    D_actor_403100_80155808->field_47C.pos.vx   = 0;
    D_actor_403100_80155808->field_47C.pos.vy   = 0;
    D_actor_403100_80155808->field_47C.key      = 0x3001F;
    D_actor_403100_80155808->field_47C.radius   = 0x400;
    D_actor_403100_80155808->field_47C.flags    = 1;
    Gp_LinkObj(2, &D_actor_403100_80155808->field_47C);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_49C, 8, 0);
    D_actor_403100_80155808->field_47C.flags   |= 0x8000;
    D_actor_403100_80155808->field_414.coord    = &arg0->extra.tmd->coords[1];
    D_actor_403100_80155808->field_414.ctx.recs = (GpRec18*)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155808->field_414.pos.vx   = 0;
    D_actor_403100_80155808->field_414.pos.vy   = 0;
    D_actor_403100_80155808->field_414.pos.vz   = 0;
    D_actor_403100_80155808->field_414.key      = 0x3001F;
    D_actor_403100_80155808->field_414.radius   = 0x800;
    D_actor_403100_80155808->field_414.flags    = 1;
    Gp_LinkObj(2, &D_actor_403100_80155808->field_414);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_434, 3, 0);
    D_actor_403100_80155808->field_55C.key      = 0x3001F;
    D_actor_403100_80155808->field_414.flags   |= 0x8000;
    D_actor_403100_80155808->field_55C.coord    = &arg0->extra.tmd->coords[7];
    D_actor_403100_80155808->field_55C.ctx.recs = (GpRec18*)D_actor_403100_80155808->pad_57C;
    D_actor_403100_80155808->field_55C.pos.vx   = -0x200;
    D_actor_403100_80155808->field_55C.pos.vy   = 0;
    D_actor_403100_80155808->field_55C.pos.vz   = 0x200;
    D_actor_403100_80155808->field_55C.radius   = 0x3A0;
    D_actor_403100_80155808->field_55C.flags    = 1;
    Gp_LinkObj(3, &D_actor_403100_80155808->field_55C);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_57C, 1, 0);
    D_actor_403100_80155808->field_594.key      = 0x3001F;
    D_actor_403100_80155808->field_55C.flags   &= 0x7FFF;
    D_actor_403100_80155808->field_594.coord    = &arg0->extra.tmd->coords[6];
    D_actor_403100_80155808->field_594.ctx.recs = (GpRec18*)D_actor_403100_80155808->pad_5B4;
    D_actor_403100_80155808->field_594.pos.vx   = -0x200;
    D_actor_403100_80155808->field_594.pos.vy   = 0;
    D_actor_403100_80155808->field_594.pos.vz   = 0x180;
    D_actor_403100_80155808->field_594.radius   = 0x3A0;
    D_actor_403100_80155808->field_594.flags    = 1;
    Gp_LinkObj(3, &D_actor_403100_80155808->field_594);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_5B4, 1, 0);
    D_actor_403100_80155808->field_594.flags &= 0x7FFF;
}
void func_actor_403100_80132528(Task* arg0)
{
    SVECTOR    pos;
    SVECTOR    rotation;
    MATRIX     matrix;
    GameActor* player;
    GpCoord*   joint;
    GpCoord*   playerCoord;
    s32        angle;
    s32        anim;
    u16        savedAngle;
    GpCoord*   coords;

    anim                               = D_actor_403100_80155808->field_5DE;
    playerCoord                        = (*Gp_ActorSlots)->extra.tmd->coords;
    coords                             = arg0->extra.tmd->coords;
    player                             = (*Gp_ActorSlots)->work;
    angle                              = D_actor_403100_80155808->field_5E2;
    savedAngle                         = (u16)D_actor_403100_80155808->field_5E2;
    D_actor_403100_80155808->field_5E2 = angle * 2;
    func_actor_403100_8013E02C(anim, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_801328DC(arg0);
    func_actor_403100_8013D770(arg0);
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    joint         = &coords[7];
    coords[7].flg = 0;
    Gp_UpdateCoord(joint);
    pos.vx = -0x290;
    pos.vy = 0x1E8;
    pos.vz = 0x220;
    func_actor_403100_8013D460(joint, &pos);
    func_actor_403100_8013D2F4(joint, &matrix);
    Gp_MtxToEuler(&matrix, &rotation);
    player->field_50 = rotation.vx;
    player->field_52 = rotation.vy;
    player->field_54 = rotation.vz;
    RotMatrix(&rotation, &playerCoord->coord);
    playerCoord->coord.t[0] = pos.vx;
    playerCoord->coord.t[1] = pos.vy;
    playerCoord->coord.t[2] = pos.vz;
    playerCoord->flg        = 0;
    Gp_UpdateCoord(playerCoord);
    D_actor_403100_80155808->field_5E2 = (-angle) << 1;
    func_actor_403100_8013E02C(D_actor_403100_80155808->field_5DE, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5E2 = (s16)savedAngle;
    func_actor_403100_8013E02C(D_actor_403100_80155808->field_5DE, angle, 0);
}
void func_actor_403100_801326DC(Actor403100Work* work)
{
    s32 i;

    if ((s16)D_actor_403100_80155808->field_5DC == D_actor_403100_80155808->field_5DE) {
        for (i = 1; i < 15; i++) {
            D_actor_403100_80155808->field_B8.animation.slots[i].rate = (u8)D_actor_403100_80155808->field_5E2;
        }
    } else {
        for (i = 1; i < 15; i++) {
            D_actor_403100_80155808->field_B8.animation.slots[i].rate = (u8)D_actor_403100_80155808->field_5E2;
            func_800B4114(&D_actor_403100_80155808->field_B8.animation.anim, i, D_actor_403100_80155808->field_5DE, 0, D_actor_403100_80155808->field_5FC);
        }
        D_actor_403100_80155808->field_5FC = 0;
    }
    D_actor_403100_80155808->field_5DC = D_actor_403100_80155808->field_5DE;
}
void func_actor_403100_801327CC()
{
    s32 i;
    if (D_actor_403100_80155808->field_5DA == 1) {
        func_actor_403100_801326DC(D_actor_403100_80155808);
        D_actor_403100_80155808->field_5DA = 3;
        D_actor_403100_80155808->field_5E0 = 0;
    } else if (D_actor_403100_80155808->field_5DA == 2) {
        for (i = 1; i < 15; i++) {
            Gp_AnimResetSlot(&D_actor_403100_80155808->field_B8.animation.anim, i, D_actor_403100_80155808->field_5DE);
            D_actor_403100_80155808->field_B8.animation.slots[i].rate = (u8)D_actor_403100_80155808->field_5E2;
        }
        D_actor_403100_80155808->field_5DA = 3;
        D_actor_403100_80155808->field_5E0 = 0;
        D_actor_403100_80155808->field_5DC = D_actor_403100_80155808->field_5DE;
    } else if (D_actor_403100_80155808->field_5DA == 3) {
        D_actor_403100_80155808->field_5E0 += 1;
    }
    for (i = 1; i < 15; i++) {
        Gp_AnimTickIndex(&D_actor_403100_80155808->field_B8.animation.anim, i);
    }
}
void func_actor_403100_801328DC(Task* arg0)
{
    GpCoord* root;
    GpCoord* joint;
    MATRIX*  rotation;
    MATRIX*  dest;

    root                                = arg0->extra.tmd->coords;
    *(MATRIX**)PSX_SCRATCH_ADDR(0x3FC) -= 1;
    rotation                            = *(MATRIX**)PSX_SCRATCH_ADDR(0x3FC);
    joint                               = &root[5];
    Actor403100_AccumulateRotation(joint, rotation, root);
    RotMatrixX(D_actor_403100_80155808->field_604, rotation);
    func_8004BFF8(D_actor_403100_80155808->field_608, rotation);
    Actor403100_LocalizeRotation(joint, rotation, root);
    dest          = &joint->coord;
    dest->m[0][0] = rotation->m[0][0];
    dest->m[0][1] = rotation->m[0][1];
    dest->m[0][2] = rotation->m[0][2];
    dest->m[1][0] = rotation->m[1][0];
    dest->m[1][1] = rotation->m[1][1];
    dest->m[1][2] = rotation->m[1][2];
    dest->m[2][0] = rotation->m[2][0];
    dest->m[2][1] = rotation->m[2][1];
    dest->m[2][2] = rotation->m[2][2];
    joint->flg    = 0;
    Gp_UpdateCoord(joint);
    *(MATRIX**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}
void func_actor_403100_80132C3C(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height)
{
    MATRIX    firstMatrix;
    MATRIX    secondMatrix;
    SVECTOR   first;
    SVECTOR   second;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    s32       screen0;
    s32       screen1;
    s32       screen2;
    s32       screen3;
    s32       perspective;
    s32       flags;
    s16       lastZ;
    s16       angle;
    GpCoord*  secondCoord;
    GpCoord*  firstCoord;
    s32       offset0;
    s32       offset1;
    s32       offset2;
    s32       offset3;
    s32       halfX;
    s32       halfZ;
    s32       depth;
    GpCoord*  coords;
    POLY_FT4* poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)height;
        second.vy  = (s16)height;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        angle      = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX      = (first.vx - second.vx) / 2;
        halfZ      = (first.vz - second.vz) / 2;
        offset0    = rcos(angle) * width;
        corner0.vy = (s16)height;
        corner0.vx = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1    = rcos(angle) * width;
        corner1.vy = (s16)height;
        corner1.vx = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2    = rcos(angle) * width;
        corner2.vy = (s16)height;
        corner2.vx = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3    = rcos(angle) * width;
        corner3.vy = (s16)height;
        corner3.vx = (second.vx + (offset3 >> 0xC)) - halfX;
        lastZ      = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        corner3.vz = lastZ;
        if ((corner0.vz < 0x23F0) && (corner1.vz < 0x23F0) && (corner2.vz < 0x23F0) && (lastZ < 0x23F0)) {
            if (corner0.vx >= -0x144F) {
                corner0.vx = -0x1450;
            }
            if (corner1.vx >= -0x144F) {
                corner1.vx = -0x1450;
            }
            if (corner2.vx >= -0x144F) {
                corner2.vx = -0x1450;
            }
            if (corner3.vx >= -0x144F) {
                corner3.vx = -0x1450;
            }
        } else if ((corner0.vx >= -0x144F) && (corner1.vx >= -0x144F) && (corner2.vx >= -0x144F) && (corner3.vx >= -0x144F)) {
            if (corner0.vz < 0x23F0) {
                corner0.vz = 0x23F0;
            }
            if (corner1.vz < 0x23F0) {
                corner1.vz = 0x23F0;
            }
            if (corner2.vz < 0x23F0) {
                corner2.vz = 0x23F0;
            }
            if (corner3.vz < 0x23F0) {
                corner3.vz = 0x23F0;
            }
        }
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3, &perspective, &flags);
        if (flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(poly + 1);
            setlen(poly, 9);
            poly->code            = 0x2E;
            PRIM_XY_WORD(poly, 0) = screen0;
            poly->tpage           = 0x48;
            PRIM_XY_WORD(poly, 1) = screen1;
            poly->clut            = 0x4283;
            PRIM_XY_WORD(poly, 2) = screen2;
            PRIM_XY_WORD(poly, 3) = screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            setRGB0(poly, 0xFF, 0xFF, 0xFF);
            addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
    }
}
void func_actor_403100_801331D4(Task* arg0)
{
    SVECTOR  pos;
    GpCoord* joint;
    GpCoord* playerCoord;
    s16      dx;
    s16      dx2;
    s16      dz;
    s16      dz2;
    GpCoord* coords;

    coords = arg0->extra.tmd->coords;
    joint  = &coords[3];
    if (*Gp_ActorSlots != NULL) {
        playerCoord                        = (*Gp_ActorSlots)->extra.tmd->coords;
        D_actor_403100_80155808->field_90  = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->field_92  = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->field_94  = (u16)playerCoord->coord.t[2];
        D_actor_403100_80155808->field_98  = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->field_9A  = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->field_9C  = (u16)playerCoord->coord.t[2];
        dx                                 = (u16)playerCoord->coord.t[0] - (u16)coords->coord.t[0];
        pos.vx                             = dx;
        pos.vy                             = (u16)playerCoord->coord.t[1] - (u16)coords->coord.t[1];
        dz                                 = (u16)playerCoord->coord.t[2] - (u16)coords->coord.t[2];
        pos.vz                             = dz;
        D_actor_403100_80155808->field_62E = SquareRoot0((dx * dx) + (dz * dz));
        func_actor_403100_8013D460(joint, &pos);
        dx2                                = (u16)playerCoord->coord.t[0] - (u16)pos.vx;
        pos.vx                             = dx2;
        pos.vy                             = (u16)playerCoord->coord.t[1] - pos.vy;
        dz2                                = (u16)playerCoord->coord.t[2] - (u16)pos.vz;
        pos.vz                             = dz2;
        D_actor_403100_80155808->field_630 = SquareRoot0((dx2 * dx2) + (dz2 * dz2));
    }
}
void func_actor_403100_8013335C(Task* arg0)
{
    s16 damage;
    s16 scaledDamage;
    s32 hitId;
    s16 effectKind;
    s32 i;
    u16 hp;
    u32 tickDamage;
    u32 kind;
    s32 expired;

    effectKind                          = 0;
    D_actor_403100_80155808->pad_66A[2] = 0;
    i                                   = 0;
    for (; i < 8; i++) {
        hitId = D_actor_403100_80155808->pad_49C[i].key;
        if ((hitId & 0xFFFF0000) == 0x20000) {
            if (D_actor_403100_80155808->field_5E4 == 0) {
                D_actor_403100_80155808->pad_66A[2] = 1;
                damage                              = Gp_ComputeDamage(D_actor_403100_80155808->pad_49C[i].key, D_actor_403100_80155808->field_630 / 2, 0, 0);
                scaledDamage                        = damage;
                D_actor_403100_80155808->field_5E4  = Gp_GetIdParam2(D_actor_403100_80155808->pad_49C[i].key);
                if (Gp_RollEnemyChance(D_actor_403100_8015580C, D_actor_403100_80155808->pad_49C[i].key, 0) != 0) {
                    scaledDamage = damage * 4;
                    effectKind   = 1;
                }
                if ((u8)D_actor_403100_80155808->pad_670[3] != 0) {
                    effectKind   = 2;
                    scaledDamage = scaledDamage * 2;
                }
                kind = effectKind;
                if (kind == 1)
                    goto effect1;
                if (kind == 2)
                    goto effect2;
                goto effect_end;
            effect1:
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[4], 0, 0);
                goto effect_end;
            effect2:
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[4], 3, 0);
            effect_end:
                func_800E2C78(D_actor_403100_8015580C, D_actor_403100_80155808->pad_49C[i].key, scaledDamage, 0);
                func_800DA6E8(&D_actor_403100_8015580C->node, scaledDamage, 0);
                do {
                    hp                          = (u16)D_actor_403100_8015580C->hp - scaledDamage;
                    D_actor_403100_8015580C->hp = hp;
                    if ((s16)hp < 0) {
                        D_actor_403100_8015580C->hp = 0U;
                    }
                    func_800FDB18(Gp_GetIdParam1(D_actor_403100_80155808->pad_49C[i].key) & 0xFFFF, &arg0->extra.tmd->coords[4], 0, &D_actor_403100_80155630);
                    D_actor_403100_80155808->field_62A = 1;
                    kind                               = Gp_GetIdParam0(D_actor_403100_80155808->pad_49C[i].key) & 0xFFFF;
                } while (0);
                switch (kind) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(D_actor_403100_8015580C);
                        break;
                    case 2:
                        Gp_SetObjFlag2(D_actor_403100_8015580C, D_actor_403100_80155808->pad_49C[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(D_actor_403100_8015580C, D_actor_403100_80155808->pad_49C[i].key, 0);
                        break;
                    case 4:
                        D_actor_403100_80155808->field_62A = 1;
                        break;
                    case 5:
                        D_actor_403100_80155808->field_62A = 1;
                        break;
                    case 6:
                        D_actor_403100_80155808->field_62A = 1;
                        break;
                    case 7:
                        D_actor_403100_80155808->field_62A = 1;
                        break;
                    case 8:
                        D_actor_403100_80155808->field_62A  = 0;
                        D_actor_403100_80155808->pad_66A[2] = 0;
                        break;
                    case 9:
                        D_actor_403100_80155808->field_62A = 2;
                        break;
                }
                if (D_actor_403100_80155808->field_62A != 0) {
                    D_actor_403100_80155808->field_60C = 0x10;
                    Gp_SetLightMode(arg0->spawnArg2, 1);
                }
            } else if ((Gp_GetIdParam1(hitId) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[4], 0, &D_actor_403100_80155630);
            }
        }
        if (D_actor_403100_80155808->pad_66A[2] != 0)
            break;
    }
    if (D_actor_403100_8015580C->reactionFlags & 1) {
        D_actor_403100_8015580C->reactionFlags &= ~1;
        D_actor_403100_80155808->field_62A      = 2;
    }
    if (D_actor_403100_8015580C->reactionFlags & 2) {
        D_actor_403100_8015580C->reactionFlags &= ~2;
        D_actor_403100_80155808->field_60C      = 0x5A;
        D_actor_403100_80155808->field_62A      = 3;
    }
    if (D_actor_403100_8015580C->reactionFlags & 0xC) {
        tickDamage = (u32)Gp_TickObjFlag4(D_actor_403100_8015580C) >> 2;
        if ((s16)tickDamage != 0) {
            D_actor_403100_8015580C->hp = (u16)((u16)D_actor_403100_8015580C->hp - tickDamage);
            ((void (*)(void*, s32, s32, u32))func_800DA6E8)(&D_actor_403100_8015580C->node, (s16)tickDamage, 0, tickDamage);
            if ((s16)D_actor_403100_8015580C->hp < 0) {
                D_actor_403100_8015580C->hp = 0U;
            }
            D_actor_403100_80155808->pad_66A[2] = 1;
            D_actor_403100_80155808->field_62A  = 2;
        }
        expired = Gp_ObjFlag4Expired(D_actor_403100_8015580C);
        if (expired != 0) {
            D_actor_403100_8015580C->reactionFlags = (u8)(D_actor_403100_8015580C->reactionFlags & 0xF3);
        }
    }
    if (Gp_FindRec18(D_actor_403100_80155808->field_55C.ctx.recs, 0) != 0) {
        for (i = 0; i < 1; i++) {
            if ((D_actor_403100_80155808->pad_57C[i].key & 0xFFFF0000) == 0x10000) {
                D_actor_403100_80155808->field_668.b.field_668 = 1;
            }
        }
    }
    if (Gp_FindRec18(D_actor_403100_80155808->field_594.ctx.recs, 0) != 0) {
        for (i = 0; i < 1; i++) {
            if ((D_actor_403100_80155808->pad_5B4[i].key & 0xFFFF0000) == 0x10000) {
                D_actor_403100_80155808->field_668.b.field_669 = 1;
            }
        }
    }
    Gp_ClearRec18Occupied(D_actor_403100_80155808->pad_57C);
    Gp_ClearRec18Occupied(D_actor_403100_80155808->pad_5B4);
    Gp_ClearRec18Occupied(D_actor_403100_80155808->pad_49C);
    if (D_actor_403100_80155808->field_5E4 > 0) {
        D_actor_403100_80155808->field_5E4 = (s16)((u16)D_actor_403100_80155808->field_5E4 - 1);
        return;
    }
    D_actor_403100_80155808->field_5E4 = 0;
}

s32 func_actor_403100_80133928(void)
{
    s16 state;
    s8  mode;

    mode = D_actor_403100_80155808->pad_66A[2];
    if (mode == 1) {
        state = D_actor_403100_80155808->field_62A;
        if (state == mode) {
            D_actor_403100_80155808->field_62A = 0;
            func_actor_403100_8013D24C();
            return 0;
        }
        if (state == 2) {
            SndEvt_EnqueueType7(0x401F0004, 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->field_62A = 0;
            D_actor_403100_80155808->field_5F8 = 8;
            D_actor_403100_80155808->field_5FA = 0;
            return 1;
        }
        if (state == 3) {
            SndEvt_EnqueueType7(0x401F0004, 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->field_62A = 0;
            D_actor_403100_80155808->field_5F8 = 0xA;
            D_actor_403100_80155808->field_5FA = 0;
            return 1;
        }
        D_actor_403100_80155808->field_62A = 0;
        return 0;
    }
    return 0;
}
void func_actor_403100_801339EC(Task* arg0)
{
    void (*handlers[5])(Task*) = {
        (void (*)(Task*))func_actor_403100_80133C94,
        func_actor_403100_80133D88,
        func_actor_403100_80133E88,
        (void (*)(Task*))func_actor_403100_8013E5FC,
        func_actor_403100_8013E624
    };
    OverlayMat       rotation;
    VECTOR           scale;
    OverlayMat       scaling;
    GpCoord*         coords;
    GpCoord*         center;
    GpCoord*         coords2;
    MATRIX*          mtx;
    MATRIX*          mtx2;
    GpCoord*         side;
    GpCoord*         scaled;
    Actor403100Work* work;
    s32              flash;
    u8*              head;
    u8*              head2;
    MATRIX*          dest;
    VECTOR*          pos;
    s32              brightness;

    handlers[(s16)D_actor_403100_80155808->field_5F8](arg0);
    func_actor_403100_801327CC(arg0);
    SOFT_USE_REG(arg0);
    coords                            = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_82 = (s32)((u16)D_actor_403100_80155808->field_82 << 20) >> 20;
    mtx                               = &rotation.mat;
    rotation.ident.m00_m01            = 0x1000;
    rotation.ident.m02_m10            = 0;
    *(s32*)&mtx->m[1][1]              = 0x1000;
    rotation.ident.m20_m21            = 0;
    mtx->m[2][2]                      = 0x1000;
    func_8004BFF8(D_actor_403100_80155808->field_82, &rotation.mat);
    dest                  = &coords->coord;
    dest->m[0][0]         = rotation.mat.m[0][0];
    dest->m[0][1]         = rotation.mat.m[0][1];
    dest->m[0][2]         = rotation.mat.m[0][2];
    dest->m[1][0]         = rotation.mat.m[1][0];
    dest->m[1][1]         = rotation.mat.m[1][1];
    dest->m[1][2]         = rotation.mat.m[1][2];
    dest->m[2][0]         = rotation.mat.m[2][0];
    dest->m[2][1]         = rotation.mat.m[2][1];
    dest->m[2][2]         = rotation.mat.m[2][2];
    coords->flg           = 0;
    scaled                = arg0->extra.tmd->coords;
    scale.vx              = D_actor_403100_80155808->field_5D8;
    scale.vy              = scale.vx;
    scale.vz              = scale.vx;
    mtx2                  = &scaling.mat;
    scaling.ident.m00_m01 = 0x1000;
    scaling.ident.m02_m10 = 0;
    *(s32*)&mtx2->m[1][1] = 0x1000;
    scaling.ident.m20_m21 = 0;
    mtx2->m[2][2]         = 0x1000;
    ScaleMatrix(&scaling.mat, &scale);
    MulMatrix(&scaled->coord, &scaling.mat);
    func_actor_403100_801328DC(arg0);
    coords2        = arg0->extra.tmd->coords;
    coords2[8].flg = 0;
    coords2[7].flg = 0;
    coords2[6].flg = 0;
    coords2[5].flg = 0;
    coords2[4].flg = 0;
    coords2[3].flg = 0;
    coords2[2].flg = 0;
    coords2[1].flg = 0;
    coords2[0].flg = 0;
    side           = &coords2[4];
    center         = &coords2[3];

    Gp_UpdateCoord(&coords2[8]);
    USE_REG(center);
    Gp_UpdateCoord(side);
    __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
    head                               = *(u8**)(head + 0x3FC);
    pos                                = (VECTOR*)(head - 16);
    pos->vx                            = center->workm.t[0];
    pos->vy                            = center->workm.t[1];
    pos->vz                            = center->workm.t[2];
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = pos;
    Gp_UpdateActorColor(arg0->spawnArg2, pos, 0, 0);
    work  = D_actor_403100_80155808;
    flash = work->field_5FE;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(work));
    head2 = *(u8**)(head2 + 0x3FC);
    SOFT_TOUCH_REG_USE(head2, flash);
    head2 += 16;
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2) : "memory");
    if (flash != 0) {
        if (flash >= 16) {
            brightness = rsin(gDisplayState.animFrame << 9) << 13;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 12;
        }
        Display_ClampField126((s8)(brightness >> 24));
        D_actor_403100_80155808->field_5FE = (u16)D_actor_403100_80155808->field_5FE - 1;
    } else {
        Display_ClampField126(0);
    }
}
void func_actor_403100_80133C94(void)
{
    s32 i;

    SndEvt_EnqueueType7(0x401F0004, 0xA);
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    D_actor_403100_80155810 = 0;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5FC  = 0x20;
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 0x11;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5D8  = 0x1400;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_604  = 0;
    D_actor_403100_80155808->field_608  = 0;
    D_actor_403100_80155808->field_5F8 += 1;
}
void func_actor_403100_80133D88(Task* arg0)
{
    GpCoord* coord;
    u16      frame;

    coord                              = arg0->extra.tmd->coords;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0xA0) {
        coord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x82) {
        func_actor_403100_801345E0(arg0, arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x12C) {
        D_actor_403100_80155808->field_5D8 = 0x1600;
        coord->coord.t[0]                  = -0xA28;
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 0xC;
        D_actor_403100_80155808->field_5DA = 2;
        coord->coord.t[1]                  = 0x1390;
        coord->coord.t[2]                  = 0x1130;
        D_actor_403100_80155808->field_82  = -0x6B0;
        D_actor_403100_80155808->field_604 = -0x140;
        D_actor_403100_80155808->field_608 = -0x100;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5F8 = D_actor_403100_80155808->field_5F8 + 1;
    }
}

void func_actor_403100_80133E88(Task* arg0)
{
    SVECTOR  pos;
    Task*    task;
    GpCoord* coord;
    s32      x;
    u16      frame;
    GpCoord* rootCoord;

    rootCoord                          = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_604 = (u16)(D_actor_403100_80155808->field_604 - 2);
    rootCoord->flg                     = 0;
    rootCoord->coord.t[1]              = (s32)(rootCoord->coord.t[1] + 0xC);
    if ((D_actor_403100_80155808->field_5EC & 0x7F) == 0x28) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[0][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[0][1];
        }
    }
    if ((D_actor_403100_80155808->field_5EC & 0x3F) == 0x20) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[1][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[1][1];
        }
    }
    if ((D_actor_403100_80155808->field_5EC & 0x7F) == 8) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[2][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[2][1];
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 2) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[1][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[1][1];
            pos.vx            = 0x578;
            pos.vy            = -0xFA0;
            pos.vz            = -0xAF0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x600A5, coord, 3, &pos);
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x1E) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[1][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[1][1];
            pos.vx            = 0x3E8;
            pos.vy            = -0xFA0;
            pos.vz            = -0x1130;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x600A5, coord, 4, &pos);
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x3C) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[1][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[1][1];
            pos.vx            = 0;
            pos.vy            = -0xFA0;
            pos.vz            = -0xFA0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x600A5, coord, 5, &pos);
        }
    }
    if ((D_actor_403100_80155808->field_5EC & 0x7F) == 0x40) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = (s32)D_actor_403100_80155794[4][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = (s32)D_actor_403100_80155794[4][1];
        }
    }
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xBE) {
        GameFlag_SetNibble(0x73, 1);
        D_actor_403100_80155808->field_5F8 = (u16)(D_actor_403100_80155808->field_5F8 + 1);
    }
}
void func_actor_403100_801342B4(Task* arg0)
{
    SVECTOR  pos1, pos2, offset1, offset2;
    GpCoord* coords;
    GpCoord* coord1;
    GpCoord* coord2;
    s32      i;

    coords            = arg0->extra.tmd->coords;
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    coord1        = &coords[8];
    coords[8].flg = 0;
    Gp_UpdateCoord(coord1);
    pos1.vx = offset1.vx = 0x160;
    pos1.vy = offset1.vy = 0x148;
    i                    = 3;
    pos1.vz = offset1.vz = 0x2C0;
    func_actor_403100_8013D460(coord1, &pos1);
    coord2  = &coords[7];
    pos2.vx = offset2.vx = 0;
    pos2.vy = offset2.vy = 0;
    pos2.vz = offset2.vz = 0;
    func_actor_403100_8013D460(coord2, &pos2);
    for (; i < 9; i++) {
        if (Actor403100_FindRegion(pos1.vx, pos1.vz) == i) {
            if (D_actor_403100_80155808->regions.regionFlags[i] == 0) {
                Gp_SpawnEff(0x60050, coord1, 0, &offset1);
                func_8017E250((s16)i, 1);
                D_actor_403100_80155808->regions.regionFlags[i] = 1;
            } else {
                Gp_SpawnEff(0x60050, coord1, 1, &offset1);
            }
        }
    }
    for (i = 3; i < 9; i++) {
        if (Actor403100_FindRegion(pos2.vx, pos2.vz) == i) {
            if (D_actor_403100_80155808->regions.regionFlags[i] == 0) {
                Gp_SpawnEff(0x60050, coord2, 0, &offset2);
                func_8017E250((s16)i, 1);
                D_actor_403100_80155808->regions.regionFlags[i] = 1;
            } else {
                Gp_SpawnEff(0x60050, coord2, 1, &offset2);
            }
        }
    }
}
void func_actor_403100_801345E0(Task* arg0, Task* arg1)
{
    s32      x;
    Task*    task;
    GpCoord* coord;

    arg0->extra.tmd->coords->flg = 0;
    if (!(D_actor_403100_80155808->field_5EC & 0x3F)) {
        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            USE_REG(coord);
            x                 = D_actor_403100_80155794[0][0];
            coord->coord.t[0] = x;
            USE_REG(x);
            coord->coord.t[1] = 0;
            coord->coord.t[2] = D_actor_403100_80155794[0][1];
        }
        if (!(D_actor_403100_80155808->field_5EC & 0x3F)) {
            task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
            if (task != NULL) {
                coord = task->extra.tmd->coords;
                USE_REG(coord);
                x                 = D_actor_403100_80155794[1][0];
                coord->coord.t[0] = x;
                USE_REG(x);
                coord->coord.t[1] = 0;
                coord->coord.t[2] = D_actor_403100_80155794[1][1];
            }
            if (!(D_actor_403100_80155808->field_5EC & 0x3F)) {
                task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
                if (task != NULL) {
                    coord = task->extra.tmd->coords;
                    USE_REG(coord);
                    x                 = D_actor_403100_80155794[2][0];
                    coord->coord.t[0] = x;
                    USE_REG(x);
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = D_actor_403100_80155794[2][1];
                }
                if (!(D_actor_403100_80155808->field_5EC & 0x3F)) {
                    task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
                    if (task != NULL) {
                        coord = task->extra.tmd->coords;
                        USE_REG(coord);
                        x                 = D_actor_403100_80155794[3][0];
                        coord->coord.t[0] = x;
                        USE_REG(x);
                        coord->coord.t[1] = 0;
                        coord->coord.t[2] = D_actor_403100_80155794[3][1];
                    }
                    if (!(D_actor_403100_80155808->field_5EC & 0x3F)) {
                        task = Task_SpawnFromTable(&D_actor_403100_8015560C, 1, 0, 0);
                        if (task != NULL) {
                            coord = task->extra.tmd->coords;
                            USE_REG(coord);
                            x                 = D_actor_403100_80155794[4][0];
                            coord->coord.t[0] = x;
                            USE_REG(x);
                            coord->coord.t[1] = 0;
                            coord->coord.t[2] = D_actor_403100_80155794[4][1];
                        }
                    }
                }
            }
        }
    }
}

/* Past `entry`, each entry's fields are reached through the scalar byte
   offset `offset` from a fixed base, the form the ROM steps; indexing the
   entry array instead strength-reduces into a walking pointer. */
void func_actor_403100_8013480C(Task* arg0, s32 arg1)
{
    Actor403100Entry* entries;
    SVECTOR           pos;
    GpDeltaScratch    delta;
    s32               screen;
    s32               flag;
    s32               depth;
    Actor403100Entry* entry;
    s16               size;
    s32               collision;
    s32               j;
    s32               offset;
    s32               i;
    s16               growth;
    s32               region;
    s32               count;
    s32               recordSize;
    u32               workHigh;
    Actor403100Work*  work;
    GpObj*            objects;
    Actor403100Entry* current;
    GpObj*            object;
    GpObj*            walker;

    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    __asm__ volatile("lui %0,%%hi(D_actor_403100_80155808)" : "=r"(workHigh));
    __asm__ volatile("lw %0,%%lo(D_actor_403100_80155808)(%1)" : "=r"(work) : "r"(workHigh) : "memory");
    count = work->flags_634.h.high;
    i     = 0;
    if (count > 0) {
        entries = (Actor403100Entry*)D_actor_403100_80155814;
        walker  = &entries[0].obj;
        objects = walker;
        offset  = 0;
        entry   = entries;
    next_entry: {
        if (entry->active != 0) {
            __asm__ volatile("lw %0,%%lo(D_actor_403100_80155808)(%1)" : "=r"(work) : "r"(workHigh) : "memory");
            if (work->regions.regionFlags[2] == 0) {
                region = Actor403100_FindEffectRegion(entry->position.vx, entry->position.vz);
                if (region == 2) {
                    func_8017E250(2, 1);
                    D_actor_403100_80155808->regions.regionFlags[2] = 1;
                }
            }
            pos.vx = entry->position.vx;
            pos.vy = entry->position.vy;
            pos.vz = entry->position.vz;
            gte_ldv0(&pos);
            gte_rtps();
            gte_stsxy(&screen);
            gte_stflg(&flag);
            gte_stszotz(&depth);
            if ((s16)D_actor_403100_80155808->field_5F8 == 5) {
                growth = entry->age * 0xF0;
            } else {
                growth = entry->age * 0x3C;
            }
            growth += 0x90;
            size    = arg1;
            size   += growth;
            TOUCH_REG_USE2(size, arg1, arg1);
            if (flag >= 0) {
                func_8017F6C8(screen, (s32)(depth << 0xC) >> 0x10, (s16)((s32)((s32)(size << 0x10) >> 1) / (s32)(depth * 4)), entry->frame);
            } else {
                walker->flags = (u16)((walker->flags & 0x7FFF) | 0x4000);
            }
            if (Gp_StateF0.field_4 == 0) {
                if (Gp_FindRec18(entry->obj.ctx.recs, 0) != 0) {
                    j = 0;
                    do {
                        if (((((Actor403100Entry*)((j * 0x18 + offset) + (u32)entries))->records[0].key & 0xFFFF0000) == 0x10000) && (D_actor_403100_80155810 == 0)) {
                            D_actor_403100_80155810 = 0xA;
                        }
                        j += 1;
                    } while (j < 4);
                }
                collision = func_800E0C10((GpRec18*)((u8*)D_actor_403100_801558A4 + offset), &delta, 4, 0);
                if (collision == 0) {
                    entry->position.vx = (u16)(entry->position.vx + entry->delta.vx);
                    entry->position.vy = (u16)(entry->position.vy + entry->delta.vy);
                    entry->position.vz = (u16)(entry->position.vz + entry->delta.vz);
                } else if (collision >= 0) {
                    if (collision < 3) {
                        current              = ((Actor403100Entry*)(offset + (u32)entries));
                        current->position.vy = (u16)((current->position.vy - 0x100) - current->delta.vy);
                    }
                }
                Gp_UpdateCoord((GpCoord*)((u8*)&D_actor_403100_80155834 + offset));
                entry->coord.coord.t[0] = (s32)(s16)entry->position.vx;
                entry->coord.flg        = 0;
                entry->coord.coord.t[1] = (s32)(s16)entry->position.vy;
                entry->coord.coord.t[2] = (s32)entry->position.vz;
                entry->obj.radius       = (s16)((s16)size / 3);
                if (D_actor_403100_80155810 == 0) {
                    walker->flags = (u16)(walker->flags | 0x8000);
                } else {
                    object        = (GpObj*)(offset + (u32)objects);
                    object->flags = (u16)(object->flags & 0x7FFF);
                }
                walker->flags = (u16)(walker->flags | 0x4000);
                recordSize    = 0x20;
                Gp_ClearRec18Occupied((GpRec18*)(offset + ((u32)objects + recordSize)));
                entry->age   = (s16)((u16)entry->age + 1);
                entry->frame = (s16)((u16)entry->frame + 1);
                if (entry->age == D_actor_403100_80155808->flags_634.h.high) {
                    entry->active = 0;
                    Gp_UnlinkObj(walker);
                }
            } else {
                walker->flags &= 0x3FFF;
            }
        }
        walker  = (GpObj*)((u8*)walker + sizeof(Actor403100Entry));
        offset += sizeof(Actor403100Entry);
        do {
        } while (0);
    }
        __asm__ volatile("lui %0,%%hi(D_actor_403100_80155808)" : "=r"(workHigh));
        __asm__ volatile("lw %0,%%lo(D_actor_403100_80155808)(%1)" : "=r"(work) : "r"(workHigh) : "memory");
        count = work->flags_634.h.high;
        TOUCH_REG_USE(i, count);
        i += 1;
        entry++;
        if (i < count)
            goto next_entry;
    }
    if (D_actor_403100_80155810 != 0) {
        D_actor_403100_80155810 = (u16)D_actor_403100_80155810 - 1;
    }
}

/// States of the task `func_actor_403100_8013E04C` runs, by `Task::state`.
const TaskFuncTable3 D_actor_403100_80131E70 = {
    {
        func_actor_403100_8013E6A0,
        func_actor_403100_8013E6F0,
        func_actor_403100_8013E784,
    },
};

/// States of the task `func_actor_403100_8013E0A4` runs, by `Task::state`.
const TaskFuncTable3 D_actor_403100_80131E7C = {
    {
        func_actor_403100_8013E7C8,
        func_actor_403100_8013E88C,
        func_actor_403100_8013E920,
    },
};

void func_actor_403100_80134D50(Task* arg0)
{
    TmdObject* object                 = arg0->extra.tmd;
    void       (*handlers[10])(Task*) = {
        (void (*)(Task*))func_actor_403100_8013E964,
        func_actor_403100_8013E96C,
        func_actor_403100_8013E9D8,
        func_actor_403100_8013EA60,
        func_actor_403100_8013EAD4,
        func_actor_403100_8013EB68,
        func_actor_403100_8013EBC8,
        func_actor_403100_8013EC4C,
        func_actor_403100_8013ECD0,
        (void (*)(Task*))func_actor_403100_8013ED48
    };
    OverlayMat       rotation;
    VECTOR           scale;
    OverlayMat       scaling;
    GpCoord*         coords;
    GpCoord*         center;
    GpCoord*         coords2;
    MATRIX*          mtx;
    MATRIX*          mtx2;
    GpCoord*         side;
    GpCoord*         scaled;
    Actor403100Work* work;
    s32              flash;
    u8*              head;
    u8*              head2;
    MATRIX*          dest;
    VECTOR*          pos;
    s32              brightness;
    s32              timer;

    D_actor_403100_80155808 = arg0->work;
    D_actor_403100_8015580C = arg0->spawnArg2;
    handlers[(s16)D_actor_403100_80155808->field_5F8](arg0);
    coords                            = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_82 = (s32)((u16)D_actor_403100_80155808->field_82 << 20) >> 20;
    mtx                               = &rotation.mat;
    rotation.ident.m00_m01            = 0x1000;
    rotation.ident.m02_m10            = 0;
    *(s32*)&mtx->m[1][1]              = 0x1000;
    rotation.ident.m20_m21            = 0;
    mtx->m[2][2]                      = 0x1000;
    func_8004BFF8(D_actor_403100_80155808->field_82, &rotation.mat);
    dest                  = &coords->coord;
    dest->m[0][0]         = rotation.mat.m[0][0];
    dest->m[0][1]         = rotation.mat.m[0][1];
    dest->m[0][2]         = rotation.mat.m[0][2];
    dest->m[1][0]         = rotation.mat.m[1][0];
    dest->m[1][1]         = rotation.mat.m[1][1];
    dest->m[1][2]         = rotation.mat.m[1][2];
    dest->m[2][0]         = rotation.mat.m[2][0];
    dest->m[2][1]         = rotation.mat.m[2][1];
    dest->m[2][2]         = rotation.mat.m[2][2];
    coords->flg           = 0;
    scaled                = arg0->extra.tmd->coords;
    scale.vx              = D_actor_403100_80155808->field_618;
    scale.vy              = scale.vx;
    scale.vz              = scale.vx;
    mtx2                  = &scaling.mat;
    scaling.ident.m00_m01 = 0x1000;
    scaling.ident.m02_m10 = 0;
    *(s32*)&mtx2->m[1][1] = 0x1000;
    scaling.ident.m20_m21 = 0;
    mtx2->m[2][2]         = 0x1000;
    ScaleMatrix(&scaling.mat, &scale);
    MulMatrix(&scaled->coord, &scaling.mat);
    func_actor_403100_8013480C(arg0, 0x96);
    coords2        = arg0->extra.tmd->coords;
    coords2[8].flg = 0;
    coords2[7].flg = 0;
    coords2[6].flg = 0;
    coords2[5].flg = 0;
    coords2[4].flg = 0;
    coords2[3].flg = 0;
    coords2[2].flg = 0;
    coords2[1].flg = 0;
    coords2[0].flg = 0;
    side           = &coords2[4];
    center         = &coords2[3];

    Gp_UpdateCoord(&coords2[8]);
    USE_REG(center);
    Gp_UpdateCoord(side);
    __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
    head                               = *(u8**)(head + 0x3FC);
    pos                                = (VECTOR*)(head - 16);
    pos->vx                            = center->workm.t[0];
    pos->vy                            = center->workm.t[1];
    pos->vz                            = center->workm.t[2];
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = pos;
    Gp_UpdateActorColor(arg0->spawnArg2, pos, 0, 0);
    work  = D_actor_403100_80155808;
    flash = work->field_5FE;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(work));
    head2 = *(u8**)(head2 + 0x3FC);
    SOFT_TOUCH_REG_USE(head2, flash);
    head2 += 16;
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2) : "memory");
    if (flash != 0) {
        if (flash >= 16) {
            brightness = rsin(gDisplayState.animFrame << 9) << 13;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 12;
        }
        Display_ClampField126((s8)(brightness >> 24));
        D_actor_403100_80155808->field_5FE = (u16)D_actor_403100_80155808->field_5FE - 1;
    } else {
        Display_ClampField126(0);
    }
    timer = D_actor_403100_80155808->field_658;
    if (timer >= 0) {
        if (timer == 0) {
            Tmd_FreeBuffers(object);
        }
        D_actor_403100_80155808->field_658 = (u16)D_actor_403100_80155808->field_658 - 1;
    }
}
void func_actor_403100_8013506C(Task* arg0)
{
    GpCoord* coord;
    u16      frame;
    s32      sound;
    s32      pan;
    s32      sound2;
    s32      pan2;

    coord                              = arg0->extra.tmd->coords;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    coord->coord.t[1]                  = (s16)(rcos((s16)frame * 0x20) << 0xD >> 0x10) - 0x300;
    coord->coord.t[0]                 += 0x40;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x40) {
        D_actor_403100_80155808->field_5EC = 0;
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->field_5FE = 0x1E;
        func_8017E128(0);
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_801351F8(Task* arg0)
{
    u16 frame;
    s32 sound;
    s32 pan;
    s32 sound2;
    s32 pan2;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x3E) {
        func_8017E128(0);
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->field_5FE = 0x1E;
        sound                              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan                                = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
        D_actor_403100_80155808->field_5FA += 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x28) {
        D_actor_403100_80155808->field_61C = 3;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x28) {
        D_actor_403100_80155808->field_98 = -0x3E8;
        D_actor_403100_80155808->field_9A = 0;
        D_actor_403100_80155808->field_9C = -0x960;
    }
}
void func_actor_403100_8013539C(Task* arg0)
{
    GpCoord* coord;
    s32      i;

    D_actor_403100_80155810 = 0;
    coord                   = arg0->extra.tmd->coords;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    coord->coord.t[0]                  = -0x74E;
    coord->coord.t[2]                  = -0x1C51;
    coord->coord.t[1]                  = 0;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 2;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_82  = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_618 = 0x1910;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_801354A0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xC) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
        func_8017E128(1);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x18) {
        func_8017E128(0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x28) {
        D_actor_403100_80155808->field_5FA += 1;
    }
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_801355D4(Task* arg0)
{
    GpCoord* coord;
    s32      i;

    D_actor_403100_80155810                   = 0;
    coord                                     = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_61C        = 3;
    D_actor_403100_80155808->flags_634.h.high = 0x1C;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    coord->coord.t[0]                  = -0x74E;
    coord->coord.t[2]                  = -0x1770;
    coord->coord.t[1]                  = 0;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 7;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_82  = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_618 = 0x1400;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5FA += 1;
}

void func_actor_403100_801356F4(Task* arg0)
{
    s16*    frame;
    SVECTOR first;
    SVECTOR second;
    s32     sound;
    s32     pan;

    D_actor_403100_80155808->field_98   = -0xFA0;
    D_actor_403100_80155808->field_9A   = 0;
    D_actor_403100_80155808->field_9C   = 0x7B2;
    D_actor_403100_80155808->field_5EC += 1;
    frame                               = (s16*)&D_actor_403100_80155808->field_5EC;
    COMPILER_BARRIER();
    if (*frame == 1) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x32) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if (D_actor_403100_80155808->field_5EC < 0x33U) {
        first.vy  = -0x1F0;
        first.vz  = 0x620;
        second.vy = -0x20;
        first.vx  = 0;
        second.vx = 0;
        second.vz = 0xF0;
        func_actor_403100_80132064(arg0, &first, &second, 1);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_8013588C(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xE) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0xE) < 7U) {
        func_8017E128(D_actor_403100_801557A8[D_actor_403100_80155808->field_5EE]);
        D_actor_403100_80155808->field_5EE += 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x15) {
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_801359DC(Task* arg0)
{
    GpCoord*         coord;
    GpCoord*         joint;
    s32              i;
    Actor403100Work* work;
    s32              value;

    value                   = 0x10;
    D_actor_403100_80155810 = 0;
    coord                   = arg0->extra.tmd->coords;
    joint                   = &coord[6];
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    joint->coord.t[0] = -0x807;
    coord->coord.t[0] = -0x384;
    work              = D_actor_403100_80155808;
    coord->coord.t[2] = 0x1130;
    coord->coord.t[1] = 0;
    work->field_5E2   = value;
    work->field_5DE   = 0x13;
    work->field_5DA   = 2;
    work->field_5EC   = 0;
    work->field_618   = 0x1400;
    work->field_5FA  += 1;
    Gp_ApplyAreaRecs(&D_8018F2CC);
}
void func_actor_403100_80135AE0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;

    angle                             = (u16)D_actor_403100_80155808->field_82;
    D_actor_403100_80155808->field_82 = angle + ((s16)(-0x4000 - angle * 0x10) >> 9);
    if ((s16)D_actor_403100_80155808->field_5EC == 0xBE) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xF0) {
        D_actor_403100_80155808->field_5FA += 1;
    }
    D_actor_403100_80155808->field_5EC += 1;
}

void func_actor_403100_80135C00(Task* arg0)
{
    s32 sound;
    s32 sound_2;
    s32 sound_3;
    s32 sound_4;
    s32 pan;
    s32 pan_2;
    s32 pan_3;
    s32 pan_4;
    u16 frame;
    s32 depth;
    s32 depth_2;
    s32 depth_3;
    s32 depth_4;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x1E) {
        func_8018257C();
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x22) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x28) {
        func_8018257C();
        sound_2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan_2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth_2 = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound_2, (s32)pan_2, (s8)(depth_2 / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x30) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x3C) {
        func_8018257C();
        sound_3 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan_3   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth_3 = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound_3, (s32)pan_3, (s8)(depth_3 / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x3F) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x5A) {
        func_8018257C();
        sound_4 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan_4   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth_4 = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound_4, (s32)pan_4, (s8)(depth_4 / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x5E) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 0x15;
        D_actor_403100_80155808->field_5DA  = 2;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80135F30(Task* arg0)
{
    s32        sound;
    s32        sound2;
    s32        pan;
    s32        pan2;
    u16        frame;
    s32        depth;
    s32        depth2;
    TmdObject* obj;

    obj                                = arg0->extra.tmd;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xA) {
        func_8018257C();
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xE) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x26) {
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth2 = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->field_5FC  = 0x18;
        D_actor_403100_80155808->field_5E2  = 0x18;
        D_actor_403100_80155808->field_5DE  = 0x16;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_600  = 0;
        D_actor_403100_80155808->field_5FA += 1;
        obj->otOffset                       = 0;
    }
}
void func_actor_403100_80136100(Task* arg0)
{
    s16      angle;
    s32      sound;
    s32      y;
    s32      z;
    s32      pan;
    u16      frame;
    s32      depth;
    GpCoord* coord;

    coord                               = arg0->extra.tmd->coords;
    angle                               = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
    D_actor_403100_80155808->field_5EC += 1;
    D_actor_403100_80155808->field_600  = angle;
    y                                   = -((s32)(rsin((s32)angle) << 0xD) >> 0x10);
    coord->coord.t[1]                   = y;
    if (y == 0) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if ((s16)D_actor_403100_80155808->field_5EC < 0x11) {
        D_actor_403100_80155808->field_82 = (u16)D_actor_403100_80155808->field_82 - 0x34;
        coord->coord.t[2]                 = (s32)(coord->coord.t[2] - 0x10);
        coord->coord.t[0]                 = (s32)(coord->coord.t[0] - 8);
    }
    frame = D_actor_403100_80155808->field_5EC;
    if ((u32)(frame - 0x11) < 0xAU) {
        D_actor_403100_80155808->field_82 = (u16)D_actor_403100_80155808->field_82 - 0x10;
        z                                 = coord->coord.t[2] - 0x14;
    } else if ((u16)(frame - 0x1B) < 0xAU) {
        D_actor_403100_80155808->field_82 = (u16)D_actor_403100_80155808->field_82 - 0x10;
        z                                 = coord->coord.t[2] - 0x20;
    } else {
        if ((u16)(frame - 0x25) < 0xAU) {
            D_actor_403100_80155808->field_82 -= 0x10;
        } else if ((u16)(frame - 0x2F) < 0xAU) {
            D_actor_403100_80155808->field_82 -= 0xC;
        } else if ((u16)(frame - 0x39) < 0xAU) {
            D_actor_403100_80155808->field_82 -= 8;
        }
        z = coord->coord.t[2] - 0x40;
    }
    coord->coord.t[2] = z;
}
void func_actor_403100_8013631C(Task* arg0)
{
    s16      frame;
    s16      step;
    s32      sound;
    s32      pan;
    s32      depth;
    GpCoord* coord;

    frame                               = D_actor_403100_80155808->field_5EC + 1;
    coord                               = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_5EC  = (u16)frame;
    D_actor_403100_80155808->field_5EE += 1;
    D_actor_403100_80155808->field_600  = (u16)((D_actor_403100_80155808->field_600 + 0x20) & 0x7FF);
    if (D_actor_403100_80155808->field_5EE < 0xA) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->field_5EE < 0x14) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->field_5EE < 0x1E) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->field_5EE < 0x28) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->field_5EE < 0x32) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->field_5EE < 0x3C) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->field_5EE < 0x46) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->field_5EE < 0x5D) {
        coord->coord.t[2] -= 0x4;
    } else if (D_actor_403100_80155808->field_5EE < 0x69) {
        coord->coord.t[2] -= 0x8;
    } else if (D_actor_403100_80155808->field_5EE < 0x75) {
        coord->coord.t[2] -= 0x10;
    } else if (D_actor_403100_80155808->field_5EE < 0x81) {
        coord->coord.t[2] -= 0x28;
    } else if (D_actor_403100_80155808->field_5EE < 0x92) {
        coord->coord.t[2] -= 0x18;
    } else if (D_actor_403100_80155808->field_5EE < 0x9C) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->field_5EE < 0xA6) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->field_5EE < 0xB0) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->field_5EE < 0xBA) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->field_5EE < 0xC4) {
        coord->coord.t[2] -= 0x6;
    }
    step = (s16)D_actor_403100_80155808->field_5EE;
    if ((step == 0x2D) || (step == 0xE6) || (step == 0xB4) || (step == 0x57)) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 0x18;
        D_actor_403100_80155808->field_5DA = 2;
        D_actor_403100_80155808->field_5EE = 0U;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x118) {
        D_actor_403100_80155808->field_5FA = (u16)(D_actor_403100_80155808->field_5FA + 1);
    }
}
void func_actor_403100_80136610(Task* arg0)
{
    Actor403100Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    s32              i;
    u16*             flags;
    s32              kind;
    GpCoord*         coord;

    obj   = arg0->extra.tmd;
    coord = obj->coords;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & ~0xFF) != GP_LOC_KEY(3, 29, 2, 0) ||
        (arg0->work = memCalloc(0x678U, false)) == NULL) {
        Gp_DestroyEnemy(D_actor_403100_8015580C, arg0);
        return;
    }
    enemy                               = ((volatile Task*)arg0)->spawnArg2;
    work                                = ((volatile Task*)arg0)->work;
    obj->lightMtx                       = &work->field_0.matrices.light;
    D_actor_403100_8015580C             = enemy;
    D_actor_403100_80155808             = work;
    obj->colorMtx                       = &work->field_0.matrices.color;
    arg0->msgTable                      = &D_actor_403100_801556EC;
    work->field_622                     = (s16)gGameSession->at4.loc.view;
    enemy->field_48                     = 0;
    enemy->field_4                      = &coord->coord;
    D_actor_403100_8015580C->bodyPos.vx = 0;
    D_actor_403100_8015580C->bodyPos.vy = 0;
    D_actor_403100_8015580C->bodyPos.vz = 0x300;
    D_actor_403100_8015580C->coord      = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&D_actor_403100_8015580C->node);
    kind = 9;
    TOUCH_REG(kind);
    D_actor_403100_8015580C->node.state.b.flags = kind;
    D_actor_403100_8015580C->param              = &D_actor_403100_8014762C;
    D_actor_403100_8015580C->recs               = (GpRec18*)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155630                     = arg0->extra.tmd->coords;
    flags                                       = &obj->flags;
    *flags                                      = 0;
    D_actor_403100_80155808->field_658          = -1;
    func_800B3F84(&D_actor_403100_80155808->field_B8.animation.anim, &D_actor_403100_8015572C, obj, &D_actor_403100_80155808->field_B8.legacy.pad_12E[0x1F6], D_actor_403100_80155808->field_B8.animation.slots);
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 1;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC(arg0);
    coord->sub = &gGfxViewCoord;
    func_actor_403100_80132320(arg0);
    for (i = 27; i >= 0; i--) {
        D_actor_403100_80155814[i].active = 0;
    }
    func_8017E4B8();
    func_8017E3C8();
    D_actor_403100_80155810            = 0;
    D_actor_403100_8015580C->hpMax     = D_actor_403100_80147630;
    D_actor_403100_8015580C->hp        = (s16)D_actor_403100_80147630;
    arg0->state                        = 1;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
}
/// Steps of the behaviour mode `func_actor_403100_8013E96C`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131EB0 = {
    {
        func_actor_403100_8013ED50,
        func_actor_403100_8013506C,
        (TaskFunc)func_actor_403100_8013EDDC,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013E9D8`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131EBC = {
    {
        func_actor_403100_8013EE28,
        func_actor_403100_801351F8,
        (TaskFunc)func_actor_403100_8013EEB0,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EA60`, indexed by `field_5FA`.
const TaskFuncTable4 D_actor_403100_80131EC8 = {
    {
        func_actor_403100_8013539C,
        func_actor_403100_8013EEB8,
        func_actor_403100_801354A0,
        (TaskFunc)func_actor_403100_8013EF24,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EAD4`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131ED8 = {
    {
        func_actor_403100_801355D4,
        func_actor_403100_801356F4,
        (TaskFunc)func_actor_403100_8013EF2C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EB68`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131EE4 = {
    {
        (TaskFunc)func_actor_403100_8013EF34,
        func_actor_403100_8013588C,
        (TaskFunc)func_actor_403100_8013EF58,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EBC8`, indexed by `field_5FA`.
const TaskFuncTable4 D_actor_403100_80131EF0 = {
    {
        func_actor_403100_801359DC,
        func_actor_403100_80135AE0,
        (TaskFunc)func_actor_403100_8013EF60,
        (TaskFunc)func_actor_403100_8013EFC0,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EC4C`, indexed by `field_5FA`.
const TaskFuncTable4 D_actor_403100_80131F00 = {
    {
        func_actor_403100_8013EFC8,
        func_actor_403100_80135C00,
        func_actor_403100_80135F30,
        func_actor_403100_80136100,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013ECD0`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131F10 = {
    {
        func_actor_403100_8013F034,
        func_actor_403100_8013631C,
        func_actor_403100_8013F0A8,
    },
};

/// The actor's six top-level states, by `Task::state`; run by
/// `func_actor_403100_8013E0FC`.
const TaskFuncTable6 D_actor_403100_80131F1C = {
    {
        func_actor_403100_80136610,
        func_actor_403100_80134D50,
        func_actor_403100_80136830,
        func_actor_403100_801339EC,
        func_actor_403100_8013D8F4,
        func_actor_403100_8013D88C,
    },
};

/// Behaviour modes of the actor's main state, indexed by `field_5F8`. Each
/// mode steps through its own table by `field_5FA`.
const TaskFuncTable11 D_actor_403100_80131F34 = {
    {
        (TaskFunc)func_actor_403100_8013DA6C,
        func_actor_403100_8013DAC4,
        func_actor_403100_8013DB48,
        func_actor_403100_8013DC18,
        func_actor_403100_8013DCAC,
        func_actor_403100_8013DD78,
        func_actor_403100_8013DE0C,
        func_actor_403100_8013DEA0,
        (TaskFunc)func_actor_403100_8013DF0C,
        (TaskFunc)func_actor_403100_8013DF64,
        func_actor_403100_8013DFBC,
    },
};

void func_actor_403100_80136830(Task* arg0)
{
    /* Share the scale and Euler workspaces to preserve the original frame. */
    s32             identity;
    TaskFuncTable11 stateHandlers;
    union {
        Actor403100VoidTable4 handlers;
        struct {
            VECTOR     scale;
            OverlayMat matrix;
        } scaling;
        struct {
            SVECTOR    angles;
            OverlayMat matrix;
        } rotation;
    } scratch;
    void *        scratcharg0, *scratcharg1, *scratcharg2, *scratcharg3, *scratcharg4, *scratcharg5, *scratcharg6;
    PlayerStatus* config = &Player_Status;

    s32        flashTimer;
    s16        lightTimer;
    s16        armTimer;
    s32        countdown;
    s32        flash;
    u8*        head;
    GpEnemy*   enemy;
    s32        flags, zero, z;
    VECTOR*    scratchHead;
    GpCoord*   side;
    VECTOR*    position;
    Task*      player;
    GpCoord*   coordinates;
    GpCoord*   center;
    TmdObject* obj;
    GpCoord*   playerCoord;

    obj           = arg0->extra.tmd;
    player        = *Gp_ActorSlots;
    stateHandlers = D_actor_403100_80131F34;
    armTimer      = D_actor_403100_80155808->field_5E6;
    if (armTimer != 0) {
        if ((armTimer == 1) && (D_actor_403100_8015580C->hp > 0)) {
            Gp_ArmStateF0(1);
        }
        D_actor_403100_80155808->field_5E6 = (s16)((u16)D_actor_403100_80155808->field_5E6 - 1);
    }
    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            if (player == NULL) {
                D_actor_403100_80155808->field_628 = 0;
            }
            func_actor_403100_801331D4(arg0);
            countdown = D_actor_403100_80155808->field_5D0;
            if (countdown >= 0) {
                D_actor_403100_80155808->field_5D0 = (s32)(countdown - 1);
            }
            if ((config->hp > 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
                if (D_actor_403100_8015580C->hp < (s16)(((s16)D_actor_403100_8015580C->hpMax * 0x23) / 100)) {
                    D_actor_403100_80155808->field_65C = 1U;
                } else {
                    D_actor_403100_80155808->field_65C = 0U;
                }
            }
            playerCoord                        = player->extra.tmd->coords;
            D_actor_403100_80155808->field_628 = func_actor_403100_8013D9C4((s16)playerCoord->coord.t[0], (s16)playerCoord->coord.t[2], D_actor_403100_80155638);
            {
                scratch.handlers = D_actor_403100_80131E24;
                scratch.handlers.funcs[D_actor_403100_80155808->field_5F2]();
            }
            stateHandlers.funcs[(s16)D_actor_403100_80155808->field_5F8](arg0);
            if ((D_actor_403100_80155808->field_65C != 0) && ((u8)D_actor_403100_80155808->pad_660[1] == 0) && ((u8)D_actor_403100_80155808->pad_670[1] == 0)) {
                D_actor_403100_80155808->field_5F8 = 9;
                D_actor_403100_80155808->field_5FA = 0;
            }
            func_actor_403100_8013CBE0(arg0);
            func_actor_403100_8013CDC0();
            func_actor_403100_8013BA64(arg0);
            func_actor_403100_801327CC(arg0);
            flashTimer                              = D_actor_403100_80155808->field_5FE;
            D_actor_403100_80155808->flags_634.half = (u16)D_actor_403100_80155808->field_B8.legacy.flags_104.half;
            if (flashTimer != 0) {
                if (flashTimer >= 0x10) {
                    flash = rsin(gDisplayState.animFrame << 9) << 0xD;
                } else {
                    flash = rsin(gDisplayState.animFrame << 9) << 0xC;
                }
                Display_ClampField126(flash >> 0x18);
                D_actor_403100_80155808->field_5FE = (s16)((u16)D_actor_403100_80155808->field_5FE - 1);
            } else {
                Display_ClampField126(0);
            }
            func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->field_61C);
            SOFT_USE_REG(arg0);
            identity = 0x1000;
            USE_REG(identity);
            {
                MATRIX * mtx, *dest;
                GpCoord* coords;
                coords                               = arg0->extra.tmd->coords;
                D_actor_403100_80155808->field_82    = (s32)((u16)D_actor_403100_80155808->field_82 << 20) >> 20;
                scratch.scaling.matrix.ident.m00_m01 = identity;
                mtx                                  = &scratch.scaling.matrix.mat;
                scratch.scaling.matrix.ident.m02_m10 = 0;
                *(s32*)&mtx->m[1][1]                 = identity;
                scratch.scaling.matrix.ident.m20_m21 = 0;
                mtx->m[2][2]                         = identity;
                func_8004BFF8(D_actor_403100_80155808->field_82, mtx);
                dest          = &coords->coord;
                dest->m[0][0] = scratch.scaling.matrix.mat.m[0][0];
                dest->m[0][1] = scratch.scaling.matrix.mat.m[0][1];
                dest->m[0][2] = scratch.scaling.matrix.mat.m[0][2];
                dest->m[1][0] = scratch.scaling.matrix.mat.m[1][0];
                dest->m[1][1] = scratch.scaling.matrix.mat.m[1][1];
                dest->m[1][2] = scratch.scaling.matrix.mat.m[1][2];
                dest->m[2][0] = scratch.scaling.matrix.mat.m[2][0];
                dest->m[2][1] = scratch.scaling.matrix.mat.m[2][1];
                dest->m[2][2] = scratch.scaling.matrix.mat.m[2][2];
                coords->flg   = 0;
                coords        = arg0->extra.tmd->coords;
                __asm__("la %0,%1" : "=r"(scratcharg0) : "m"(*(u8*)&scratch));
                scratch.scaling.scale.vx             = 0x1400;
                scratch.scaling.scale.vy             = 0x1400;
                scratch.scaling.scale.vz             = 0x1400;
                scratch.scaling.matrix.ident.m00_m01 = identity;
                mtx                                  = &scratch.scaling.matrix.mat;
                scratch.scaling.matrix.ident.m02_m10 = 0;
                *(s32*)&mtx->m[1][1]                 = identity;
                scratch.scaling.matrix.ident.m20_m21 = 0;
                mtx->m[2][2]                         = identity;
                ScaleMatrix(mtx, (VECTOR*)scratcharg0);
                MulMatrix(&coords->coord, mtx);
            }
            func_actor_403100_801328DC(arg0);
            {
                MATRIX *mtx, *dest;
                /* Kept across UpdateCoord; the unpinned lifetime displaces identity. */
                register GpCoord* coords asm("s3");
                coords        = arg0->extra.tmd->coords;
                dest          = &coords[6].coord;
                coords[6].flg = 0;
                mtx           = &scratch.rotation.matrix.mat;
                __asm__("la %0,%1" : "=r"(scratcharg1) : "m"(*(u8*)&scratch));
                scratch.rotation.matrix.ident.m00_m01 = identity;
                scratch.rotation.matrix.ident.m02_m10 = 0;
                *(s32*)&mtx->m[1][1]                  = identity;
                scratch.rotation.matrix.ident.m20_m21 = 0;
                mtx->m[2][2]                          = identity;
                Gp_MtxToEuler(dest, (SVECTOR*)scratcharg1);
                __asm__("la %0,%1" : "=r"(scratcharg2) : "m"(*(u8*)&scratch));
                scratch.rotation.angles.vz += D_actor_403100_80155808->field_A4;
                scratch.rotation.angles.vy += D_actor_403100_80155808->field_A2;
                scratch.rotation.angles.vx += D_actor_403100_80155808->field_A0;
                RotMatrix((SVECTOR*)scratcharg2, &scratch.rotation.matrix.mat);
                dest->m[0][0] = scratch.rotation.matrix.mat.m[0][0];
                dest->m[0][1] = scratch.rotation.matrix.mat.m[0][1];
                dest->m[0][2] = scratch.rotation.matrix.mat.m[0][2];
                dest->m[1][0] = scratch.rotation.matrix.mat.m[1][0];
                dest->m[1][1] = scratch.rotation.matrix.mat.m[1][1];
                dest->m[1][2] = scratch.rotation.matrix.mat.m[1][2];
                dest->m[2][0] = scratch.rotation.matrix.mat.m[2][0];
                dest->m[2][1] = scratch.rotation.matrix.mat.m[2][1];
                dest->m[2][2] = scratch.rotation.matrix.mat.m[2][2];
                coords       += 6;
                Gp_UpdateCoord(coords);
                USE_REG(coords);
            }
            {
                MATRIX * mtx, *dest;
                GpCoord* coords;
                coords = arg0->extra.tmd->coords;
                mtx    = &scratch.rotation.matrix.mat;
                __asm__("la %0,%1" : "=r"(scratcharg3) : "m"(*(u8*)&scratch));
                scratch.rotation.matrix.ident.m00_m01 = identity;
                scratch.rotation.matrix.ident.m02_m10 = 0;
                *(s32*)&mtx->m[1][1]                  = identity;
                scratch.rotation.matrix.ident.m20_m21 = 0;
                mtx->m[2][2]                          = identity;
                dest                                  = &coords[4].coord;
                Gp_MtxToEuler(dest, (SVECTOR*)scratcharg3);
                __asm__("la %0,%1" : "=r"(scratcharg4) : "m"(*(u8*)&scratch));
                scratch.rotation.angles.vx += D_actor_403100_80155808->field_5E8;
                RotMatrix((SVECTOR*)scratcharg4, &scratch.rotation.matrix.mat);
                dest->m[0][0] = scratch.rotation.matrix.mat.m[0][0];
                dest->m[0][1] = scratch.rotation.matrix.mat.m[0][1];
                dest->m[0][2] = scratch.rotation.matrix.mat.m[0][2];
                dest->m[1][0] = scratch.rotation.matrix.mat.m[1][0];
                dest->m[1][1] = scratch.rotation.matrix.mat.m[1][1];
                dest->m[1][2] = scratch.rotation.matrix.mat.m[1][2];
                dest->m[2][0] = scratch.rotation.matrix.mat.m[2][0];
                dest->m[2][1] = scratch.rotation.matrix.mat.m[2][1];
                dest->m[2][2] = scratch.rotation.matrix.mat.m[2][2];
            }
            {
                MATRIX * mtx, *dest;
                GpCoord* coords;
                coords = arg0->extra.tmd->coords;
                mtx    = &scratch.rotation.matrix.mat;
                __asm__("la %0,%1" : "=r"(scratcharg5) : "m"(*(u8*)&scratch));
                scratch.rotation.matrix.ident.m00_m01 = identity;
                scratch.rotation.matrix.ident.m02_m10 = 0;
                *(s32*)&mtx->m[1][1]                  = identity;
                scratch.rotation.matrix.ident.m20_m21 = 0;
                mtx->m[2][2]                          = identity;
                dest                                  = &coords[3].coord;
                Gp_MtxToEuler(dest, (SVECTOR*)scratcharg5);
                __asm__("la %0,%1" : "=r"(scratcharg6) : "m"(*(u8*)&scratch));
                scratch.rotation.angles.vx += D_actor_403100_80155808->field_5EA;
                RotMatrix((SVECTOR*)scratcharg6, &scratch.rotation.matrix.mat);
                dest->m[0][0] = scratch.rotation.matrix.mat.m[0][0];
                dest->m[0][1] = scratch.rotation.matrix.mat.m[0][1];
                dest->m[0][2] = scratch.rotation.matrix.mat.m[0][2];
                dest->m[1][0] = scratch.rotation.matrix.mat.m[1][0];
                dest->m[1][1] = scratch.rotation.matrix.mat.m[1][1];
                dest->m[1][2] = scratch.rotation.matrix.mat.m[1][2];
                dest->m[2][0] = scratch.rotation.matrix.mat.m[2][0];
                dest->m[2][1] = scratch.rotation.matrix.mat.m[2][1];
                dest->m[2][2] = scratch.rotation.matrix.mat.m[2][2];
            }
            func_actor_403100_8013335C(arg0);
            if (D_actor_403100_80155808->field_60C != 0) {
                lightTimer = --D_actor_403100_80155808->field_60C;
                if ((s16)lightTimer == 0) {
                    Gp_SetLightMode(arg0->spawnArg2, 0);
                }
            }
            if ((D_actor_403100_8015580C->hp <= 0) && ((u8)D_actor_403100_80155808->pad_670[3] == 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
                if (config->hp <= 0) {
                    D_actor_403100_8015580C->hp = 0x3E8;
                } else {
                    D_actor_403100_80155808->field_658     = -1;
                    D_actor_403100_80155808->field_618     = 0x1400;
                    gGameSession->suppressDeathChecks      = 0;
                    Gp_StateC08.field_6                    = (u8)(Gp_StateC08.field_6 | 1);
                    gGameSession->field_12C                = 0;
                    D_actor_403100_8015580C->reactionFlags = 0;
                    Gp_SetLightMode(arg0->spawnArg2, 0);
                    D_actor_403100_8015580C->node.state.b.flags = 9;
                    func_800E8614((s32)&D_80166098, 0);
                    arg0->state                        = 1;
                    D_actor_403100_80155808->field_5F8 = 0;
                    D_actor_403100_80155808->field_5FA = 0;
                    D_actor_403100_80155808->field_5F8 = 9;
                    D_actor_403100_80155808->field_5FA = 0;
                    case 1:
                }
            }
            func_actor_403100_8013480C(arg0, 0x96);
            coordinates        = arg0->extra.tmd->coords;
            coordinates[8].flg = 0;
            coordinates[7].flg = 0;
            coordinates[6].flg = 0;
            coordinates[5].flg = 0;
            coordinates[4].flg = 0;
            coordinates[3].flg = 0;
            coordinates[2].flg = 0;
            coordinates[1].flg = 0;
            coordinates[0].flg = 0;
            side               = coordinates + 4;
            center             = coordinates + 3;
            Gp_UpdateCoord(coordinates + 8);
            USE_REG(center);
            Gp_UpdateCoord(side);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(scratchHead));
            scratchHead        = *(VECTOR**)((u8*)scratchHead + 0x3FC);
            scratchHead[-1].vx = center->workm.t[0];
            position           = scratchHead - 1;
            position->vy       = center->workm.t[1];
            z                  = center->workm.t[2];
            __asm__("addu %0,$zero,$zero" : "=r"(zero) : "r"(z));
            position->vz = z;
            enemy        = arg0->spawnArg2;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(position), "r"(enemy), "r"(zero) : "memory");
            Gp_UpdateActorColor(enemy, position, zero, zero);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            head  = *(u8**)(head + 0x3FC);
            head += 16;
            flags = obj->flags & 0xFF7F;
            __asm__ volatile("sw %0, 0x1F8003FC" : "+r"(head) : "r"(flags) : "memory");
            obj->flags = flags;
            return;
    }
}
void func_actor_403100_8013712C(Task* arg0)
{
    Actor403100Entry* entries;
    GpObj*            obj;
    s32               i;
    GpCoord*          coord;
    Actor403100Work*  work;

    coord                                       = arg0->extra.tmd->coords;
    D_actor_403100_8015580C->node.state.b.flags = 8;
    D_actor_403100_80155808->field_5E6          = 0x1E;
    D_actor_403100_80155808->field_62C          = 0x20;
    D_actor_403100_80155808->field_600          = 0;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    i                                        = 0;
    entries                                  = D_actor_403100_80155814;
    obj                                      = &entries->obj;
    work                                     = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    coord->coord.t[0]                        = -0x44C;
    coord->coord.t[2]                        = 0x980;
    *(volatile s16*)&D_actor_403100_80155810 = 0;
    coord->coord.t[1]                        = 0;
    work->field_82                           = 0xC00;
    work->field_5D0                          = 0x1518;
    work->field_5E2                          = 0x10;
    work->field_5DE                          = 1;
    work->field_80                           = 0;
    work->field_84                           = 0;
    work->field_5DA                          = 2;
    work->field_5EC                          = 0;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        /* The display node walks as its own pointer, one entry at a time:
           spelled `&entries[i].obj` it folds into the walk of `entries[i]`,
           and the ROM keeps the two apart. */
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5FA += 1;
}
/// Steps of the behaviour mode `func_actor_403100_8013DAC4`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80131F60 = {
    {
        (TaskFunc)func_actor_403100_8013F18C,
        (TaskFunc)func_actor_403100_80137268,
        (TaskFunc)func_actor_403100_80137310,
    },
};

void func_actor_403100_80137268(void)
{
    s16 state;

    state = D_actor_403100_80155808->field_628 - 1;
    switch (state) {
        case 0:
            if (D_actor_403100_80155808->field_62E < 0x1F40) {
                D_actor_403100_80155808->field_5FA += 1;
            }
            break;
        case 1:
        case 5:
            D_actor_403100_80155808->field_5FA += 1;
            break;
        case 2:
        case 3:
        case 4:
            if (D_actor_403100_80155808->field_62E < 0x17D4) {
                D_actor_403100_80155808->field_5FA += 1;
            }
            break;
    }
}
void func_actor_403100_80137310(void)
{
    s32 halfHealth;
    s16 phase;
    s16 previousState;
    s16 state;
    u32 random1;
    u32 random2;

    halfHealth = Player_Status.hpMax / 2;
    phase      = D_actor_403100_80155808->field_628;
    if (phase != 2 && phase != 6) {
        if ((Player_Status.hp < halfHealth) || (D_actor_403100_80155808->field_65C != 0)) {
            random1                            = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState                        = random1;
            D_actor_403100_80155808->field_5F8 = D_actor_403100_801557B0[1][(random1 >> 16) & 15];
            D_actor_403100_80155808->field_5FA = 0;
        } else {
            random2                            = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState                        = random2;
            D_actor_403100_80155808->field_5F8 = D_actor_403100_801557B0[0][(random2 >> 16) & 15];
            D_actor_403100_80155808->field_5FA = 0;
        }
        if ((s16)D_actor_403100_80155808->field_5F8 == 4) {
            if (D_actor_403100_80155808->field_65C != 0) {
                D_actor_403100_80155808->field_5F8 = 2;
                D_actor_403100_80155808->field_5FA = 0;
            }
        }
        if ((s16)D_actor_403100_80155808->field_5F8 == 5) {
            if (D_actor_403100_80155808->field_628 != 1) {
                D_actor_403100_80155808->field_5F8 = 7;
                D_actor_403100_80155808->field_5FA = 0;
            }
        }
        if (((s16)D_actor_403100_80155808->field_5F8 == 7) && ((u32)((u16)D_actor_403100_80155808->field_628 - 3) >= 2U)) {
            D_actor_403100_80155808->field_5F8 = 4;
            D_actor_403100_80155808->field_5FA = 0;
        }
        D_actor_403100_80155808->field_612[(u8)D_actor_403100_80155808->pad_66A[3]] = (u16)D_actor_403100_80155808->field_5F8;
        previousState                                                               = D_actor_403100_80155808->field_612[0];
        if ((previousState == D_actor_403100_80155808->field_612[1]) && (previousState == D_actor_403100_80155808->field_612[2])) {
            state = (s16)D_actor_403100_80155808->field_5F8;
            if (state == 2 || state == 4 || state == 7) {
                D_actor_403100_80155808->field_5F8 = 3;
                D_actor_403100_80155808->field_5FA = 0;
            } else if (state == 3) {
                if (D_actor_403100_80155808->field_65C != 0) {
                    D_actor_403100_80155808->field_5F8 = 2;
                    D_actor_403100_80155808->field_5FA = 0;
                } else {
                    D_actor_403100_80155808->field_5F8 = 4;
                    D_actor_403100_80155808->field_5FA = 0;
                }
            }
            D_actor_403100_80155808->field_612[(u8)D_actor_403100_80155808->pad_66A[3]] = (u16)D_actor_403100_80155808->field_5F8;
        }
        D_actor_403100_80155808->pad_66A[3] = (u8)D_actor_403100_80155808->pad_66A[3] + 1;
        if ((u8)D_actor_403100_80155808->pad_66A[3] >= 3U) {
            D_actor_403100_80155808->pad_66A[3] = 0;
        }
    } else {
        D_actor_403100_80155808->field_5F8 = 6;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_801375B8(void)
{
    u16 angle;
    u32 random2;
    u32 random1;

    random1                             = (Gp_LcgState * 5) + 0x71357911;
    random2                             = (random1 * 5) + 0x71357911;
    Gp_LcgState                         = random2;
    D_actor_403100_80155808->pad_65E[0] = ((random1 >> 0x10) & 1) + (((random2 >> 0x10) & 1) + 1);
    D_actor_403100_80155808->field_62C  = 0x20;
    angle                               = (u16)D_actor_403100_80155808->field_B2;
    D_actor_403100_80155808->field_5F6  = 0;
    D_actor_403100_80155808->field_61C  = 4;
    D_actor_403100_80155808->field_604  = 0;
    D_actor_403100_80155808->field_608  = 0;
    D_actor_403100_80155808->field_626  = (s16)angle;
    if ((s16)angle >= 0xD1) {
        D_actor_403100_80155808->field_626 = 0xD0;
    }
    if (D_actor_403100_80155808->field_626 < -0x160) {
        D_actor_403100_80155808->field_626 = -0x160;
    }
    D_actor_403100_80155808->field_5DE             = 4;
    D_actor_403100_80155808->field_5E2             = 0xC;
    D_actor_403100_80155808->field_5DA             = 2;
    D_actor_403100_80155808->field_5EC             = 0;
    D_actor_403100_80155808->field_55C.flags      &= 0x7FFF;
    D_actor_403100_80155808->field_594.flags      &= 0x7FFF;
    D_actor_403100_80155808->field_668.b.field_668 = 0;
    D_actor_403100_80155808->field_668.b.field_669 = 0;
    D_actor_403100_80155808->field_5FA            += 1;
    D_actor_403100_80155808->field_664.b.field_665 = 0;
    D_actor_403100_80155808->field_664.b.field_666 = 0;
    D_actor_403100_80155808->pad_66A[4]            = 0;
}
void func_actor_403100_801376D8(Task* arg0)
{
    Task* task;
    s32   sound;
    s32   pan;
    u16   counter;
    u16   angle;

    if ((u32)(D_actor_403100_80155808->field_5EC - 0x36) < 4U) {
        func_actor_403100_8013C7B4(arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x39) {
        func_actor_403100_801342B4(arg0);
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0003;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[7]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[7]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    D_actor_403100_80155808->field_5EC += 1;
    if (D_actor_403100_80155808->field_668.flags != 0) {
        D_actor_403100_80155808->field_5F6 = 4;
        if (D_actor_403100_80155808->field_5F2 == 0) {
            Gp_StateC08.field_6 |= 1;
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->field_5F4 = 0x17;
            D_actor_403100_80155808->field_5F2 = 1;
            task                               = gameGetPtrSlot(3);
            if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
                ((GameActor*)(*Gp_ActorSlots)->work)->field_956 = 0xA;
            }
        }
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->pad_66A[4]            = 1;
        func_actor_403100_8013D2A0(1);
    }
    D_actor_403100_80155808->field_608 += (s16)((D_actor_403100_80155808->field_626 - D_actor_403100_80155808->field_608) << 4) >> 7;
    if (Actor403100_TestFlags104()) {
        if ((u8)D_actor_403100_80155808->pad_66A[4] != 0) {
            D_actor_403100_80155808->field_5E2        = 0x10;
            D_actor_403100_80155808->field_5DE        = 5;
            D_actor_403100_80155808->field_5DA        = 2;
            D_actor_403100_80155808->field_5FA       += 2;
            D_actor_403100_80155808->field_55C.flags &= 0x7FFF;
            D_actor_403100_80155808->field_594.flags &= 0x7FFF;
            return;
        }
        counter                                             = D_actor_403100_80155808->field_5FA;
        *(volatile s16*)&D_actor_403100_80155808->field_5E2 = 0xA;
        *(volatile s16*)&D_actor_403100_80155808->field_5DE = 2;
        *(volatile s16*)&D_actor_403100_80155808->field_5DA = 2;
        angle                                               = *(volatile u16*)&D_actor_403100_80155808->field_B2;
        D_actor_403100_80155808->field_5EC                  = 0;
        D_actor_403100_80155808->field_626                  = (s16)angle;
        D_actor_403100_80155808->field_5FA                  = counter + 1;
        if ((s16)angle >= 0xD1) {
            D_actor_403100_80155808->field_626 = 0xD0;
        }
        if (D_actor_403100_80155808->field_626 < -0x160) {
            D_actor_403100_80155808->field_626 = -0x160;
        }
    }
}
void func_actor_403100_801379B4(Task* arg0)
{
    Task* task;
    s32   sound;
    s32   pan;
    s8    count;
    u16   angle;

    if ((u32)(D_actor_403100_80155808->field_5EC - 0x3C) < 9U) {
        func_actor_403100_8013C7B4(arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x44) {
        func_actor_403100_801342B4(arg0);
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0003;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[7]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[7]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    D_actor_403100_80155808->field_5EC += 1;
    if (D_actor_403100_80155808->field_668.flags != 0) {
        D_actor_403100_80155808->field_5F6 = 4;
        if (D_actor_403100_80155808->field_5F2 == 0) {
            Gp_StateC08.field_6 |= 1;
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->field_5F4 = 0x17;
            D_actor_403100_80155808->field_5F2 = 1;
            task                               = gameGetPtrSlot(3);
            if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
                ((GameActor*)(*Gp_ActorSlots)->work)->field_956 = 0xA;
            }
        }
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        func_actor_403100_8013D2A0(1);
        D_actor_403100_80155808->pad_66A[4] = 1;
        D_actor_403100_80155808->pad_65E[0] = 1;
    }
    D_actor_403100_80155808->field_608 += (s16)((D_actor_403100_80155808->field_626 - D_actor_403100_80155808->field_608) << 4) >> 7;
    if (Actor403100_TestFlags104()) {
        count                               = (u8)D_actor_403100_80155808->pad_65E[0] - 1;
        D_actor_403100_80155808->pad_65E[0] = count;
        if (!(count & 0xFF)) {
            D_actor_403100_80155808->field_5E2        = 0x10;
            D_actor_403100_80155808->field_5DE        = 5;
            D_actor_403100_80155808->field_5DA        = 2;
            D_actor_403100_80155808->field_5FA       += 1;
            D_actor_403100_80155808->field_55C.flags &= 0x7FFF;
            D_actor_403100_80155808->field_594.flags &= 0x7FFF;
            return;
        }
        D_actor_403100_80155808->field_5E2 = 0xA;
        angle                              = (u16)D_actor_403100_80155808->field_B2;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5DE = 2;
        D_actor_403100_80155808->field_5DA = 2;
        D_actor_403100_80155808->field_626 = (s16)angle;
        if ((s16)angle >= 0xD1) {
            D_actor_403100_80155808->field_626 = 0xD0;
        }
        if (D_actor_403100_80155808->field_626 < -0x140) {
            D_actor_403100_80155808->field_626 = -0x140;
        }
    }
}
void func_actor_403100_80137CA8(void)
{
    u16 angleY;
    u16 angleX;
    u32 random;

    angleX                             = (u16)D_actor_403100_80155808->field_604;
    angleY                             = (u16)D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_604 = angleX + ((s32) - (angleX << 0x14) >> 0x17);
    D_actor_403100_80155808->field_608 = angleY + ((s32) - (angleY << 0x14) >> 0x17);
    if (Actor403100_TestFlags104()) {
        if ((u8)D_actor_403100_80155808->pad_66A[4] != 0) {
            D_actor_403100_80155808->field_5EC = 0;
            random                             = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState                        = random;
            if (!((random >> 0x10) & 3)) {
                D_actor_403100_80155808->field_5FC  = 0x14;
                D_actor_403100_80155808->field_5E2  = 0x1C;
                D_actor_403100_80155808->field_5DE  = 3;
                D_actor_403100_80155808->field_5DA  = 1;
                D_actor_403100_80155808->field_5FA += 1;
                return;
            }
            D_actor_403100_80155808->field_5FC  = 8;
            D_actor_403100_80155808->field_5E2  = 0x20;
            D_actor_403100_80155808->field_5DE  = 1;
            D_actor_403100_80155808->field_5DA  = 1;
            D_actor_403100_80155808->field_5FA += 2;
            return;
        }
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_80137DC4(Task* arg0)
{
    s32 sound;
    s32 sound2;
    s32 pan;
    s32 pan2;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_80137F4C(void)
{
    Actor403100Entry* entry;
    GpObj*            obj;
    Actor403100Entry* entries;
    s32               i;

    D_actor_403100_80155808->field_5F6 = 0;
    if (D_actor_403100_80155808->field_65C == 0) {
        D_actor_403100_80155808->field_62C = 0x1C;
    } else {
        D_actor_403100_80155808->field_62C = 0x30;
    }
    i                                         = 0;
    entries                                   = D_actor_403100_80155814;
    obj                                       = &D_actor_403100_80155814->obj;
    entry                                     = entries;
    D_actor_403100_80155810                   = 0;
    D_actor_403100_80155808->flags_634.h.high = 0x14;
    for (; i < 0x1C; i++) {
        if (entry->active != 0) {
            entry->active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
        entry++;
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 7;
    D_actor_403100_80155808->field_5DA  = 2;
    D_actor_403100_80155808->field_61C  = 1;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_80138048(Task* arg0)
{
    SVECTOR  offset;
    SVECTOR  velocity;
    GpCoord* effectCoord;
    s32      sound;
    s32      sound2;
    s32      state;
    s32      pan;
    s32      pan2;

    if (D_actor_403100_80155810 == 1) {
        D_actor_403100_80155808->field_5FA = 3;
        return;
    }
    state                               = (s8)D_actor_403100_80155808->pad_66A[2];
    D_actor_403100_80155808->field_5EC += 1;
    if (state == 1) {
        if (D_actor_403100_80155808->field_664.b.field_666 == 0) {
            D_actor_403100_80155808->field_664.b.field_666 = (u8)state;
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC < 0x33) {
        if (D_actor_403100_80155808->field_628 == 4) {
            D_actor_403100_80155808->field_98  = -0x1770;
            D_actor_403100_80155808->field_9A  = -0xC80;
            D_actor_403100_80155808->field_9C  = -0x1B58;
            D_actor_403100_80155808->field_61C = 3;
        }
        if (D_actor_403100_80155808->field_628 == 5) {
            D_actor_403100_80155808->field_98  = 0x500;
            D_actor_403100_80155808->field_9A  = -0xC80;
            D_actor_403100_80155808->field_9C  = 0x2710;
            D_actor_403100_80155808->field_61C = 3;
        }
        if ((s16)D_actor_403100_80155808->field_5EC < 0x33) {
            func_80182730();
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x33) {
        D_actor_403100_80155808->field_61C = 5;
        if (D_actor_403100_80155808->field_65C) {
            D_actor_403100_80155808->field_65A = 0x10;
        } else {
            D_actor_403100_80155808->field_65A = 8;
        }
    }
    if (((s16)D_actor_403100_80155808->field_5EC == 0x3D) && (D_actor_403100_80155808->field_65C == 0)) {
        D_actor_403100_80155808->field_61C = 0;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        D_actor_403100_80155808->field_61C = 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x33) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x34) < 0x48U) {
        offset.vy   = -0x1F0;
        offset.vz   = 0x620;
        velocity.vy = -0x20;
        offset.vx   = 0;
        velocity.vx = 0;
        velocity.vz = 0xF0;
        func_actor_403100_80132064(arg0, &offset, &velocity, 0);
        func_actor_403100_8013D11C(arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7E) {
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x7D) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_61C  = 1;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_8013842C(Task* arg0)
{
    SVECTOR  offset;
    s32      sound;
    s32      sound2;
    s32      pan;
    GpCoord* effectCoord;
    s32      pan2;
    u16      frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE             = 0x12;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x32) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_80138610(Task* arg0)
{
    u16      counter;
    u16      timer;
    u16      angle;
    GpCoord* coord;

    coord = arg0->extra.tmd->coords;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        coord->coord.t[1]                 += 0x30;
        timer                              = D_actor_403100_80155808->field_5EC;
        D_actor_403100_80155808->field_5EC = timer + 1;
        if ((s16)timer >= 0x11) {
            counter                                             = *(volatile u16*)&D_actor_403100_80155808->field_5FA;
            *(volatile s16*)&D_actor_403100_80155808->field_61E = 0;
            *(volatile s16*)&D_actor_403100_80155808->field_61E = 2;
            angle                                               = *(volatile u16*)&D_actor_403100_80155808->field_B2;
            D_actor_403100_80155808->field_5EC                  = 0;
            D_actor_403100_80155808->field_620                  = 0;
            D_actor_403100_80155808->field_626                  = (s16)angle;
            D_actor_403100_80155808->field_5FA                  = counter + 1;
            if ((s16)angle >= 0xD1) {
                D_actor_403100_80155808->field_626 = 0xD0;
            }
            if (D_actor_403100_80155808->field_626 < -0x160) {
                D_actor_403100_80155808->field_626 = -0x160;
            }
        }
    }
}
void func_actor_403100_801386DC(Task* arg0)
{
    u16      velocity;
    u16      accel;
    GpCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    D_actor_403100_80155808->field_5EC += 1;
    accel                               = D_actor_403100_80155808->field_61E + 4;
    velocity                            = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620  = velocity;
    D_actor_403100_80155808->field_61E  = accel;
    coord->coord.t[1]                  -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->field_5EC >= 6) {
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138790(Task* arg0)
{
    u16      velocity;
    u16      accel;
    GpCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    D_actor_403100_80155808->field_5EC += 1;
    accel                               = D_actor_403100_80155808->field_61E - 4;
    velocity                            = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620  = velocity;
    D_actor_403100_80155808->field_61E  = accel;
    coord->coord.t[1]                  -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->field_5EC >= 6) {
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138844(Task* arg0)
{
    Task*    player;
    s32      sound;
    s32      sound2;
    s32      y;
    s32      pan;
    s32      pan2;
    u16      velocity;
    u16      accel;
    GpCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    if ((D_actor_403100_80155808->field_668.flags != 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
        Gp_StateC08.field_6 |= 1;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->field_5F4 = 0x17;
        D_actor_403100_80155808->field_5F2 = 1;
        player                             = gameGetPtrSlot(3);
        if (Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
            ((GameActor*)(*Gp_ActorSlots)->work)->field_956 = 0xA;
        }
    }
    func_actor_403100_8013C7B4(arg0);
    D_actor_403100_80155808->field_5EC += 1;
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    accel                              = D_actor_403100_80155808->field_61E - 4;
    velocity                           = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620 = velocity;
    D_actor_403100_80155808->field_61E = accel;
    y                                  = coord->coord.t[1] - (s16)velocity;
    coord->coord.t[1]                  = y;
    if (y >= 0) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        func_actor_403100_801342B4(arg0);
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
        sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan                                = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0003;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[7]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[7]) / 2));
        coord->coord.t[1]                   = 0;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138AB4(void)
{
    Task* player;
    u16   frame;

    func_actor_403100_8013D24C();
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if (((s16)frame < 0x20) && ((u8)D_actor_403100_80155808->field_668.b.field_668 != 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
        Gp_StateC08.field_6 |= 1;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->field_5F4 = 0x17;
        D_actor_403100_80155808->field_5F2 = 1;
        player                             = gameGetPtrSlot(3);
        if (Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
            ((GameActor*)(*Gp_ActorSlots)->work)->field_956 = 0xA;
        }
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 5;
        D_actor_403100_80155808->field_5F6  = 0;
        D_actor_403100_80155808->field_5DA  = 2;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138C18(void)
{
    u16 velocityZ;
    u16 velocityX;
    u32 random;

    func_actor_403100_8013D24C();
    velocityX                          = (u16)D_actor_403100_80155808->field_604;
    velocityZ                          = (u16)D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_604 = velocityX + ((s32) - (velocityX << 0x14) >> 0x17);
    D_actor_403100_80155808->field_608 = velocityZ + ((s32) - (velocityZ << 0x14) >> 0x17);
    if (Actor403100_TestFlags104()) {
        random      = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState = random;
        if ((random >> 0x10) & 1) {
            func_actor_403100_8013D2A0(1);
        }
        D_actor_403100_80155808->field_5FC  = 8;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5DE  = 1;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138D08(Task* arg0)
{
    TmdObject*       obj;
    Actor403100Work* work;

    obj = arg0->extra.tmd;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        obj->otOffset                                  = 0;
        work                                           = D_actor_403100_80155808;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        work->field_62C                                = 0x40;
        work->field_5E2                                = 0x10;
        work->field_5DE                                = 9;
        work->field_5F6                                = 0;
        work->field_5DA                                = 2;
        work->field_61C                                = 2;
        work->field_5EC                                = 0;
        work->field_604                                = 0;
        work->field_608                                = 0;
        work->field_632                                = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->pad_670[3]            = 0;
        D_actor_403100_80155808->field_5FA            += 1;
    }
}
void func_actor_403100_80138DB0(Task* arg0)
{
    s32              sound;
    s32              pan;
    s32              depth;
    Task*            player;
    Actor403100Work* work;

    player                             = (Task*)Gp_ActorSlots[0];
    D_actor_403100_80155808->field_5EC = (u16)(D_actor_403100_80155808->field_5EC + 1);
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        if ((s16)D_actor_403100_80155808->field_5EC < 0x101) {
            func_actor_403100_8013C7B4(arg0);
            work = D_actor_403100_80155808;
            if ((u8)work->field_668.b.field_668 != 0) {
                work->pad_670[3]                    = 1;
                D_actor_403100_80155808->pad_670[1] = 1;
                Gp_StateC08.field_6                 = (u8)(Gp_StateC08.field_6 | 1);
                sound                               = (((u16)((GpEnemy*)player->spawnArg2)->placeKey >> 0xC) << 8) | 7;
                pan                                 = (s8)Gp_GetObjPan(&player->extra.tmd->coords[1]);
                depth                               = gpGetObjDepth(&player->extra.tmd->coords[1]);
                SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
                D_actor_403100_80155808->field_65F  = 0;
                D_actor_403100_80155808->pad_660[0] = 1;
                func_actor_403100_8013D1B8(1, 0x3F4);
                D_actor_403100_80155808->field_5EC  = 0U;
                D_actor_403100_80155808->pad_65E[0] = 0;
                D_actor_403100_80155808->field_5FA += 1;
            } else if ((u8)work->field_668.b.field_669 != 0) {
                work->field_5E2 = -0x10;
                work->field_5FA = 0xA;
            }
            D_actor_403100_80155808->field_61C = 0;
        } else {
            D_actor_403100_80155808->field_61C = 2;
        }
        if (Actor403100_TestFlags104()) {
            D_actor_403100_80155808->field_5F8 = 1;
            D_actor_403100_80155808->field_5FA = 0U;
        }
    }
}
void func_actor_403100_80138F88(Task* arg0)
{
    s32              message[6];
    Actor403100Work* work;
    s32              sound;
    s32              y;
    s32              pan;
    s32              depth;
    GpCoord*         coords;
    GpCoord*         part;

    coords = arg0->extra.tmd->coords;
    part   = coords + 6;
    func_actor_403100_8013C008(D_actor_403100_80155808->field_60E, D_actor_403100_80155808->field_610);
    D_actor_403100_80155808->field_60E = (u16)D_actor_403100_80155808->field_60E - 1;
    D_actor_403100_80155808->field_610 = (u16)D_actor_403100_80155808->field_610 + 6;
    y                                  = *(s32*)(u32)&coords->coord.t[1];
    *(s32*)(u32)&coords->coord.t[1]    = y + (-y >> 6);
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_82  = 0xA00;
    D_actor_403100_80155808->field_84  = 0;
    func_actor_403100_80132528(arg0);
    if (Actor403100_TestFlags104()) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F000C;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        Mc_SaveData.at4.loc.view        = 0xB;
        *(s32*)(u32)&coords->coord.t[0] = -0x44C;
        work                            = D_actor_403100_80155808;
        SOFT_TOUCH_REG(work);
        *(s32*)(u32)&coords->coord.t[2] = 0x1770;
        *(s32*)(u32)&coords->coord.t[1] = 0;
        work->field_82                  = 0xC00;
        work->field_5E2                 = 0x10;
        work->field_5DE                 = 0xC;
        work->field_5DA                 = 2;
        work->field_604                 = 0xD0;
        work->field_608                 = -0x350;
        work->field_A0                  = -0x110;
        work->field_A2                  = 0x290;
        work->field_80                  = 0;
        work->field_84                  = 0;
        work->field_5EC                 = 0;
        work->field_A4                  = 0x60;
        work->field_5FA                += 1;
        part->coord.t[0]                = -0xBD0;
        work->field_604                 = 0x30;
        work->field_608                 = -0xD0;
        work->field_A0                  = -0x150;
        work->field_A2                  = 0x270;
        work->field_A4                  = -0xA0;
        part->coord.t[0]                = -0x1120;
        work->field_47C.radius          = 0x500;
        func_actor_403100_8013D74C(arg0);
        D_actor_403100_80155808->field_664.b.field_667 = 0;
        D_actor_403100_80155808->field_65F             = 0;
        D_actor_403100_80155808->pad_660[0]            = 0;
        D_actor_403100_80155808->field_5E8             = 0;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        func_actor_403100_8013D1B8(1, 0x3FF);
        message[5] = 0x28;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)message, 0);
        D_actor_403100_80155808->field_656 = (u16)D_actor_403100_8015580C->hp;
    }
}
void func_actor_403100_8013922C(Task* arg0)
{
    s32      message[6];
    s16      health;
    s32      damage;
    s32      state;
    u16      frame;
    u32      random;
    u8       request;
    GpCoord* coords;
    GpCoord* part;

    coords                              = arg0->extra.tmd->coords;
    part                                = coords + 6;
    D_actor_403100_80155808->field_5EC += 1;
    func_actor_403100_8013D6B4(arg0);
    func_actor_403100_8013C214(arg0);
    func_actor_403100_8013C214(arg0);
    D_actor_403100_80155808->field_82 = 0xC00;
    D_actor_403100_80155808->field_80 = 0;
    D_actor_403100_80155808->field_84 = 0;
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        request = (u8)D_actor_403100_80155808->field_65F;
        if ((request == 1) && ((u8)D_actor_403100_80155808->field_664.b.field_667 == request)) {
            random                             = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState                        = random;
            D_actor_403100_80155808->field_638 = (((random >> 0x10) & 0x1F) + 0x3C) * 3;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x400, 0, 0);
            D_actor_403100_80155808->field_65F = 0;
        }
    } else {
        frame                              = D_actor_403100_80155808->field_654 + 1;
        D_actor_403100_80155808->field_654 = frame;
        if ((s16)frame == 0x3C) {
            func_actor_403100_8013D2A0(1);
        }
        if ((s16)D_actor_403100_80155808->field_654 >= 0x79) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
    func_actor_403100_80132528(arg0);
    func_actor_403100_8013D700(arg0);
    func_actor_403100_8013C214(arg0);
    if (D_actor_403100_80155808->pad_66A[2] != 0) {
        if (D_actor_403100_80155808->field_5DE != 0xD) {
            D_actor_403100_80155808->field_5DE = 0xD;
            D_actor_403100_80155808->field_5E2 = 0x10;
            D_actor_403100_80155808->field_5DA = 2;
            if ((u8)D_actor_403100_80155808->field_664.b.field_665 == 0) {
                D_actor_403100_80155808->field_66F             = 1;
                D_actor_403100_80155808->field_664.b.field_665 = 1;
            }
        }
    } else {
        if (Actor403100_TestFlags()) {
            D_actor_403100_80155808->field_5E2 = 0x10;
            D_actor_403100_80155808->field_5DE = 0xC;
            D_actor_403100_80155808->field_5DA = 2;
        }
    }
    if ((u8)D_actor_403100_80155808->pad_660[0] != 0) {
        D_actor_403100_80155808->pad_660[0] = 0;
        func_actor_403100_8013D1B8(1, 0x3FF);
        message[5] = 0x28;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)message, 0);
        D_actor_403100_80155808->pad_65E[0] = (u8)D_actor_403100_80155808->pad_65E[0] + 1;
    }
    if (((u8)D_actor_403100_80155808->pad_65E[0] != 0) ||
        (((u8)D_actor_403100_80155808->field_664.b.field_667 == 1) &&
         ((s16)D_actor_403100_80155808->field_5EC >= 0x1C2) &&
         ((u8)D_actor_403100_80155808->pad_670[0] == 0)) ||
        (D_actor_403100_8015580C->hp <= 0)) {
        damage = (s16)D_actor_403100_80155808->field_656 - D_actor_403100_8015580C->hp;
        health = D_actor_403100_8015580C->hp;
        if ((damage >= 0x3C) && (health > 0)) {
            D_actor_403100_80155808->field_5EC  = 0;
            D_actor_403100_80155808->field_5FA += 1;
            return;
        }
        func_actor_403100_8013D1B8(1, 0x3F4);
        TOUCH_MEM(D_actor_403100_80155808);
        state = 8;
        TOUCH_REG(state);
        D_actor_403100_80155808->field_5DE = 0xE;
        D_actor_403100_80155808->field_5DA = 2;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5E2 = state;
        Mc_SaveData.at4.loc.view           = 0xC;
        *(s32*)(u32)&coords->coord.t[0]    = -0x44C;
        *(s32*)(u32)&coords->coord.t[2]    = 0x1770;
        *(s32*)(u32)&coords->coord.t[1]    = 0;
        D_actor_403100_80155808->field_82  = 0xC00;
        D_actor_403100_80155808->field_80  = 0;
        D_actor_403100_80155808->field_84  = 0;
        D_actor_403100_80155808->field_604 = 0;
        D_actor_403100_80155808->field_608 = 0;
        D_actor_403100_80155808->field_A0  = 0;
        D_actor_403100_80155808->field_A2  = 0;
        D_actor_403100_80155808->field_A4  = 0;
        part->coord.t[0]                   = -0x877;
        D_actor_403100_80155808->field_5FA = state;
    }
}
void func_actor_403100_801395EC(Task* arg0)
{
    Actor403100Entry* entry;
    Actor403100Entry* entries;
    GpObj*            obj;
    s32               x;
    s32               i;
    u16               frame;
    TmdObject*        model;
    GpCoord*          part;
    GpCoord*          coords;

    model                              = arg0->extra.tmd;
    coords                             = model->coords;
    part                               = coords + 6;
    D_actor_403100_80155808->field_604 = (u16)D_actor_403100_80155808->field_604 + ((s32)(-0x2E0 - D_actor_403100_80155808->field_604) >> 3);
    D_actor_403100_80155808->field_608 = (u16)D_actor_403100_80155808->field_608 + ((s32)(0x10 - D_actor_403100_80155808->field_608) >> 3);
    D_actor_403100_80155808->field_A0  = (u16)D_actor_403100_80155808->field_A0 + ((s32)(-0x150 - D_actor_403100_80155808->field_A0) >> 3);
    D_actor_403100_80155808->field_A2  = (u16)D_actor_403100_80155808->field_A2 + ((s32)(0x280 - D_actor_403100_80155808->field_A2) >> 3);
    D_actor_403100_80155808->field_A4  = (u16)D_actor_403100_80155808->field_A4 + ((s32)(0x140 - D_actor_403100_80155808->field_A4) >> 3);
    x                                  = *(s32*)(u32)&part->coord.t[0];
    *(s32*)(u32)&part->coord.t[0]      = (s32)(x + ((s32)(-0xBE0 - x) >> 3));
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_82  = 0xC00;
    D_actor_403100_80155808->field_84  = 0;
    func_actor_403100_80132528(arg0);
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    i                                  = 0;
    if (((s32)(frame << 0x10) >> 0x10) >= 0x1F) {
        entries                                   = D_actor_403100_80155814;
        obj                                       = &entries->obj;
        entry                                     = entries;
        Mc_SaveData.at4.loc.view                  = 0x18;
        *(s32*)(u32)&coords->coord.t[1]           = -0x1388;
        D_actor_403100_80155808->field_5E8        = 0;
        D_actor_403100_80155808->field_604        = 0;
        D_actor_403100_80155808->field_608        = 0;
        D_actor_403100_80155808->field_A0         = 0;
        D_actor_403100_80155808->field_A2         = 0;
        D_actor_403100_80155808->field_A4         = 0;
        *(s32*)(u32)&part->coord.t[0]             = -0x877;
        D_actor_403100_80155808->field_5DE        = 7;
        D_actor_403100_80155808->field_5DA        = 2;
        D_actor_403100_80155808->flags_634.h.high = 0x14;
        D_actor_403100_80155808->field_B2         = 0x200;
        D_actor_403100_80155808->field_5EC        = 0;
        D_actor_403100_80155808->field_5E2        = 0x10;
        D_actor_403100_80155808->field_61C        = 0;
        D_actor_403100_80155808->field_B0         = 0;
        D_actor_403100_80155808->field_B4         = 0;
        D_actor_403100_80155810                   = 0;
        D_actor_403100_80155808->field_5FA       += 1;
        *(s32*)(u32)&coords->coord.t[0]           = -0x44C;
        *(s32*)(u32)&coords->coord.t[2]           = 0x2710;
        *(s32*)(u32)&coords->coord.t[1]           = -0x1388;
        D_actor_403100_80155808->field_80         = 0;
        D_actor_403100_80155808->field_82         = 0xA00;
        D_actor_403100_80155808->field_84         = 0;
        do {
            if (entry->active != 0) {
                entry->active = 0;
                Gp_UnlinkObj(obj);
            }
            obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
            i  += 1;
            entry++;
        } while (i < 0x1C);
        SndEvt_EnqueueType7(0x401F0004, 1);
    }
}
void func_actor_403100_80139818(Task* arg0)
{
    SVECTOR          position;
    SVECTOR          velocity;
    Task*            playerTask;
    Task*            task;
    s16              deathFrame;
    s32              sound;
    s32              sound2;
    s32              sound3;
    s32              sound4;
    s32              sound5;
    s32              pan;
    s32              pan2;
    s32              pan3;
    s32              pan4;
    s32              pan5;
    u16              frame;
    s32              depth;
    s32              depth2;
    s32              depth3;
    s32              depth4;
    s32              depth5;
    GpCoord*         effectCoords;
    u32              configHi;
    Actor403100Work* work;
    PlayerStatus*    config;
    GpCoord*         part;
    GpCoord*         coords;

    playerTask = *Gp_ActorSlots;
    coords     = arg0->extra.tmd->coords;
    part       = coords + 6;
    __asm__("lui %0, %%hi(Player_Status)" : "=r"(configHi));
    __asm__("addiu %0, %1, %%lo(Player_Status)" : "=r"(config) : "r"(configHi));
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if ((u8)D_actor_403100_80155808->field_65F == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x400, 0, 0);
            D_actor_403100_80155808->field_65F = 0;
        }
    }
    D_actor_403100_80155808->field_60E = 0;
    D_actor_403100_80155808->field_610 = 0x3C;
    func_actor_403100_8013C008(D_actor_403100_80155808->field_60E, 0x3C);
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x33) {
        D_actor_403100_80155808->field_61C = 0;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        D_actor_403100_80155808->field_61C = 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x33) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 4);
        depth = gpGetObjDepth(arg0->extra.tmd->coords + 4);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x34) < 0x48U) {
        position.vy = -0x1F0;
        position.vz = 0x620;
        velocity.vy = -0x20;
        position.vx = 0;
        velocity.vx = 0;
        velocity.vz = 0xE0;
        func_actor_403100_80132064(arg0, &position, &velocity, 0);
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x7D) < 0xBU) {
        effectCoords = arg0->extra.tmd->coords;
        position.vy  = -0x140;
        position.vx  = 0;
        position.vz  = 0x400;
        Gp_SpawnEff(0x60070, effectCoords + 3, -0x3FFCB400, &position);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x64) {
        func_8010B2A0(0, 3);
        func_actor_403100_8013D1B8(1, 0x3F4);
        task = gameGetPtrSlot(3);
        Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 4), 0);
        if (config->hp <= 0) {
            sound2 = (((u16)((GpEnemy*)((Task*)playerTask)->spawnArg2)->placeKey >> 0xC) << 8) | 0x531D000B;
            pan2   = (s8)Gp_GetObjPan(playerTask->extra.tmd->coords + 1);
            depth2 = gpGetObjDepth(playerTask->extra.tmd->coords + 1);
            SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
            gGameSession->areaBgmCountdown    = 0x7F;
            work                              = D_actor_403100_80155808;
            work->pad_670[0]                  = 1;
            work->field_654                   = 0;
            gGameSession->suppressDeathChecks = 1;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x400, 0, 0);
            func_actor_403100_8013D1B8(6, 0x3FF);
        } else {
            sound3 = (((u16)((GpEnemy*)((Task*)playerTask)->spawnArg2)->placeKey >> 0xC) << 8) | 7;
            pan3   = (s8)Gp_GetObjPan(playerTask->extra.tmd->coords + 1);
            depth3 = gpGetObjDepth(playerTask->extra.tmd->coords + 1);
            SndEvt_EnqueueType6(sound3, (s32)pan3, (s8)(depth3 / 2));
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x80) {
        sound4 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F000D;
        pan4   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 4);
        depth4 = gpGetObjDepth(arg0->extra.tmd->coords + 4);
        SndEvt_EnqueueType6(sound4, (s32)pan4, (s8)(depth4 / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xC9) {
        sound5 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F000E;
        pan5   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 1);
        depth5 = gpGetObjDepth(arg0->extra.tmd->coords + 1);
        SndEvt_EnqueueType6(sound5, (s32)pan5, (s8)(depth5 / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    D_actor_403100_80155808->field_604 = -0x20;
    D_actor_403100_80155808->field_608 = 0x450;
    D_actor_403100_80155808->field_A0  = 0x290;
    D_actor_403100_80155808->field_A2  = 0x210;
    D_actor_403100_80155808->field_A4  = -0x160;
    *(s32*)(u32)&part->coord.t[0]      = -0xE27;
    *(s32*)(u32)&coords->coord.t[0]    = -0x44C;
    *(s32*)(u32)&coords->coord.t[1]    = -0x1388;
    *(s32*)(u32)&coords->coord.t[2]    = 0x2710;
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_82  = 0xA00;
    D_actor_403100_80155808->field_84  = 0;
    func_actor_403100_80132528(arg0);
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (Actor403100_TestFlags()) {
            func_actor_403100_8013D1B8(1, 0x3F4);
            D_actor_403100_80155808->field_5EC  = 0;
            D_actor_403100_80155808->field_61C  = 0;
            D_actor_403100_80155808->field_610  = 0x3C;
            D_actor_403100_80155808->field_5FA += 1;
        }
    } else {
        deathFrame                         = (u16)D_actor_403100_80155808->field_654 + 1;
        D_actor_403100_80155808->field_654 = deathFrame;
        if (deathFrame == 0x3C) {
            func_actor_403100_8013D2A0(0);
        }
        if (D_actor_403100_80155808->field_654 >= 0x79) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
}
void func_actor_403100_80139E80(Task* arg0)
{
    s16      angle;
    s32      y;
    s32      x;
    u16      velocityX;
    u16      rotationX;
    u16      rotationZ;
    u16      velocityZ;
    u16      rotationY;
    u16      frame;
    GpCoord* part;
    GpCoord* coords;

    coords                             = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_60E = (u16)D_actor_403100_80155808->field_60E - 1;
    angle                              = (u16)D_actor_403100_80155808->field_610 + 6;
    D_actor_403100_80155808->field_610 = angle;
    func_actor_403100_8013C008(D_actor_403100_80155808->field_60E, angle);
    part                               = coords + 6;
    y                                  = coords->coord.t[1];
    coords->coord.t[1]                 = (s32)(y + ((s32)-y >> 6));
    velocityX                          = (u16)D_actor_403100_80155808->field_604;
    velocityZ                          = (u16)D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_604 = velocityX + ((s32)(-0x1100 - (s16)(velocityX * 0x10)) >> 7);
    rotationX                          = (u16)D_actor_403100_80155808->field_A0;
    D_actor_403100_80155808->field_608 = velocityZ + ((s32)(0x4E00 - (s16)(velocityZ * 0x10)) >> 7);
    rotationY                          = (u16)D_actor_403100_80155808->field_A2;
    D_actor_403100_80155808->field_A0  = rotationX + ((s32)(0x2400 - (s16)(rotationX * 0x10)) >> 7);
    rotationZ                          = (u16)D_actor_403100_80155808->field_A4;
    D_actor_403100_80155808->field_A2  = rotationY + ((s32)(-0x1D00 - (s16)(rotationY * 0x10)) >> 7);
    D_actor_403100_80155808->field_A4  = rotationZ + ((s32)(0x2E00 - (s16)(rotationZ * 0x10)) >> 7);
    x                                  = part->coord.t[0];
    part->coord.t[0]                   = (s32)(x + ((s32)(-0x807 - x) >> 3));
    func_actor_403100_80132528(arg0);
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0x1F) {
        D_actor_403100_80155808->field_5E2              = 8;
        D_actor_403100_80155808->field_5DE              = 0xE;
        D_actor_403100_80155808->field_5DA              = 2;
        *(s16*)(u32)&D_actor_403100_80155808->field_5EC = 0;
        Mc_SaveData.at4.loc.view                        = 0xC;
        *(s32*)(u32)&coords->coord.t[0]                 = -0x44C;
        *(s32*)(u32)&coords->coord.t[2]                 = 0x1770;
        *(s32*)(u32)&coords->coord.t[1]                 = 0;
        D_actor_403100_80155808->field_82               = 0xC00;
        D_actor_403100_80155808->field_80               = 0;
        D_actor_403100_80155808->field_84               = 0;
        D_actor_403100_80155808->field_604              = 0;
        D_actor_403100_80155808->field_608              = 0;
        D_actor_403100_80155808->field_A0               = 0;
        D_actor_403100_80155808->field_A2               = 0;
        D_actor_403100_80155808->field_A4               = 0;
        *(s32*)(u32)&part->coord.t[0]                   = -0x877;
        D_actor_403100_80155808->field_5FA             += 1;
    }
}
void func_actor_403100_8013A064(Task* arg0)
{
    GameActor* actor;
    Task*      task;
    s16        state;
    s16        message;
    s32        sound;
    s32        pan;
    u16        frame;
    s32        depth;

    actor                              = (*Gp_ActorSlots)->work;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x3C) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F000F;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[8]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[8]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    D_actor_403100_80155808->field_80 = 0;
    D_actor_403100_80155808->field_82 = 0xC00;
    D_actor_403100_80155808->field_84 = 0;
    func_actor_403100_80132528(arg0);
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x44) {
        D_actor_403100_80155808->field_5FC  = 0x14;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 5;
        D_actor_403100_80155808->field_5DA  = 1;
        Mc_SaveData.at4.loc.view            = 0x17;
        D_actor_403100_80155808->field_5FA += 1;
        func_actor_403100_8013D0B8(-0x1BBC, -0xC80, -0x4B0, 0x400);
        task = gameGetPtrSlot(3);
        if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 5), 0) != 0) {
            gGameSession->suppressDeathChecks = 1;
            state                             = 7;
            if (D_actor_403100_8015580C->hp <= 0) {
                D_actor_403100_8015580C->hp = 0x3E8;
            }
            message                             = 0x3FF;
            D_actor_403100_80155808->pad_670[0] = 1;
            actor->field_956                    = 0xA;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->field_5EC = 0;
    }
}
void func_actor_403100_8013A254(void)
{
    Task*            actor;
    Actor403100Work* work;
    s32              sound;
    s32              sound2;
    s32              pan;
    s32              pan2;
    u16              frame;
    s32              depth;
    s32              depth2;

    actor                              = *Gp_ActorSlots;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xB) {
        if (D_actor_403100_80155808->regions.fields.field_63E == 0) {
            func_8017E250(1, 1);
            D_actor_403100_80155808->regions.fields.field_63E = 1;
        } else {
            func_8017E250(1, 2);
        }
        Gp_SpawnPadLerp(8, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 8;
        sound                              = (((u16)((GpEnemy*)((Task*)actor)->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0006;
        pan                                = (s8)Gp_GetObjPan(actor->extra.tmd->coords + 1);
        depth                              = gpGetObjDepth(actor->extra.tmd->coords + 1);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x1C) {
        sound2 = (((u16)((GpEnemy*)((Task*)actor)->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0007;
        pan2   = (s8)Gp_GetObjPan(actor->extra.tmd->coords + 1);
        depth2 = gpGetObjDepth(actor->extra.tmd->coords + 1);
        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    D_actor_403100_80155808->field_65F = 0;
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            if (D_actor_403100_8015580C->hp > 0) {
                Mc_SaveData.at4.loc.view = 6;
            }
            D_actor_403100_80155808->field_632             = 0;
            D_actor_403100_80155808->field_668.b.field_668 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work                                = D_actor_403100_80155808;
            work->pad_670[3]                    = 0;
            work->field_47C.radius              = 0x400;
            D_actor_403100_80155808->pad_670[1] = 0;
            D_actor_403100_80155808->field_5F8  = 1;
            D_actor_403100_80155808->field_5FA  = 0;
        }
    } else if ((s16)D_actor_403100_80155808->field_5EC >= 0x1E) {
        gGameSession->suppressDeathChecks = 0;
    }
}
void func_actor_403100_8013A4C8(Task* arg0)
{
    Actor403100Entry* entries;
    GpObj*            obj;
    s32               i;
    Actor403100Work*  work;

    i                                          = 0;
    entries                                    = D_actor_403100_80155814;
    obj                                        = &entries->obj;
    arg0->extra.tmd->otOffset                  = 0x1F;
    work                                       = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    (*(volatile s16*)&D_actor_403100_80155810) = 0;
    work->field_62C                            = 0x30;
    work->field_5F6                            = 0;
    work->flags_634.h.high                     = 0x1C;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 7;
    D_actor_403100_80155808->field_5DA  = 2;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013A5AC(Task* arg0)
{
    SVECTOR  offset;
    SVECTOR  velocity;
    s32      sound;
    s32      pan;
    s32      depth;
    GpCoord* effectCoord;

    if (D_actor_403100_80155810 == 1) {
        D_actor_403100_80155808->field_5FA = 3;
        return;
    }
    D_actor_403100_80155808->field_98   = -0x3110;
    D_actor_403100_80155808->field_5EC += 1;
    D_actor_403100_80155808->field_9A   = -0xC80;
    D_actor_403100_80155808->field_9C   = ((s32)(rsin((s16)D_actor_403100_80155808->field_5EC << 5) * 0x10) >> 6) + 0x6DB;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x33) {
        D_actor_403100_80155808->field_61C = 0;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        D_actor_403100_80155808->field_61C = 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x33) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x7C) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x34) < 0x48U) {
        offset.vy   = -0x1F0;
        offset.vz   = 0x620;
        velocity.vy = -0x20;
        offset.vx   = 0;
        velocity.vx = 0;
        velocity.vz = 0xE0;
        func_actor_403100_80132064(arg0, &offset, &velocity, 0);
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x7D) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_8013A81C(Task* arg0)
{
    SVECTOR  offset;
    s32      sound;
    s32      sound2;
    s32      pan;
    GpCoord* effectCoord;
    s32      pan2;
    u16      frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(gpGetObjDepth(&arg0->extra.tmd->coords[1]) / 2));
        D_actor_403100_80155808->field_664.b.field_666 = 1;
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x32) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013AA04(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;
    u16 frame;
    s32 depth;

    angle                              = (u16)D_actor_403100_80155808->field_82;
    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_82  = angle + ((s32)((-0x4000 - (angle * 0x10)) << 0x10) >> 0x16);
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((u32)((frame - 0x33) & 0xFFFF) < 0x16U) {
        func_actor_403100_8013C7B4(arg0);
    } else if ((func_actor_403100_80133928() << 0x10) != 0) {
        return;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x44) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0008;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[7]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[7]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if (D_actor_403100_80155808->field_668.flags != 0) {
        D_actor_403100_80155808->pad_670[3] = 1;
        D_actor_403100_80155808->pad_670[1] = 1;
        Gp_StateC08.field_6                |= 1;
        func_actor_403100_8013D1B8(3, 0x3F4);
        func_actor_403100_8013D0B8(D_actor_403100_80155808->field_90, D_actor_403100_80155808->field_92, (s16)((u16)D_actor_403100_80155808->field_94 + 0xBB8), 0x800);
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5EC             = 0;
        gGameSession->field_12C                        = 1;
        D_actor_403100_80155808->field_5FA            += 1;
        return;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5F8             = 1;
        D_actor_403100_80155808->field_5FA             = 0;
    }
}
void func_actor_403100_8013AC04(void)
{
    s32              state;
    s32              message;
    s32              sound;
    Task*            task;
    s32              finished;
    s32              pan;
    s32              depth;
    Task*            player;
    GameActor*       actor;
    u8               completed;
    Actor403100Work* work;

    player   = (Task*)*Gp_ActorSlots;
    actor    = (GameActor*)player->work;
    finished = 0;
    if ((s16)D_actor_403100_80155808->field_5EC == 0) {
        Gp_SpawnPadLerp(0xC, 0xFF, 0x80);
        sound = (((u16)((GpEnemy*)player->spawnArg2)->placeKey >> 0xC) << 8) | 7;
        pan   = (s8)Gp_GetObjPan(&player->extra.tmd->coords[1]);
        depth = gpGetObjDepth(&player->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    }
    work = D_actor_403100_80155808;
    if ((s16)work->field_5EC < 5) {
        func_actor_403100_8013D0B8(work->field_90, work->field_92, (s16)(work->field_94 + 0x3E8), 0x800);
    }
    if (((s16)D_actor_403100_80155808->field_5EC >= 6) || (D_actor_403100_80155808->field_94 >= 0x1B58)) {
        finished = 1;
    }
    D_actor_403100_80155808->field_5EC = (s16)((u16)D_actor_403100_80155808->field_5EC + 1);
    if ((completed = finished != 0)) {
        Mc_SaveData.at4.loc.view = 0x14;
        task                     = gameGetPtrSlot(3);
        if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 2), 0) != 0) {
            gGameSession->suppressDeathChecks   = 1;
            gGameSession->areaBgmCountdown      = 0x7F;
            D_actor_403100_80155808->pad_670[0] = 1U;
        }
        func_actor_403100_8013D0B8(-0x1928, -0xC7C, 0x29D6, 0x800);
        if ((u8)D_actor_403100_80155808->pad_670[0] != 0) {
            actor->field_956 = 0xA;
            state            = 7;
            message          = 0x3FF;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5FA = (u16)(D_actor_403100_80155808->field_5FA + 1);
    }
}
void func_actor_403100_8013AE28(void)
{
    Task* player;
    s32   sound;
    s32   sound2;
    s32   sound3;
    s32   pan;
    s32   pan2;
    s32   pan3;
    s32   depth;
    u16   frame;

    player                             = (Task*)*Gp_ActorSlots;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xA) {
        if (D_actor_403100_80155808->regions.fields.field_63C == 0) {
            func_8017E250(0, 1);
            D_actor_403100_80155808->regions.fields.field_63C = 1;
        } else {
            func_8017E250(0, 2);
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xB) {
        sound = (((u16)((GpEnemy*)player->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0006;
        pan   = (s8)Gp_GetObjPan(&player->extra.tmd->coords[1]);
        depth = gpGetObjDepth(&player->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x1C) {
        sound2 = (((u16)((GpEnemy*)player->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0007;
        pan2   = (s8)Gp_GetObjPan(&player->extra.tmd->coords[1]);
        depth  = gpGetObjDepth(&player->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x20) {
        if ((u8)D_actor_403100_80155808->pad_670[0] != 0) {
            sound3 = (((u16)((GpEnemy*)player->spawnArg2)->placeKey >> 0xC) << 8) | 0x531D000B;
            pan3   = (s8)Gp_GetObjPan(&player->extra.tmd->coords[1]);
            depth  = gpGetObjDepth(&player->extra.tmd->coords[1]);
            SndEvt_EnqueueType6(sound3, pan3, (s8)(depth / 2));
        }
    }
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            D_actor_403100_80155808->field_668.b.field_668 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            if (D_actor_403100_8015580C->hp > 0) {
                Mc_SaveData.at4.loc.view = 4;
            }
            gGameSession->field_12C             = 0;
            D_actor_403100_80155808->pad_670[3] = 0;
            D_actor_403100_80155808->pad_670[1] = 0;
            D_actor_403100_80155808->field_5F8  = 1;
            D_actor_403100_80155808->field_5FA  = 0;
        }
    } else if ((s16)D_actor_403100_80155808->field_5EC >= 0x32) {
        gGameSession->suppressDeathChecks = 0;
    }
}
void func_actor_403100_8013B128(Task* arg0)
{
    Actor403100Entry* entries;
    Actor403100Work*  work;
    Actor403100Work*  finalWork;
    GpObj*            obj;
    s32               sound;
    s32               i;
    s32               pan;
    s32               depth;
    GpCoord*          coords;
    TmdObject*        model;

    model                                  = arg0->extra.tmd;
    coords                                 = model->coords;
    gGameSession->field_12C                = 0;
    model->otOffset                        = 0;
    D_actor_403100_8015580C->reactionFlags = 0;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    D_actor_403100_8015580C->node.state.b.flags = 9;
    i                                           = 0;
    if (D_actor_403100_80155808->field_5D0 < 0) {
        arg0->state                        = 4;
        D_actor_403100_80155808->field_5F8 = 0;
        D_actor_403100_80155808->field_5FA = 0U;
        return;
    }
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    entries                 = D_actor_403100_80155814;
    obj                     = &entries->obj;
    D_actor_403100_80155810 = 0;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_622              = (s16)gGameSession->at4.loc.view;
    D_actor_403100_80155808->field_0.matrices.coord = coords->coord;
    D_actor_403100_80155808->savedRotation          = *(SVECTOR*)&D_actor_403100_80155808->field_80;
    Mc_SaveData.at4.loc.view                        = 0x18;
    work                                            = D_actor_403100_80155808;
    work->pad_660[1]                                = 1;
    work->field_5F6                                 = 5;
    work->field_5E2                                 = 0x10;
    work->field_5DE                                 = 0x10;
    work->field_5DA                                 = 2;
    work->field_61C                                 = 2;
    D_actor_403100_80155808->field_604              = 0;
    D_actor_403100_80155808->field_608              = 0;
    D_actor_403100_80155808->field_A0               = 0;
    D_actor_403100_80155808->field_A2               = 0;
    D_actor_403100_80155808->field_A4               = 0;
    coords->coord.t[0]                              = -0x44C;
    coords->coord.t[1]                              = -0x1388;
    coords->coord.t[2]                              = 0x2710;
    D_actor_403100_80155808->field_80               = 0;
    D_actor_403100_80155808->field_82               = 0xA00;
    D_actor_403100_80155808->field_84               = 0;
    gGameSession->hideHud                           = 1;
    Gp_MsgPlayerWeapon(0);
    sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F000B;
    pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
    depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
    SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    finalWork             = D_actor_403100_80155808;
    finalWork->field_5EC  = 0;
    finalWork->pad_670[3] = 1;
    finalWork->field_5FA  = (u16)(finalWork->field_5FA + 1);
}
void func_actor_403100_8013B3C4(Task* arg0)
{
    s32      sound;
    s32      pan;
    s32      depth;
    GpCoord* coords;

    coords                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_5EC += 1;
    Mc_SaveData.at4.loc.view            = 0x18;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x64) {
        Gp_LoadImages(&D_actor_403100_801555EC);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x10E) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0005;
        pan   = (s8)Gp_GetObjPan(&arg0->extra.tmd->coords[4]);
        depth = gpGetObjDepth(&arg0->extra.tmd->coords[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    }
    coords->coord.t[0]                = -0x44C;
    coords->coord.t[1]                = -0x1388;
    coords->coord.t[2]                = 0x2710;
    D_actor_403100_80155808->field_80 = 0;
    D_actor_403100_80155808->field_82 = 0xA00;
    D_actor_403100_80155808->field_84 = 0;
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->pad_670[3] = 0;
        Gp_LinkNode(&D_actor_403100_8015580C->node);
        D_actor_403100_8015580C->node.state.b.flags = 8;
        gGameSession->hideHud                       = 0;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        coords->coord                                 = D_actor_403100_80155808->field_0.matrices.coord;
        *(SVECTOR*)&D_actor_403100_80155808->field_80 = D_actor_403100_80155808->savedRotation;
        Mc_SaveData.at4.loc.view                      = (u8)D_actor_403100_80155808->field_622;
        D_actor_403100_80155808->field_5F8            = 1;
        D_actor_403100_80155808->field_5FA            = 0;
    }
}

/// The scratch-pad block of turning model part 3 toward a point: the matrices
/// the turn is built in, and the pitch and yaw toward the point.
typedef struct Actor403100AimScratch {
    MATRIX  mats[4];
    SVECTOR angles;
} Actor403100AimScratch;
STATIC_ASSERT_SIZEOF(Actor403100AimScratch, 0x88);

void func_actor_403100_8013B5E0(Task* arg0, s16 arg1)
{
    SVECTOR                headRotation, middleRotation, lowerRotation;
    MATRIX                 worldMatrix;
    VECTOR                 delta, local;
    Actor403100AimScratch* allocated;
    MATRIX*                matrices;
    SVECTOR*               angles;
    GpCoord*               coords;
    GpCoord*               head;
    GpCoord*               middle;
    GpCoord*               lower;
    GpCoord*               root;
    MATRIX*                transpose;
    MATRIX*                transpose2;
    MATRIX*                dest;
    s32                    sum;
    s32                    offsetY;
    s32                    mode3;
    u16                    copyValue;

    coords    = arg0->extra.tmd->coords;
    allocated = SCRATCH_HEAD(Actor403100AimScratch) - 1;
    __asm__("move %0,%1" : "=r"(matrices) : "r"(allocated));
    angles                             = &allocated->angles;
    *(s32*)&allocated->mats[0].m[0][0] = 0x1000;
    *(s32*)&matrices->m[0][2]          = 0;
    *(s32*)&matrices->m[1][1]          = 0x1000;
    *(s32*)&matrices->m[2][0]          = 0;
    matrices->m[2][2]                  = 0x1000;
    root                               = arg0->extra.tmd->coords;
    SCRATCH_HEAD(MATRIX)               = matrices;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &root[3].workm, &worldMatrix);
    delta.vx = D_actor_403100_80155808->field_98 - worldMatrix.t[0];
    offsetY  = worldMatrix.t[1] + 0x600;
    delta.vy = D_actor_403100_80155808->field_9A - offsetY;
    delta.vz = D_actor_403100_80155808->field_9C - worldMatrix.t[2];
    ApplyTransposeMatrixLV(&root->coord, &delta, &local);
    angles->vx = (ratan2(-local.vy, local.vz) << 20) >> 20;
    angles->vy = (ratan2(local.vx, local.vz) << 20) >> 20;
    angles->vz = 0;
    head       = &coords[3];
    middle     = &coords[2];
    lower      = &coords[1];
    mode3      = 3;
    SOFT_TOUCH_REG(mode3);
    if (arg1 == 0) {
        func_actor_403100_8013CEAC((u16*)angles, 8, 0x280, -0x2C0);
        func_actor_403100_8013CF60(angles, 8, 2, 1, 4);
        func_actor_403100_8013D06C();
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    } else if (arg1 == 1) {
        Gp_MtxToEuler(&coords[3].coord, &headRotation);
        D_actor_403100_80155808->field_B0 += ((s32)(((u16)headRotation.vx - (u16)D_actor_403100_80155808->field_B0) << 20) >> 23);
        D_actor_403100_80155808->field_B4 += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->field_B4) << 20) >> 23);
        func_actor_403100_8013CF60(angles, 8, 4, 1, 4);
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    } else if (arg1 == 2) {
        Gp_MtxToEuler(&coords[3].coord, &headRotation);
        Gp_MtxToEuler(&coords[2].coord, &middleRotation);
        Gp_MtxToEuler(&coords[1].coord, &lowerRotation);
        SOFT_USE_REG(mode3);
        sum                                = (u16)headRotation.vx + ((u16)middleRotation.vx + (u16)lowerRotation.vx);
        headRotation.vx                    = sum;
        headRotation.vy                    = (u16)headRotation.vy + ((u16)middleRotation.vy + (u16)lowerRotation.vy);
        headRotation.vz                    = (u16)headRotation.vz + ((u16)middleRotation.vz + (u16)lowerRotation.vz);
        D_actor_403100_80155808->field_B0 += ((s32)((sum - (u16)D_actor_403100_80155808->field_B0) << 20) >> 23);
        D_actor_403100_80155808->field_B2 += ((s32)(((u16)headRotation.vy - (u16)D_actor_403100_80155808->field_B2) << 20) >> 23);
        D_actor_403100_80155808->field_B4 += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->field_B4) << 20) >> 23);
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    } else if (arg1 == mode3) {
        func_actor_403100_8013CEAC((u16*)angles, 0x10, 0x280, -0x280);
        func_actor_403100_8013CF60(angles, 0x10, 4, 2, 8);
        func_actor_403100_8013D06C();
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    } else if (arg1 == 4) {
        Gp_MtxToEuler(&coords[3].coord, &headRotation);
        D_actor_403100_80155808->field_B0 += ((s32)(((u16)headRotation.vx - (u16)D_actor_403100_80155808->field_B0) << 20) >> 23);
        D_actor_403100_80155808->field_B4 += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->field_B4) << 20) >> 23);
        func_actor_403100_8013CF60(angles, 0x10, 4, 2, 8);
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    } else if (arg1 == 5) {
        func_actor_403100_8013CEAC((u16*)angles, 0x10, 0x280, -0x280);
        func_actor_403100_8013CF60(angles, D_actor_403100_80155808->field_65A, 4, 2, 8);
        func_actor_403100_8013D06C();
        RotMatrixZXY((SVECTOR*)&D_actor_403100_80155808->field_B0, matrices);
    }
    transpose = &matrices[3];
    TransposeMatrix(&middle->coord, transpose);
    transpose2 = &matrices[2];
    TransposeMatrix(&lower->coord, transpose2);
    MulMatrix(transpose, transpose2);
    MulMatrix(transpose, matrices);
    head->coord.m[0][0] = (u16)transpose->m[0][0];
    copyValue           = (u16)transpose->m[0][1];
    SOFT_USE_REG(copyValue);
    dest = &head->coord;
    SOFT_TOUCH_REG(dest);
    dest->m[0][1] = copyValue;
    dest->m[0][2] = (u16)transpose->m[0][2];
    dest->m[1][0] = (u16)transpose->m[1][0];
    dest->m[1][1] = (u16)transpose->m[1][1];
    dest->m[1][2] = (u16)transpose->m[1][2];
    dest->m[2][0] = (u16)transpose->m[2][0];
    dest->m[2][1] = (u16)transpose->m[2][1];
    dest->m[2][2] = (u16)transpose->m[2][2];
    SCRATCH_POP(Actor403100AimScratch);
    lower->flg  = 0;
    middle->flg = 0;
    head->flg   = 0;
}
/// Steps of the behaviour mode `func_actor_403100_8013DB48`, indexed by `field_5FA`.
const TaskFuncTable6 D_actor_403100_80131F84 = {
    {
        (TaskFunc)func_actor_403100_801375B8,
        func_actor_403100_801376D8,
        func_actor_403100_801379B4,
        (TaskFunc)func_actor_403100_80137CA8,
        func_actor_403100_80137DC4,
        (TaskFunc)func_actor_403100_8013F1D8,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DC18`, indexed by `field_5FA`.
const TaskFuncTable5 D_actor_403100_80131F9C = {
    {
        (TaskFunc)func_actor_403100_80137F4C,
        func_actor_403100_80138048,
        (TaskFunc)func_actor_403100_8013F230,
        (TaskFunc)func_actor_403100_8013F270,
        func_actor_403100_8013842C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DCAC`, indexed by `field_5FA`.
const TaskFuncTable9 D_actor_403100_80131FB0 = {
    {
        (TaskFunc)func_actor_403100_8013F2D8,
        (TaskFunc)func_actor_403100_8013F344,
        func_actor_403100_80138610,
        func_actor_403100_801386DC,
        func_actor_403100_80138790,
        func_actor_403100_80138844,
        (TaskFunc)func_actor_403100_80138AB4,
        (TaskFunc)func_actor_403100_80138C18,
        (TaskFunc)func_actor_403100_8013F3AC,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DD78`, indexed by `field_5FA`.
const TaskFuncTable11 D_actor_403100_80131FD4 = {
    {
        func_actor_403100_80138D08,
        func_actor_403100_80138DB0,
        func_actor_403100_8013F3EC,
        func_actor_403100_80138F88,
        func_actor_403100_8013922C,
        func_actor_403100_801395EC,
        func_actor_403100_80139818,
        func_actor_403100_80139E80,
        func_actor_403100_8013A064,
        (TaskFunc)func_actor_403100_8013A254,
        (TaskFunc)func_actor_403100_8013F488,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DE0C`, indexed by `field_5FA`.
const TaskFuncTable5 D_actor_403100_80132000 = {
    {
        func_actor_403100_8013A4C8,
        func_actor_403100_8013A5AC,
        (TaskFunc)func_actor_403100_8013F4E0,
        (TaskFunc)func_actor_403100_8013F520,
        func_actor_403100_8013A81C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DEA0`, indexed by `field_5FA`.
const TaskFuncTable4 D_actor_403100_80132014 = {
    {
        (TaskFunc)func_actor_403100_8013F588,
        func_actor_403100_8013AA04,
        (TaskFunc)func_actor_403100_8013AC04,
        (TaskFunc)func_actor_403100_8013AE28,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DFBC`, indexed by `field_5FA`.
const TaskFuncTable3 D_actor_403100_80132024 = {
    {
        (TaskFunc)func_actor_403100_8013F6B0,
        (TaskFunc)func_actor_403100_8013F6F4,
        (TaskFunc)func_actor_403100_8013F76C,
    },
};

/// Handlers `func_actor_403100_8013BA64` dispatches on `field_5F6`.
const TaskFuncTable6 D_actor_403100_80132030 = {
    {
        func_actor_403100_8013BB8C,
        func_actor_403100_8013BDE4,
        (TaskFunc)func_actor_403100_8013F7AC,
        (TaskFunc)func_actor_403100_8013F7B4,
        func_actor_403100_8013BEF0,
        (TaskFunc)func_actor_403100_8013F7BC,
    },
};

void func_actor_403100_8013BA64(Task* arg0)
{
    GpCoord*       coords   = arg0->extra.tmd->coords;
    TaskFuncTable6 handlers = D_actor_403100_80132030;

    D_actor_403100_80155808->field_602 = (u16)D_actor_403100_80155808->field_600;
    handlers.funcs[D_actor_403100_80155808->field_5F6](arg0);
    if (((Gp_GetViewIndex() & 0xFF) == 7) || ((Gp_GetViewIndex() & 0xFF) == 8)) {
        if ((s16)D_actor_403100_80155808->field_5F8 == 6) {
            if (D_actor_403100_80155808->field_628 == 2) {
                coords->coord.t[0] += (-4000 - coords->coord.t[0]) >> 4;
            } else {
                coords->coord.t[0] += (-2700 - coords->coord.t[0]) >> 4;
            }
        }
    } else {
        coords->coord.t[0] += (-1100 - coords->coord.t[0]) >> 3;
    }
}
void func_actor_403100_8013BB8C(Task* arg0)
{
    s16                 mode;
    s32                 delta;
    s32                 delta2;
    s32                 delta3;
    s32                 sound;
    s32                 pan;
    s32                 depth;
    TmdObject*          obj;
    register TmdObject* original asm("v1");
    GpCoord*            coords;

    original = arg0->extra.tmd;
    SOFT_TOUCH_REG(original);
    obj    = original;
    coords = obj->coords;
    mode   = (u16)D_actor_403100_80155808->field_628 - 1;
    switch (mode) {
        case 0:
        case 4:
            obj->otOffset = 0;
            delta         = D_actor_403100_80155808->field_94 - coords->coord.t[2];
            if (delta > 4864) {
                coords->coord.t[2]                += D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else if (delta < -4864) {
                coords->coord.t[2]                -= D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->field_5F6++;
            }
            break;
        case 1:
        case 5:
            delta2 = 1300 - coords->coord.t[2];
            if (delta2 > 48) {
                coords->coord.t[2]                += D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else if (delta2 < -48) {
                coords->coord.t[2]                -= D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->field_5F6++;
            }
            break;
        case 2:
        case 3:
            obj->otOffset = 0;
            delta3        = D_actor_403100_80155808->field_94 - coords->coord.t[2];
            if (delta3 > 640) {
                coords->coord.t[2]                += D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else if (delta3 < -640) {
                coords->coord.t[2]                -= D_actor_403100_80155808->field_62C;
                D_actor_403100_80155808->field_600 = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->field_5F6++;
            }
            break;
    }
    if (D_actor_403100_80155808->field_600 == 0) {
        if (D_actor_403100_80155808->field_602 != 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->field_5FE = 0x1E;
            sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
            pan                                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 1);
            depth                              = gpGetObjDepth(arg0->extra.tmd->coords + 1);
            SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        }
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->field_600) << 13) >> 16);
}
void func_actor_403100_8013BDE4(Task* arg0)
{
    s16      angle;
    s32      sound;
    s32      pan;
    s32      depth;
    GpCoord* coords;

    coords = arg0->extra.tmd->coords;
    if (D_actor_403100_80155808->field_600 != 0) {
        angle                              = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
        D_actor_403100_80155808->field_600 = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->field_5FE = 0x1E;
            sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
            pan                                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 1);
            depth                              = gpGetObjDepth(arg0->extra.tmd->coords + 1);
            SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->field_5F6 = 0;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->field_600) << 13) >> 16);
}
void func_actor_403100_8013BEF0(Task* arg0)
{
    s16      angle;
    s32      sound;
    s32      pan;
    s32      depth;
    GpCoord* coords;

    coords = arg0->extra.tmd->coords;
    if (D_actor_403100_80155808->field_600 != 0) {
        angle                              = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
        D_actor_403100_80155808->field_600 = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->field_5FE = 0x1E;
            sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x401F0001;
            pan                                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords + 1);
            depth                              = gpGetObjDepth(arg0->extra.tmd->coords + 1);
            SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->field_5F6++;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->field_600) << 13) >> 16);
}
void func_actor_403100_8013C008(s16 arg0, s16 arg1)
{
    POLY_FT4*             poly;
    Actor403100QuadEntry* entry;
    s32                   i;
    u16                   clut;

    for (i = 0; i < 2; i++) {
        entry          = &D_actor_403100_801557E0[i];
        poly           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setPolyFT4(poly);
        poly->tpage = entry->tpage;
        SOFT_TOUCH_REG(poly);
        clut = entry->clut;
        setShadeTex(poly, 1);
        poly->clut = clut;
        poly->u0   = entry->u;
        poly->v0   = entry->v;
        poly->u1   = entry->u + (u8)entry->w;
        poly->v1   = entry->v;
        poly->u2   = entry->u;
        poly->v2   = entry->v + (u8)entry->h;
        poly->u3   = entry->u + (u8)entry->w;
        poly->v3   = entry->v + (u8)entry->h;
        poly->x0   = entry->x + arg0;
        poly->y0   = entry->y + arg1;
        poly->x1   = arg0 + (entry->x + entry->w);
        poly->y1   = entry->y + arg1;
        poly->x2   = entry->x + arg0;
        poly->y2   = arg1 + (entry->y + entry->h);
        poly->x3   = arg0 + (entry->x + entry->w);
        poly->y3   = arg1 + (entry->y + entry->h);
        addPrim((u32*)((((u32)(entry->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
}
void func_actor_403100_8013C214(Task* arg0)
{
    Task*            playerTask;
    GpCoord*         soundCoords;
    Task*            task;
    s16              next;
    s16              next2;
    s32              sound;
    s32              randomSound;
    s32              phase;
    s32              x1;
    s32              nextX1;
    s32              x2;
    s32              nextX2;
    s32              x3;
    s32              nextX3;
    s32              x4;
    s32              nextX4;
    s32              sound2;
    s32              pan;
    s32              pan2;
    s32              depth;
    u32              random;
    s32              depth2;
    GpCoord*         coords;
    Actor403100Work* work;
    s32              sound3, pan3;
    s32              depth3;

    playerTask = *Gp_ActorSlots;
    coords     = arg0->extra.tmd->coords + 6;
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (D_actor_403100_80155808->field_638 != 0) {
            next                               = (u16)D_actor_403100_80155808->field_638 - 1;
            D_actor_403100_80155808->field_638 = next;
            if (next == 1) {
                D_actor_403100_80155808->pad_660[0] = 1;
            }
        } else {
            D_actor_403100_80155808->field_5EE = (u16)D_actor_403100_80155808->field_5EE + 1;
            next2                              = (u16)D_actor_403100_80155808->field_63A + 1;
            D_actor_403100_80155808->field_63A = next2;
            if (next2 >= 0xB4) {
                D_actor_403100_80155808->field_63A = 0;
                task                               = gameGetPtrSlot(3);
                if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 3), 0) != 0) {
                    sound = (((u16)((GpEnemy*)((Task*)playerTask)->spawnArg2)->placeKey >> 0xC) << 8) | 0x531D000B;
                    pan   = (s8)Gp_GetObjPan(playerTask->extra.tmd->coords + 1);
                    depth = gpGetObjDepth(playerTask->extra.tmd->coords + 1);
                    SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
                    gGameSession->areaBgmCountdown    = 0x7F;
                    work                              = D_actor_403100_80155808;
                    work->pad_670[0]                  = 1;
                    work->field_654                   = 0;
                    gGameSession->suppressDeathChecks = 1;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x400, 0, 0);
                    func_actor_403100_8013D1B8(6, 0x3FF);
                }
                if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
                    Gp_SpawnPadLerp(0xA, 0xC0U, 0x20U);
                    random      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = random;
                    randomSound = (random >> 0x10) & 3;
                    if (randomSound == 0) {
                        soundCoords = playerTask->extra.tmd->coords + 1;
                        sound2      = (((u16)((GpEnemy*)((Task*)playerTask)->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                        pan2        = (s8)Gp_GetObjPan(soundCoords);
                        depth2      = gpGetObjDepth(playerTask->extra.tmd->coords + 1);
                        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
                    } else if (randomSound == 1) {
                        soundCoords = playerTask->extra.tmd->coords + 1;
                        sound3      = (((u16)((GpEnemy*)((Task*)playerTask)->spawnArg2)->placeKey >> 0xC) << 8) | 7;
                        pan3        = (s8)Gp_GetObjPan(soundCoords);
                        depth3      = gpGetObjDepth(playerTask->extra.tmd->coords + 1);
                        SndEvt_EnqueueType6(sound3, (s32)pan3, (s8)(depth3 / 2));
                    }
                }
            }
            phase = ((u16)D_actor_403100_80155808->field_5EE >> 5) & 3;
            if (phase == 0) {
                D_actor_403100_80155808->field_664.b.field_667 = 0;
                D_actor_403100_80155808->field_604             = (u16)D_actor_403100_80155808->field_604 + ((s32)(0xD0 - D_actor_403100_80155808->field_604) >> 1);
                D_actor_403100_80155808->field_608             = (u16)D_actor_403100_80155808->field_608 + ((s32)(-0x350 - D_actor_403100_80155808->field_608) >> 1);
                D_actor_403100_80155808->field_A0              = (u16)D_actor_403100_80155808->field_A0 + ((s32)(-0x110 - D_actor_403100_80155808->field_A0) >> 1);
                D_actor_403100_80155808->field_A2              = (u16)D_actor_403100_80155808->field_A2 + ((s32)(0x290 - D_actor_403100_80155808->field_A2) >> 1);
                D_actor_403100_80155808->field_A4              = (u16)D_actor_403100_80155808->field_A4 + ((s32)(0x60 - D_actor_403100_80155808->field_A4) >> 1);
                x1                                             = coords->coord.t[0];
                nextX1                                         = x1 + ((s32)(-0xB80 - x1) >> 3);
                coords->coord.t[0]                             = nextX1;
                if (nextX1 >= -0xBD0) {
                    D_actor_403100_80155808->field_664.b.field_667 = 1;
                    return;
                }
            } else if (phase == 1) {
                D_actor_403100_80155808->field_664.b.field_667 = 0;
                D_actor_403100_80155808->field_604             = (u16)D_actor_403100_80155808->field_604 + ((s32)(0x30 - D_actor_403100_80155808->field_604) >> 2);
                D_actor_403100_80155808->field_608             = (u16)D_actor_403100_80155808->field_608 + ((s32)(-0xD0 - D_actor_403100_80155808->field_608) >> 2);
                D_actor_403100_80155808->field_A0              = (u16)D_actor_403100_80155808->field_A0 + ((s32)(-0x150 - D_actor_403100_80155808->field_A0) >> 2);
                D_actor_403100_80155808->field_A2              = (u16)D_actor_403100_80155808->field_A2 + ((s32)(0x270 - D_actor_403100_80155808->field_A2) >> 2);
                D_actor_403100_80155808->field_A4              = (u16)D_actor_403100_80155808->field_A4 + ((s32)(-0xA0 - D_actor_403100_80155808->field_A4) >> 2);
                x2                                             = coords->coord.t[0];
                nextX2                                         = x2 + ((s32)(-0x1180 - x2) >> 2);
                coords->coord.t[0]                             = nextX2;
                if (nextX2 < -0x111F) {
                    D_actor_403100_80155808->field_664.b.field_667 = 1;
                }
            } else if (phase == 2) {
                D_actor_403100_80155808->field_664.b.field_667 = 0;
                D_actor_403100_80155808->field_604             = (u16)D_actor_403100_80155808->field_604 + ((s32)(0xD0 - D_actor_403100_80155808->field_604) >> 2);
                D_actor_403100_80155808->field_608             = (u16)D_actor_403100_80155808->field_608 + ((s32)(-0x350 - D_actor_403100_80155808->field_608) >> 2);
                D_actor_403100_80155808->field_A0              = (u16)D_actor_403100_80155808->field_A0 + ((s32)(-0x110 - D_actor_403100_80155808->field_A0) >> 2);
                D_actor_403100_80155808->field_A2              = (u16)D_actor_403100_80155808->field_A2 + ((s32)(0x290 - D_actor_403100_80155808->field_A2) >> 2);
                D_actor_403100_80155808->field_A4              = (u16)D_actor_403100_80155808->field_A4 + ((s32)(0x60 - D_actor_403100_80155808->field_A4) >> 2);
                x3                                             = coords->coord.t[0];
                nextX3                                         = x3 + ((s32)(-0xB80 - x3) >> 2);
                coords->coord.t[0]                             = nextX3;
                if (nextX3 >= -0xBD0) {
                    D_actor_403100_80155808->field_664.b.field_667 = 1;
                    return;
                }
            } else if (phase == 3) {
                D_actor_403100_80155808->field_664.b.field_667 = 0;
                D_actor_403100_80155808->field_604             = (u16)D_actor_403100_80155808->field_604 + ((s32)(0x30 - D_actor_403100_80155808->field_604) >> 1);
                D_actor_403100_80155808->field_608             = (u16)D_actor_403100_80155808->field_608 + ((s32)(-0xD0 - D_actor_403100_80155808->field_608) >> 1);
                D_actor_403100_80155808->field_A0              = (u16)D_actor_403100_80155808->field_A0 + ((s32)(-0x150 - D_actor_403100_80155808->field_A0) >> 1);
                D_actor_403100_80155808->field_A2              = (u16)D_actor_403100_80155808->field_A2 + ((s32)(0x270 - D_actor_403100_80155808->field_A2) >> 1);
                D_actor_403100_80155808->field_A4              = (u16)D_actor_403100_80155808->field_A4 + ((s32)(-0xA0 - D_actor_403100_80155808->field_A4) >> 1);
                x4                                             = coords->coord.t[0];
                nextX4                                         = x4 + ((s32)(-0x1180 - x4) >> 2);
                coords->coord.t[0]                             = nextX4;
                if (nextX4 < -0x111F) {
                    D_actor_403100_80155808->field_664.b.field_667 = 1;
                }
            }
        }
    }
}
static inline s32 Actor403100CoordToViewInline(GpCoord* coord, SVECTOR* pos, GpCoord* view)
{
    SVECTOR  local;
    VECTOR   result;
    s32      flag;
    GpCoord* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == view) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

static inline void Actor403100ResetStateInline(s16 anim, s16 angle, s16 frame)
{
    D_actor_403100_80155808->field_5FC = frame;
    D_actor_403100_80155808->field_5E2 = angle;
    D_actor_403100_80155808->field_5DE = anim;
    D_actor_403100_80155808->field_5DA = 1;
}

void func_actor_403100_8013C7B4(Task* arg0)
{
    SVECTOR  pos0, pos1, delta;
    GpCoord* playerCoord;
    GpCoord* joint;
    s16      dx0;
    s16      dx1;
    s16      dz0;
    s16      dz1;
    u16      savedAngle;
    GpCoord* coords;
    GpCoord* second;
    GpCoord* walker;
    GpCoord* view;

    playerCoord = (*Gp_ActorSlots)->extra.tmd->coords;
    savedAngle  = (u16)D_actor_403100_80155808->field_5E2;
    coords      = arg0->extra.tmd->coords;
    Actor403100ResetStateInline(D_actor_403100_80155808->field_5DE, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC();
    func_actor_403100_801328DC(arg0);
    view              = &gGfxViewCoord;
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(view);
    SOFT_TOUCH_REG_USE(view, arg0);
    second        = coords + 7;
    joint         = coords + 8;
    coords[8].flg = 0;
    Gp_UpdateCoord(joint);
    pos0.vx = 0x160;
    pos0.vy = 0x148;
    pos0.vz = 0x2C0;
    Actor403100CoordToViewInline(joint, &pos0, view);
    dx0      = (u16)playerCoord->coord.t[0] - (u16)pos0.vx;
    delta.vx = dx0;
    delta.vy = (u16)playerCoord->coord.t[1] - ((u16)pos0.vy + 0x352);
    dz0      = (u16)playerCoord->coord.t[2] - (u16)pos0.vz;
    delta.vz = dz0;
    if ((SquareRoot0((dx0 * dx0) + (dz0 * dz0)) < 0x401) && ((u32)(((u16)delta.vy + 0x351) & 0xFFFF) < 0x6A3U)) {
        D_actor_403100_80155808->field_668.b.field_668 = 1;
    }
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;
        walker          = second;
        pos1.vx         = 0x160;
        pos1.vy         = 0x148;
        pos1.vz         = 0x180;
        local.vx        = 0x160;
        local.vy        = 0x148;
        local.vz        = 0x180;
        while (1) {
            if (walker->sub == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->sub;
                continue;
            }
            pos1.vx = local.vx;
            pos1.vy = local.vy;
            pos1.vz = local.vz;
            break;
        }
    }
    dx1      = (u16)playerCoord->coord.t[0] - (u16)pos1.vx;
    delta.vx = dx1;
    delta.vy = (u16)playerCoord->coord.t[1] - ((u16)pos1.vy + 0x352);
    dz1      = (u16)playerCoord->coord.t[2] - (u16)pos1.vz;
    delta.vz = dz1;
    if ((SquareRoot0((dx1 * dx1) + (dz1 * dz1)) < 0x401) && ((u32)(((u16)delta.vy + 0x351) & 0xFFFF) < 0x6A3U)) {
        D_actor_403100_80155808->field_668.b.field_669 = 1;
    }
    D_actor_403100_80155808->field_5E2 = -(s16)savedAngle;
    Actor403100ResetStateInline(D_actor_403100_80155808->field_5DE, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5E2 = (s16)savedAngle;
    Actor403100ResetStateInline(D_actor_403100_80155808->field_5DE, D_actor_403100_80155808->field_5E2, 0);
}
void func_actor_403100_8013CBE0(Task* arg0)
{
    s16 next;
    s16 next2;
    s32 sound;
    s32 soundId;
    s32 pan;
    u32 random;
    s32 depth;
    u8  request;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_665;
    switch (state) {
        case 0:
            D_actor_403100_80155808->field_5E8 = (s16)((u16)D_actor_403100_80155808->field_5E8 + ((s32) - (D_actor_403100_80155808->field_5E8 * 0x10) >> 7));
            return;
        case 1:
            request = D_actor_403100_80155808->field_66F;
            if (request == state) {
                soundId = 0x401F0009;
                goto play_sound;
            }
            if (request == 2) {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if ((random >> 16) & 1) {
                    soundId = 0x401F0000;
                    TOUCH_REG(soundId);
                    soundId |= 2;
                } else {
                    soundId = 0x401F0000;
                    TOUCH_REG(soundId);
                    soundId |= 5;
                }
            play_sound:
                sound = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | soundId;
                pan   = (s8)((s32 (*)(GpCoord*, s32))Gp_GetObjPan)(arg0->extra.tmd->coords + 4, soundId);
                depth = gpGetObjDepth(arg0->extra.tmd->coords + 4);
                SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
            }
            D_actor_403100_80155808->field_66F             = 0U;
            D_actor_403100_80155808->field_664.b.field_665 = (u8)(D_actor_403100_80155808->field_664.b.field_665 + 1);
            return;
        case 2:
            next                               = (u16)D_actor_403100_80155808->field_5E8 + ((s32)(-0x2200 - (D_actor_403100_80155808->field_5E8 * 0x10)) >> 7);
            D_actor_403100_80155808->field_5E8 = next;
            if (next < -0x1FF) {
                D_actor_403100_80155808->field_664.b.field_665 = (u8)(D_actor_403100_80155808->field_664.b.field_665 + 1);
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5E8 + 0xC;
            D_actor_403100_80155808->field_5E8 = next2;
            if ((next2 << 16) >= 0) {
                D_actor_403100_80155808->field_664.b.field_665 = 0U;
            }
            break;
    }
}
void func_actor_403100_8013CDC0(void)
{
    s16 next;
    s16 next2;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_666;
    switch (state) { /* irregular */
        case 0:
            D_actor_403100_80155808->field_5EA =
                (u16)D_actor_403100_80155808->field_5EA +
                ((s32) - (D_actor_403100_80155808->field_5EA * 0x10) >> 7);
            return;
        case 1:
            D_actor_403100_80155808->field_664.b.field_666 = 2;
            return;
        case 2:
            next = (u16)D_actor_403100_80155808->field_5EA +
                   ((s32)(0x1E00 - (D_actor_403100_80155808->field_5EA * 0x10)) >> 7);
            D_actor_403100_80155808->field_5EA = next;
            if (next >= 0x1C0) {
                D_actor_403100_80155808->field_664.b.field_666 =
                    D_actor_403100_80155808->field_664.b.field_666 + 1;
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5EA - 0xC;
            D_actor_403100_80155808->field_5EA = next2;
            if ((next2 << 0x10) <= 0) {
                D_actor_403100_80155808->field_664.b.field_666 = 0;
            }
            break;
    }
}

void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    s16 facing;
    s16 target;
    s16 angle;
    u16 targetU;
    u16 facingU;

    angle = *arg0 - 0x140;
    *arg0 = angle;
    if ((angle < (s16)arg2) && (arg3 < (s16)angle)) {
        target  = D_actor_403100_80155808->field_B0;
        targetU = (u16)D_actor_403100_80155808->field_B0;
        if ((u32)(((s16)angle - target) + 0x20) >= 0x41U) {
            if (target < (s16)angle) {
                D_actor_403100_80155808->field_B0 = (s16)(targetU + arg1);
                return;
            }
            D_actor_403100_80155808->field_B0 = (s16)(targetU - arg1);
        }
    } else {
        facing  = D_actor_403100_80155808->field_B0;
        facingU = (u16)D_actor_403100_80155808->field_B0;
        if (facing >= 0x21) {
            D_actor_403100_80155808->field_B0 = (s16)(facingU - 0x18);
            return;
        }
        if (facing < -0x20) {
            D_actor_403100_80155808->field_B0 = (s16)(facingU + 0x18);
        }
    }
}
void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    s16 facing;
    s16 target;
    u16 yaw;
    u16 targetU;
    u16 facingU;

    yaw = (u16)arg0->vy;
    if ((u32)((yaw + 0x27F) & 0xFFFF) < 0x4FFU) {
        target  = D_actor_403100_80155808->field_B2;
        targetU = (u16)D_actor_403100_80155808->field_B2;
        if ((u32)(((s16)yaw - target) + 0x20) >= 0x41U) {
            if (target < (s16)yaw) {
                D_actor_403100_80155808->field_B2 = targetU + arg1;
                D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg2;
                return;
            }
            D_actor_403100_80155808->field_B2 = targetU - arg1;
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg2;
            return;
        }
        facing  = (s16)D_actor_403100_80155808->field_82;
        facingU = D_actor_403100_80155808->field_82;
        if (facing < target) {
            D_actor_403100_80155808->field_82 = facingU + arg3;
            return;
        }
        if (target < facing) {
            D_actor_403100_80155808->field_82 = facingU - arg3;
        }
    } else {
        if ((s16)yaw >= 0x281) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg4;
        }
        if (arg0->vy < -0x280) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg4;
        }
    }
}

void func_actor_403100_8013D06C(void)
{
    if (D_actor_403100_80155808->field_B4 >= 0x11) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 - 8;
    }
    if (D_actor_403100_80155808->field_B4 < -0x10) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 + 8;
    }
}

void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpXformArg msg;

    msg.pos.vx = arg0;
    msg.pos.vy = arg1;
    msg.pos.vz = arg2;
    msg.rot.vx = 0;
    msg.rot.vy = arg3;
    msg.rot.vz = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&msg, 0);
}
void func_actor_403100_8013D11C(Task* arg0)
{
    GpCoord*      coords;
    GpCoord64*    slot;
    GpPointLight* light;
    s16           value;
    u32           random;

    coords                      = arg0->extra.tmd->coords;
    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 8;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    random                      = Gp_LcgState * 5 + 0x71357911;
    light->outer                = 0x3000;
    value                       = ((random >> 16) & 0x700) + 0x800;
    light->head.r               = value;
    light->head.g               = value >> 3;
    light->head.b               = value >> 4;
    coords                     += 3;
    light->head.u.at.local.t[0] = coords->coord.t[0];
    light->head.u.at.local.t[1] = coords->coord.t[1];
    light->head.u.at.local.t[2] = coords->coord.t[2];
    Gp_LcgState                 = random;
    slot->data.coord.flg        = 0;
}
void func_actor_403100_8013D1B8(s16 arg0, s16 arg1)
{
    GpAnimArg msg;

    msg.animBlock.ptr                  = &D_actor_403100_8015570C;
    msg.field_4                        = (s32)arg0;
    msg.field_8                        = 0;
    msg.field_C                        = 0;
    msg.field_10                       = 0;
    D_actor_403100_80155808->field_65D = (s8)arg0;
    if (arg1 == 0x3FF) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    } else if (arg1 == 0x3F4) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
    }
}
void func_actor_403100_8013D24C(void)
{
    s32 state;

    state = (s8)D_actor_403100_80155808->pad_66A[2];
    if ((state == 1) && !(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        D_actor_403100_80155808->field_664.b.field_665 = state;
        D_actor_403100_80155808->field_664.b.field_666 = (u8)state;
        D_actor_403100_80155808->field_66F             = state;
    }
}
void func_actor_403100_8013D2A0(s16 arg0)
{
    if (!(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        if (arg0 == 1) {
            D_actor_403100_80155808->field_66F = 2;
        }
        D_actor_403100_80155808->field_664.b.field_665 = 1;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
}

s32 func_actor_403100_8013D2F4(GpCoord* coord, MATRIX* matrix)
{
    MATRIX   result;
    MATRIX   parent;
    GpCoord* current;

    current = coord->sub;
    *matrix = coord->coord;
    while (1) {
        if (current == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            return 1;
        }
        parent = current->coord;
        MatrixNormal(&parent, &parent);
        gte_SetRotMatrix(&parent);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &result);
        *matrix = result;
        current = current->sub;
    }
}
/// Carries `pos`, a point local to `coord`, up the `sub` chain by applying
/// each level's matrix. If the chain reaches the view coordinate the
/// transformed point is written back to `pos` and 1 is returned; if it ends
/// first, `pos` is left untouched and 0 is returned.
s32 func_actor_403100_8013D460(GpCoord* coord, SVECTOR* pos)
{
    SVECTOR  local;
    VECTOR   result;
    s32      flag;
    GpCoord* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

void func_actor_403100_8013D564(Task* arg0, s32 arg1, u16* arg2)
{
    u16 value;

    switch (arg2[1]) {
        case 10:
            arg0->state                        = 3;
            D_actor_403100_80155808->field_5F8 = 0;
            D_actor_403100_80155808->field_5FA = 0;
            break;
        case 0xFFFF:
            D_actor_403100_80155808->field_5F8 = 0;
            D_actor_403100_80155808->field_5FA = 0;
            arg0->state                        = 2;
            break;
        default:
            value = arg2[1];
            if (value < 9U) {
                D_actor_403100_80155808->field_5F8 = value;
                D_actor_403100_80155808->field_5FA = 0;
                arg0->state                        = 1;
            }
            break;
    }
    D_actor_403100_80155808->field_5FA = 0;
}
void func_actor_403100_8013D5F4(void)
{
    D_actor_403100_80155808->field_65F = 1;
}

void func_actor_403100_8013D608(Task* arg0, s32 arg1, s32 arg2)
{
    u16        flags;
    TmdObject* object;

    object = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            object->flags = (object->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            object->flags = object->flags & 0xFF7B;
            return;
        case 2:
            object->flags                      = object->flags | 0x80;
            D_actor_403100_80155808->field_658 = arg2;
            flags                              = object->flags | 4;
            object->flags                      = flags;
            return;
        case 3:
            flags         = (object->flags & 0xFF7F) | 4;
            object->flags = flags;
            return;
    }
}
void func_actor_403100_8013D6B4(Task* arg0)
{
    GpCoord* coord;

    coord                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_606 = D_actor_403100_80155808->field_604;
    D_actor_403100_80155808->field_60A = D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_A8  = D_actor_403100_80155808->field_A0;
    D_actor_403100_80155808->field_AA  = D_actor_403100_80155808->field_A2;
    D_actor_403100_80155808->field_AC  = D_actor_403100_80155808->field_A4;
    D_actor_403100_80155808->field_5D4 = coord[6].coord.t[0];
    D_actor_403100_80155808->field_5F0 = D_actor_403100_80155808->field_5EE;
}
void func_actor_403100_8013D700(Task* arg0)
{
    GpCoord* coord;

    coord                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_604 = D_actor_403100_80155808->field_606;
    D_actor_403100_80155808->field_608 = D_actor_403100_80155808->field_60A;
    D_actor_403100_80155808->field_A0  = D_actor_403100_80155808->field_A8;
    D_actor_403100_80155808->field_A2  = D_actor_403100_80155808->field_AA;
    D_actor_403100_80155808->field_A4  = D_actor_403100_80155808->field_AC;
    coord[6].coord.t[0]                = D_actor_403100_80155808->field_5D4;
    D_actor_403100_80155808->field_5EE = D_actor_403100_80155808->field_5F0;
}
void func_actor_403100_8013D74C(Task* arg0)
{
    D_actor_403100_80155808->field_5EE             = 0;
    D_actor_403100_80155808->field_664.b.field_667 = 0;
    D_actor_403100_80155808->field_638             = 0;
    D_actor_403100_80155808->field_63A             = 0;
}
void func_actor_403100_8013D770(Task* arg0)
{
    SVECTOR    rotation;
    OverlayMat matrix;
    MATRIX*    dest;
    MATRIX*    mtx;
    GpCoord*   coords;
    GpCoord*   updated;

    coords               = arg0->extra.tmd->coords;
    dest                 = &coords[6].coord;
    coords[6].flg        = 0;
    mtx                  = &matrix.mat;
    matrix.ident.m00_m01 = 0x1000;
    matrix.ident.m02_m10 = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matrix.ident.m20_m21 = 0;
    mtx->m[2][2]         = 0x1000;
    Gp_MtxToEuler(dest, &rotation);
    USE_REG(mtx);
    rotation.vz += D_actor_403100_80155808->field_A4;
    rotation.vy += D_actor_403100_80155808->field_A2;
    rotation.vx += D_actor_403100_80155808->field_A0;
    RotMatrix(&rotation, &matrix.mat);
    dest->m[0][0] = matrix.mat.m[0][0];
    dest->m[0][1] = matrix.mat.m[0][1];
    dest->m[0][2] = matrix.mat.m[0][2];
    dest->m[1][0] = matrix.mat.m[1][0];
    dest->m[1][1] = matrix.mat.m[1][1];
    dest->m[1][2] = matrix.mat.m[1][2];
    dest->m[2][0] = matrix.mat.m[2][0];
    dest->m[2][1] = matrix.mat.m[2][1];
    updated       = coords + 6;
    USE_REG(updated);
    dest->m[2][2] = matrix.mat.m[2][2];
    Gp_UpdateCoord(updated);
}
void func_actor_403100_8013D88C(Task* arg0)
{
    Gp_UnlinkObj(&D_actor_403100_80155808->field_47C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_414);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_55C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_594);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

void func_actor_403100_8013D8F4(Task* arg0)
{
    D_actor_403100_80155808->field_658     = -1;
    D_actor_403100_80155808->field_618     = 0x1400;
    Gp_StateC08.field_6                    = Gp_StateC08.field_6 | 1;
    gGameSession->field_12C                = 0;
    D_actor_403100_8015580C->reactionFlags = 0;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    func_800E8614((s32)&D_80165FC0, 0);
    arg0->state                        = 1;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
    D_actor_403100_80155808->field_5F8 = 9;
    D_actor_403100_80155808->field_5FA = 0;
}
s32 func_actor_403100_8013D9C4(s16 x, s16 y, Actor403100RectEntry* entry)
{
    while (entry->value != -1) {
        if (x >= entry->x && entry->x + entry->w >= x &&
            y >= entry->y && entry->y + entry->h >= y) {
            return entry->value;
        }
        entry++;
    }
    return 0;
}
void func_actor_403100_8013DA6C(void)
{
    void (*fns[2])(void) = { (void (*)(void))func_actor_403100_8013712C, func_actor_403100_8013F12C };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}

void func_actor_403100_8013DAC4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131F60;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DB48(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_403100_80131F84;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
        func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
        func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
    }
}
void func_actor_403100_8013DC18(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80131F9C;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DCAC(Task* arg0)
{
    TaskFuncTable9 sp;

    sp = D_actor_403100_80131FB0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
    func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
}
void func_actor_403100_8013DD78(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_403100_80131FD4;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013DE0C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80132000;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DEA0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_403100_80132014;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013DF0C(void)
{
    void (*fns[2])(void) = { func_actor_403100_8013F610, func_actor_403100_8013F658 };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}
void func_actor_403100_8013DF64(void)
{
    void (*fns[2])(void) = { (void (*)(void))func_actor_403100_8013B128, (void (*)(void))func_actor_403100_8013B3C4 };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}
void func_actor_403100_8013DFBC(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80132024;
    func_actor_403100_8013D24C();
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013E02C(s16 arg0, s16 arg1, s16 arg2)
{
    D_actor_403100_80155808->field_5FC = arg2;
    D_actor_403100_80155808->field_5E2 = arg1;
    D_actor_403100_80155808->field_5DE = arg0;
    D_actor_403100_80155808->field_5DA = 1;
}

void func_actor_403100_8013E04C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E70;
    sp.funcs[task->state](task);
}

void func_actor_403100_8013E0A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E7C;
    sp.funcs[task->state](task);
}

/// Runs the handler for the task's current top-level state.
void func_actor_403100_8013E0FC(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_403100_80131F1C;
    sp.funcs[arg0->state](arg0);
}

void func_actor_403100_8013E16C(void)
{
}

void func_actor_403100_8013E174(void)
{
    s16 timer;

    if (Player_Status.hp > 0) {
        timer                              = (u16)D_actor_403100_80155808->field_5F4 - 1;
        D_actor_403100_80155808->field_5F4 = timer;
        if (timer < 0) {
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->field_5F2 = 2;
            return;
        }
        func_actor_403100_8013D1B8(5, 0x3F4);
    }
}
void func_actor_403100_8013E1E4(void)
{
    GpAnimArg sp;

    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        D_actor_403100_8015570C.sets[4] = ((Actor403100AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + Player_Status.weapon])->sets[7];
        sp.animBlock.ptr                = &D_actor_403100_8015570C;
        sp.field_8                      = 1;
        sp.field_C                      = 3;
        sp.field_10                     = 0;
        sp.field_4                      = 4;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&sp, 0);
        D_actor_403100_80155808->field_5F2 = 3;
    }
}
void func_actor_403100_8013E2BC(void)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        D_actor_403100_80155808->field_5F4             = 0;
        D_actor_403100_80155808->field_65D             = 0;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5F2             = 0;
    }
}
s32 func_actor_403100_8013E33C(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2)
{
    MATRIX   matrix;
    GpCoord* coord;

    coord = arg0->sub;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->sub;
    }
}
s32 func_actor_403100_8013E450(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2)
{
    MATRIX   matrix;
    MATRIX   normal;
    MATRIX   transposed;
    GpCoord* coord;

    coord = arg0->sub;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    matrix = coord->coord;
    while (1) {
        coord = coord->sub;
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            break;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    __asm__ volatile(
        "lhu $12, 0(%0);"
        "lhu $13, 6(%0);"
        "lhu $14, 12(%0);"
        "sh $12, 0(%1);"
        "sh $13, 2(%1);"
        "sh $14, 4(%1);"
        "lhu $12, 2(%0);"
        "lhu $13, 8(%0);"
        "lhu $14, 14(%0);"
        "sh $12, 6(%1);"
        "sh $13, 8(%1);"
        "sh $14, 10(%1);"
        "lhu $12, 4(%0);"
        "lhu $13, 10(%0);"
        "lhu $14, 16(%0);"
        "sh $12, 12(%1);"
        "sh $13, 14(%1);"
        "sh $14, 16(%1);"
        : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}
void func_actor_403100_8013E5FC(void)
{
    D_actor_403100_8015580C->recs       = 0;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5F8 += 1;
}
void func_actor_403100_8013E624(Task* arg0)
{
    u16 timer;

    timer                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = timer;
    if ((s16)timer == 0x12) {
        Gp_ReleaseStateF0Add(arg0, 0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x168) {
        arg0->state                        = 5;
        D_actor_403100_80155808->field_5F8 = 0;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013E6A0(Task* arg0)
{
    GpCoord* coord;

    coord               = arg0->extra.tmd->coords;
    arg0->killCountdown = 0x5A;
    coord->sub          = &gGfxViewCoord;
    coord->flg          = 0;
    arg0->state         = arg0->state + 1;
    func_actor_403100_8013E6F0(arg0);
}

void func_actor_403100_8013E6F0(Task* arg0)
{
    GpCoord* coord;
    u16      countdown;

    coord = arg0->extra.tmd->coords;
    if (Gp_StateF0.field_4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

void func_actor_403100_8013E784(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}

void func_actor_403100_8013E7C8(Task* arg0)
{
    GpCoord* coord;
    GpCoord* coord2;
    u16      countdown;

    coord2              = arg0->extra.tmd->coords;
    arg0->killCountdown = 0x5A;
    coord2->sub         = &gGfxViewCoord;
    coord2->flg         = 0;
    arg0->state        += 1;
    coord               = arg0->extra.tmd->coords;
    if (Gp_StateF0.field_4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

void func_actor_403100_8013E88C(Task* arg0)
{
    GpCoord* coord;
    u16      countdown;

    coord = arg0->extra.tmd->coords;
    if (Gp_StateF0.field_4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x20400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

void func_actor_403100_8013E920(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}
void func_actor_403100_8013E964(void)
{
}

void func_actor_403100_8013E96C(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EB0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013E9D8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EBC;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->field_61C);
}
void func_actor_403100_8013EA60(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131EC8;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013EAD4(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131ED8;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->field_61C);
}
void func_actor_403100_8013EB68(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EE4;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013EBC8(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131EF0;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013EC4C(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131F00;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013ECD0(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131F10;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013ED48(void)
{
}

void func_actor_403100_8013ED50(Task* arg0)
{
    GpCoord* coord;

    coord                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC();
    D_actor_403100_80155808->field_82  = 0;
    coord->coord.t[0]                  = -0x2710;
    coord->coord.t[1]                  = -0x258;
    coord->coord.t[2]                  = -0x2328;
    D_actor_403100_80155808->field_600 = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013EDDC(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x1E) {
        func_8017E128(1);
    }
}

void func_actor_403100_8013EE28(Task* arg0)
{
    GpCoord* coord;

    coord                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_61C = 2;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_82   = 0;
    coord->coord.t[0]                   = -0x1710;
    coord->coord.t[1]                   = 0;
    coord->coord.t[2]                   = -0x2846;
    D_actor_403100_80155808->field_618  = 0x1910;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013EEB0(void)
{
}

void func_actor_403100_8013EEB8(Task* arg0)
{
    GpCoord* coord;

    coord                               = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_5EC += 1;
    coord->coord.t[2]                  += 0x64;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x20) {
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
    func_actor_403100_801327CC();
}
void func_actor_403100_8013EF24(void)
{
}

void func_actor_403100_8013EF2C(void)
{
}

void func_actor_403100_8013EF34(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5EE = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013EF58(void)
{
}

void func_actor_403100_8013EF60(void)
{
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->field_5FC  = 0xB4;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 1;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_8013EFC0(void)
{
}

void func_actor_403100_8013EFC8(Task* arg0)
{
    TmdObject* obj;
    GpCoord*   coord;

    obj               = arg0->extra.tmd;
    obj->otOffset     = 0x10;
    coord             = obj->coords;
    coord->coord.t[0] = -0x49C;
    coord->coord.t[2] = 0x1130;
    coord->coord.t[1] = 0;

    D_actor_403100_80155808->field_82  = 0xC00;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0x14;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F034(Task* arg0)
{
    GpCoord* coord;

    coord             = arg0->extra.tmd->coords;
    coord->coord.t[0] = -0xAF0;
    coord->coord.t[1] = 0x300;
    coord->coord.t[2] = -0xE74;

    D_actor_403100_80155808->field_82  = 0x800;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0x18;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_600 = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5EE = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F0A8(Task* arg0)
{
    TmdObject* obj;

    obj                           = arg0->extra.tmd;
    obj->flags                   |= 0x80;
    D_actor_403100_8014762C.bp    = 0;
    D_actor_403100_8014762C.mp    = 0;
    D_actor_403100_8014762C.exp >>= 1;
    gGameSession->at4.loc.place   = 4;
    Gp_ReleaseStateF0Add(arg0, 0);
    arg0->state                        = 5;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
}
void func_actor_403100_8013F12C(void)
{
    u16 frame;

    if ((func_actor_403100_80133928() << 0x10) == 0) {
        frame                              = D_actor_403100_80155808->field_5EC + 1;
        D_actor_403100_80155808->field_5EC = frame;
        if ((s16)frame >= 0x1F) {
            D_actor_403100_80155808->field_5F8 = 1;
            D_actor_403100_80155808->field_5FA = 0;
        }
    }
}

void func_actor_403100_8013F18C(void)
{
    D_actor_403100_80155808->field_5FC = 4;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 1;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_62C = 0x20;
    D_actor_403100_80155808->field_5F6 = 0;
    D_actor_403100_80155808->field_61C = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F1D8(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013F230(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

void func_actor_403100_8013F270(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    D_actor_403100_80155808->field_5FC = 0x14;
    D_actor_403100_80155808->field_5E2 = 0x1C;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F2D8(void)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->field_61C             = 4;
        D_actor_403100_80155808->field_5F6             = 4;
        D_actor_403100_80155808->field_604             = 0;
        D_actor_403100_80155808->field_608             = 0;
        D_actor_403100_80155808->field_5FA             = D_actor_403100_80155808->field_5FA + 1;
    }
}

void func_actor_403100_8013F344(void)
{
    if (((func_actor_403100_80133928() << 0x10) == 0) &&
        (D_actor_403100_80155808->field_5F6 == 5)) {
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 4;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5DA = 2;
        D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
    }
}

void func_actor_403100_8013F3AC(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0x5A) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

void func_actor_403100_8013F3EC(Task* arg0)
{
    GpCoord* coord;

    coord                              = arg0->extra.tmd->coords;
    D_actor_403100_80155808->field_5F6 = 5;
    coord->coord.t[0]                  = -0x44C;
    coord->coord.t[1]                  = -0x1388;
    coord->coord.t[2]                  = 0x2710;
    D_actor_403100_80155808->field_82  = 0xA00;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0xB;
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_84  = 0;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_80132528(arg0);
    Mc_SaveData.at4.loc.view            = 0x18;
    D_actor_403100_80155808->field_60E  = 0;
    D_actor_403100_80155808->field_610  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013F488(void)
{
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013F4E0(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013F520(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    D_actor_403100_80155808->field_5FC  = 0x14;
    D_actor_403100_80155808->field_5E2  = 0x1C;
    D_actor_403100_80155808->field_5DE  = 3;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013F588(void)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->field_5F6             = 4;
        D_actor_403100_80155808->field_5E2             = 0xC;
        D_actor_403100_80155808->field_61C             = 2;
        D_actor_403100_80155808->field_5DE             = 8;
        D_actor_403100_80155808->field_5DA             = 2;
        D_actor_403100_80155808->field_5EC             = 0;
        D_actor_403100_80155808->field_82              = (u16)D_actor_403100_80155808->field_82 & 0xFFF;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->field_5FA            += 1;
    }
}
void func_actor_403100_8013F610(void)
{
    D_actor_403100_80155808->field_61C = 2;
    D_actor_403100_80155808->field_5F6 = 4;
    D_actor_403100_80155808->field_5FC = 8;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0xA;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F658(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013F6B0(void)
{
    D_actor_403100_80155808->field_61C  = 2;
    D_actor_403100_80155808->field_5F6  = 4;
    D_actor_403100_80155808->field_5FC  = 0xA;
    D_actor_403100_80155808->field_5E2  = 8;
    D_actor_403100_80155808->field_5DE  = 0xA;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013F6F4(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5FC  = 4;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5DE  = 1;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_8013F76C(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0x3D) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

void func_actor_403100_8013F7AC(void)
{
}

void func_actor_403100_8013F7B4(void)
{
}

void func_actor_403100_8013F7BC(void)
{
}
