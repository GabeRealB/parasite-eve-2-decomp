#include "common.h"
#include "main/stage.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"

#include "actors/actor.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "gameplay/areaplace.h"
#include "gameplay/pairsrc.h"
#include "gameplay/3FB8.h"

typedef struct Actor100400QuadWork {
    /* 0x00 */ GpEnemy* field_0;
    /* 0x04 */ SVECTOR  vertices[4];
    /* 0x24 */ u8       intensity;
} Actor100400QuadWork;

/// 0x64-byte work block of the marker task `Actor00400_Fn064B0` spawns from
/// `Actor00400_D16028[1]`: a display object and its two `GpRec18` slots, then
/// the view-space span between the marker's base and tip.
typedef struct Actor100400MarkerWork {
    /* 0x00 */ byte    pad_0[8];
    /* 0x08 */ GpObj   obj;
    /* 0x28 */ GpRec18 recs[2];
    /* 0x58 */ s16     field_58;
    /* 0x5A */ s16     field_5A;
    /* 0x5C */ s16     field_5C;
    /* 0x5E */ byte    pad_5E[2];
    /* 0x60 */ s32     field_60;
} Actor100400MarkerWork;
STATIC_ASSERT_SIZEOF(Actor100400MarkerWork, 0x64);

/// 0x1C-byte scratch `Actor00400_Fn03318` carves off `G_SCRATCH_HEAD` to hold
/// `RotTransPers4`'s outputs for the quad it projects: the four screen-space
/// corners, the perspective term, the clip flags and the average depth used as
/// the OT key.
typedef struct Actor100400TextQuadScratch {
    /* 0x00 */ s32 screen0;
    /* 0x04 */ s32 screen1;
    /* 0x08 */ s32 screen2;
    /* 0x0C */ s32 screen3;
    /* 0x10 */ s32 perspective;
    /* 0x14 */ s32 flags;
    /* 0x18 */ s32 depth;
} Actor100400TextQuadScratch;
STATIC_ASSERT_SIZEOF(Actor100400TextQuadScratch, 0x1C);

/// 8-byte waypoint record in the `Actor100400Work.field_608` array, walked
/// until `field_6` is -1. `field_0` / `field_4` are the X and Z the actor
/// steers toward; `field_6` selects the kind, where 1 is only eligible for
/// the record `field_64A` already points at.
typedef struct Actor100400Record {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ s16  field_4;
    /* 0x06 */ s16  field_6;
} Actor100400Record;

/// 0x1C-byte scratch taken off `G_SCRATCH_HEAD` by `Actor00400_Fn031A4` while
/// it searches `Actor100400Work.field_608` for the nearest record: `delta`
/// holds the XZ difference from `field_5E4`, `best` the smallest distance seen
/// so far and `index` the record being tested.
typedef struct Actor100400NearestScratch {
    /* 0x00 */ VECTOR delta;
    /* 0x10 */ s32    best;
    /* 0x14 */ s32    dist;
    /* 0x18 */ s16    index;
    /* 0x1A */ s16    bestIndex;
} Actor100400NearestScratch;
STATIC_ASSERT_SIZEOF(Actor100400NearestScratch, 0x1C);

/// 8-byte waypoint indexed by `Actor100400Work.field_65B` (wraps at 8);
/// `field_2` offsets the Y base in `Actor00400_Fn05D00`, and
/// `Actor00400_Fn02208` measures the XZ distance from the root coordinate.
typedef struct Actor100400Entry8 {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[2];
} Actor100400Entry8;

typedef union Actor100400Mat {
    MATRIX     mat;
    GpMtxWords ident;
    /// The same storage reused as the view-space position `Actor00400_Fn0A08C`
    /// fills in, once the rotation it held has been handed to the coordinate.
    SVECTOR vec;
} Actor100400Mat;
STATIC_ASSERT_SIZEOF(Actor100400Mat, 0x20);

typedef union Actor100400Flags {
    u32 word;
    u16 half;
    struct {
        u16 lo;
        s16 field_62E;
    } hi;
} Actor100400Flags;

typedef struct Actor100400Work {
    /* 0x000 */ GpAnimCtx          anim;
    /* 0x014 */ GpAnimSlot         slots[15];
    /* 0x26C */ byte               poses[0xF0];
    /* 0x35C */ GpObj              obj_35C;
    /* 0x37C */ GpObj              obj_37C;
    /* 0x39C */ GpRec18            field_39C[6];
    /* 0x42C */ GpObj              obj_42C;
    /* 0x44C */ GpRec18            field_44C[6];
    /* 0x4DC */ GpObj              obj_4DC;
    /* 0x4FC */ GpRec18            rec_4FC[3];
    /* 0x544 */ byte               pad_544[2];
    /* 0x546 */ u16                field_546;
    /* 0x548 */ byte               pad_548[4];
    /* 0x54C */ s16                field_54C;
    /* 0x54E */ s16                field_54E;
    /* 0x550 */ s16                field_550;
    /* 0x552 */ byte               pad_552[2];
    /* 0x554 */ s16                field_554;
    /* 0x556 */ s16                field_556;
    /* 0x558 */ s16                field_558;
    /* 0x55A */ byte               pad_55A[0xA];
    /* 0x564 */ s16                field_564;
    /* 0x566 */ byte               pad_566[2];
    /* 0x568 */ s16                field_568;
    /* 0x56A */ byte               pad_56A[2];
    /* 0x56C */ SVECTOR            field_56C;
    /* 0x574 */ SVECTOR            field_574;
    /* 0x57C */ MATRIX             field_57C;
    /* 0x59C */ MATRIX             field_59C;
    /* 0x5BC */ MATRIX             field_5BC;
    /* 0x5DC */ GpEffArg           field_5DC;
    /* 0x5E4 */ SVECTOR            field_5E4;
    /* 0x5EC */ SVECTOR            field_5EC;
    /* 0x5F4 */ SVECTOR            field_5F4;
    /* 0x5FC */ byte               pad_5FC[0xC];
    /* 0x608 */ Actor100400Record* field_608;
    /* 0x60C */ Actor100400Entry8* field_60C;
    /* 0x610 */ s32                field_610;
    /* 0x614 */ s16                field_614[3];
    /* 0x61A */ byte               pad_61A[2];
    /* 0x61C */ s16                field_61C;
    /* 0x61E */ s16                field_61E;
    /* 0x620 */ s16                field_620;
    /* 0x622 */ s16                field_622;
    /* 0x624 */ s16                field_624;
    /* 0x626 */ s16                field_626;
    /* 0x628 */ s16                field_628;
    /* 0x62A */ s16                field_62A;
    /* 0x62C */ Actor100400Flags   flags_62C;
    /* 0x630 */ s16                field_630;
    /* 0x632 */ s16                field_632;
    /* 0x634 */ u16                field_634;
    /* 0x636 */ s16                field_636;
    /* 0x638 */ s16                field_638;
    /* 0x63A */ u16                field_63A;
    /* 0x63C */ s16                field_63C;
    /* 0x63E */ s16                field_63E;
    /* 0x640 */ s16                field_640;
    /* 0x642 */ s16                field_642;
    /* 0x644 */ s16                field_644;
    /* 0x646 */ s16                field_646;
    /* 0x648 */ s16                field_648;
    /* 0x64A */ s16                field_64A;
    /* 0x64C */ s16                field_64C;
    /* 0x64E */ s16                field_64E;
    /* 0x650 */ s16                field_650;
    /* 0x652 */ s16                field_652;
    /* 0x654 */ s16                field_654;
    /* 0x656 */ byte               pad_656[2];
    /* 0x658 */ u16                field_658;
    /* 0x65A */ u8                 field_65A;
    /* 0x65B */ u8                 field_65B;
    /* 0x65C */ byte               pad_65C[1];
    /* 0x65D */ u8                 field_65D;
    /* 0x65E */ u8                 field_65E;
    /* 0x65F */ u8                 field_65F;
    /* 0x660 */ u8                 field_660;
    /* 0x661 */ u8                 field_661;
    /* 0x662 */ byte               pad_662[1];
    /* 0x663 */ u8                 field_663;
    /* 0x664 */ u8                 field_664;
    /* 0x665 */ s8                 field_665;
    /* 0x666 */ u8                 field_666;
} Actor100400Work;

/// One 0x14-byte row of `Actor00400_D15F20`, the per-room spawn table the entry
/// state walks until `area` reads 0xFF. A row matches when its `area` / `room`
/// equal `GameSession.at4.loc.stage` / `at4.loc.area`; `flags` bit 1 rejects the actor
/// outright and bit 2 hides its root coordinate. The three pointers are
/// optional overrides taken from the room overlay: `waypointSets` is indexed by
/// the spawn argument's second nibble, `records` becomes
/// `Actor100400Work.field_608` and `height` seeds `field_64E`.
typedef struct Actor100400AreaConfig {
    /* 0x00 */ Actor100400Entry8** waypointSets;
    /* 0x04 */ Actor100400Record*  records;
    /* 0x08 */ u16*                height;
    /* 0x0C */ s16                 area;
    /* 0x0E */ s16                 room;
    /* 0x10 */ u16                 flags;
    /* 0x12 */ byte                pad_12[2];
} Actor100400AreaConfig;
STATIC_ASSERT_SIZEOF(Actor100400AreaConfig, 0x14);

void       Actor00400_Fn005DC(GpCoord* arg0, u16 arg1, u16 arg2, s32 arg3);
extern s32 D_80115738;

/* This overlay calls the gameplay helpers through its own (wider) prototypes:
   the extra trailing arguments are set up at every call site but ignored by
   the definitions in src/gameplay/3A34.c. */

void Actor00400_Fn0875C(Task* arg0, Actor100400Entry8* arg1, s32 arg2, s32 arg3);
void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void Actor00400_Fn02648(Task* arg0, s32 arg1);
void Actor00400_Fn0237C(Task* arg0);
void Actor00400_Fn02FF8(Task* arg0);
void Actor00400_Fn0A190(Task* arg0);
void Actor00400_Fn0A28C(Task* arg0);
void Actor00400_Fn089C8(Task* arg0);
void Actor00400_Fn03920(Task* arg0);
void Actor00400_Fn04580(Task* arg0);
void Actor00400_Fn04B48(Task* arg0);
void Actor00400_Fn04E18(Task* arg0);
void Actor00400_Fn040DC(Task* arg0);
void Actor00400_Fn06B7C(Task* arg0);
void Actor00400_Fn070C0(Task* arg0);
void Actor00400_Fn085B8(Task* arg0);
void Actor00400_Fn08624(Task* arg0);
s16  Actor00400_Fn086FC(Task* arg0, s16 arg1);
void Actor00400_Fn08814(Task* arg0);
s16  Actor00400_Fn08908(Task* arg0);
void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
void Actor00400_Fn060CC(Task* arg0);
void Actor00400_Fn09714(Task* arg0);
void Actor00400_Fn097C8(Task* arg0);
void Actor00400_Fn06EA4(Task* arg0);
void Actor00400_Fn08ADC(Task* arg0);
void Actor00400_Fn08A88(Task* arg0);
void Actor00400_Fn08B40(Task* arg0);
void Actor00400_Fn08B94(Task* arg0);
void Actor00400_Fn06F64(Task* arg0);
void Actor00400_Fn0A880(Task* arg0);
void Actor00400_Fn04900(Task* arg0);
void Actor00400_Fn0A940(Task* arg0);
void Actor00400_Fn04A1C(Task* arg0);
void Actor00400_Fn0A9F4(Task* arg0);
void Actor00400_Fn0AA40(Task* arg0);
void Actor00400_Fn0A3D4(Task* arg0);
void Actor00400_Fn0A414(Task* arg0);
void Actor00400_Fn098A8(Task* arg0);
void Actor00400_Fn09924(Task* arg0);
s16  Actor00400_Fn02154(Task* arg0);
void Actor00400_Fn0A5B8(Task* arg0);
void Actor00400_Fn019B4(Task* arg0);
void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3);
void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst);
void Actor00400_Fn03570(GpCoord* arg0, s16 arg1);
void func_8004BFF8(s32 angle, MATRIX* matrix);
/* Declared here with a signed id: see the note in gameplay/1BC.h. */
void func_800B4114(Actor100400Work* anim, s32 slot, s32 active, s32 arg3, s32 arg4);
s32  Actor00400_Fn02208(Task* arg0);
void Actor00400_Fn0A680(Task* arg0);
void Actor00400_Fn0A6B0(Task* arg0);
void Actor00400_Fn0A704(Task* arg0);
void Actor00400_Fn0A760(Task* arg0);
void Actor00400_Fn0A7F0(Task* arg0);
void Actor00400_Fn0A82C(Task* arg0);
void Actor00400_Fn0A034(Task* arg0);
s32  Actor00400_Fn0A08C(GpCoord* coord, SVECTOR* pos);
void Actor00400_Fn0A510(Task* arg0);
void Actor00400_Fn0A57C(Task* arg0);
void Actor00400_Fn0762C(Task* arg0, s16 arg1, s16 arg2);

/* States the dispatch tables name before their definitions. */
void Actor00400_Fn042C0(Task* arg0);
void Actor00400_Fn04414(Task* arg0);
void Actor00400_Fn04CF8(Task* arg0);
void Actor00400_Fn05728(Task* arg0);
void Actor00400_Fn07738(Task* arg0);
void Actor00400_Fn077F4(Task* arg0);
void Actor00400_Fn078C8(Task* arg0);
void Actor00400_Fn0793C(Task* arg0);
void Actor00400_Fn07998(Task* task);
void Actor00400_Fn079A0(Task* task);
void Actor00400_Fn079A8(Task* arg0);
void Actor00400_Fn079FC(Task* arg0);
void Actor00400_Fn07ABC(Task* arg0);
void Actor00400_Fn07B10(Task* arg0);
void Actor00400_Fn07B98(Task* arg0);
void Actor00400_Fn07C04(Task* arg0);
void Actor00400_Fn07CC4(Task* arg0);
void Actor00400_Fn07DE0(Task* arg0);
void Actor00400_Fn07E20(Task* arg0);
void Actor00400_Fn07E74(Task* arg0);
void Actor00400_Fn07EE8(Task* arg0);
void Actor00400_Fn07F18(Task* arg0);
void Actor00400_Fn07F44(Task* arg0);
void Actor00400_Fn07F88(Task* arg0);
void Actor00400_Fn07FEC(Task* arg0);
void Actor00400_Fn08C54(Task* arg0);
void Actor00400_Fn08D70(Task* arg0);
void Actor00400_Fn08DFC(Task* arg0);
void Actor00400_Fn08E50(Task* arg0);
void Actor00400_Fn08FB0(Task* arg0);
void Actor00400_Fn08FC8(Task* arg0);
void Actor00400_Fn08FF4(Task* arg0);
void Actor00400_Fn09038(Task* arg0);
void Actor00400_Fn0909C(Task* arg0);
void Actor00400_Fn090B4(Task* arg0);
void Actor00400_Fn09124(Task* arg0);
void Actor00400_Fn091F8(Task* arg0);
void Actor00400_Fn09260(Task* arg0);
void Actor00400_Fn092D4(Task* arg0);
void Actor00400_Fn09348(Task* arg0);
void Actor00400_Fn093BC(Task* task);
void Actor00400_Fn093C4(Task* arg0);
void Actor00400_Fn09418(Task* arg0);
void Actor00400_Fn0946C(Task* arg0);
void Actor00400_Fn094C0(Task* arg0);
void Actor00400_Fn094DC(Task* arg0);
void Actor00400_Fn095D8(Task* arg0);
void Actor00400_Fn096C0(Task* arg0);
void Actor00400_Fn09A1C(Task* arg0);
void Actor00400_Fn09A48(Task* arg0);
void Actor00400_Fn09A8C(Task* arg0);
void Actor00400_Fn09AE0(Task* arg0);
void Actor00400_Fn09B44(Task* arg0);
void Actor00400_Fn09B74(Task* arg0);
void Actor00400_Fn09BDC(Task* arg0);
void Actor00400_Fn09C04(Task* arg0);
void Actor00400_Fn09C84(Task* arg0);
void Actor00400_Fn09CCC(Task* arg0);
void Actor00400_Fn09D3C(Task* arg0);
void Actor00400_Fn09D98(Task* arg0);
void Actor00400_Fn09E70(Task* arg0);
void Actor00400_Fn09F18(Task* arg0);
void Actor00400_Fn09FDC(Task* arg0);

extern MATRIX     Gfx_ViewWorldMtx;
extern GpPairSrcE Actor00400_D0FDC8;
/// Pair table `Actor00400_Fn0A190` packs, at index 1, into the marker object's
/// `key`.
extern struct GpU16Pair      Actor00400_D0FDC0;
extern TaskDesc              Actor00400_D16028;
extern Actor100400AreaConfig Actor00400_D15F20[];
extern u32                   Actor00400_D16010;

extern u16   Actor00400_D1609C[8];
extern byte  Actor00400_D1604C[];
extern u8    Actor00400_D0E5B8[];
extern u8    Actor00400_D0E970[];
extern u8    Actor00400_D0ED28[];
extern u8    Actor00400_D0F25C[];
extern u8    Actor00400_D0F790[];
extern u8    Actor00400_D0FD9C[];
extern void* D_800678F0[1];

/* Inline rotation traversal helpers. Every ancestor rotation is copied out
   and renormalised before it is fed to the GTE, instead of being loaded
   straight from the coordinate. */

/// Accumulate `arg0`'s parent chain into `arg1`: seed it with the node's own
/// rotation, then pre-multiply by each (renormalised) ancestor up to but not
/// including `arg2`, renormalising after every step. Returns whether the walk
/// stopped on `arg2` rather than running off the end of the chain.
static __inline__ s32 Actor00400_AccumulateRotation(GpCoord* arg0, MATRIX* arg1, GpCoord* arg2)
{
    MATRIX   normal;
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
        matrix = coord->coord;
        MatrixNormal(&matrix, &matrix);
        gte_SetRotMatrix(&matrix);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &normal);
        *arg1 = normal;
        coord = coord->sub;
    }
}

/// Undo the parent chain again, turning the world-space rotation in `arg1`
/// back into one relative to `arg0`'s parent: accumulate the chain *above* the
/// parent, transpose it (the 3x3 inverse of a rotation) and pre-multiply.
/// Nothing to do when the parent is already the view coordinate.
///
/// Returns `arg0` so the caller stores through the returned pointer; the copy
/// GCC emits where the exits merge is what gives the store base its own
/// pseudo. Three details here are matching requirements rather than style:
/// the early `return arg0;` on the end-of-chain exit (it is what lifts `arg0`
/// past the scratch pointers in global-alloc's priority order, so it keeps
/// `$s3`), and the `mp` / `lp` pointer variables, whose declarations must
/// precede `view` so their pseudos out-rank it when the two tie.
static __inline__ GpCoord* Actor00400_LocalizeRotation(GpCoord* arg0, MATRIX* arg1)
{
    MATRIX   matrix;
    MATRIX   local;
    MATRIX   normal;
    MATRIX   transposed;
    MATRIX*  mp;
    MATRIX*  lp;
    GpCoord* coord;
    GpCoord* view;

    coord = arg0->sub;
    if (coord != &gGfxViewCoord) {
        mp     = &matrix;
        view   = &gGfxViewCoord;
        lp     = &local;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                return arg0;
            }
            if (coord == view) {
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
                    : : "r"(mp), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(arg1);
                break;
            }
            local = coord->coord;
            MatrixNormal(&local, &local);
            gte_SetRotMatrix(lp);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return arg0;
}

/// Spawns the hit/impact effect burst for `coord`. `arg3` packs an effect
/// parameter in its low 12 bits and a 4-bit variant index at bits 12..15;
/// `kind` selects between a single spark (0), a spark plus an optional
/// directional puff (1), and a four-shot burst (2). `phase` drives the
/// sub-effect `Actor00400_Fn005DC` plays and gates the puff on its low bits.
void Actor00400_Fn001AC(GpCoord* coord, u16 phase, u16 kind, u32 arg3)
{
    SVECTOR vec;
    s32     i;
    u16     variant;
    u16     param;

    if (Gp_State1C->eventState != 0) {
        Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, 0x400, 0);
        if (Gp_State1C->eventState >= 4) {
            return;
        }
    }

    variant = (arg3 >> 12) & 0xF;
    param   = arg3 & 0xFFF;

    switch (kind) {
        case 0:
            Gp_SpawnEff(D_80115738, coord, 0x14001000 + param + variant, NULL);
            break;

        case 1:
            Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, param, 0);
            if (!(phase & 1)) {
                Gp_SpawnEff(D_80115738, coord, 0x01000000 + param + variant, NULL);
            }
            if (!(phase & 7)) {
                SVECTOR* dir;

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;

        case 2:
            Actor00400_Fn005DC(coord, ((u32)phase >> 1) % 6, param, 0);
            Gp_SpawnEff(D_80115738, coord, 0x10001000 + param + variant, NULL);
            for (i = 0; i < 4; i++) {
                SVECTOR* dir;

                Gp_SpawnEff(D_80115738, coord, 0x02001000 + param + variant, NULL);

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;
    }
}

/// Links one frame of the rotating impact-spark billboard at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` picks
/// one of the six 0x27 x 0x27 texture frames along row 0x38 of tpage 0x2A,
/// `arg2` sizes the quad and `arg3` spins it: the corners sit `arg2 * 0x27 /
/// otz` from the projected centre along `arg3` and `arg3 + 0x400`, so the
/// spark shrinks with depth.
void Actor00400_Fn005DC(GpCoord* arg0, u16 arg1, u16 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    GpEffFlareScratch* blk;
    GpEffFlareScratch* copy;
    POLY_FT4*          prim;
    s32                ang;
    u16                frame;
    s32                u;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    blk                            = (GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch));
    copy                           = blk;
    blk->vec.vx                    = (u16)arg0->workm.t[0];
    blk->vec.vy                    = (u16)arg0->workm.t[1];
    blk->vec.vz                    = (u16)arg0->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((GpEffFlareScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpEffFlareScratch*)(head - 0x1C))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(copy);
        ((GpEffFlareScratch*)(head - 0x1C))->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        frame       = arg1 % 6;
        u           = frame * 0x28;
        setUV4(prim, u, 0x38, u + 0x27, 0x38, u, 0x5F, u + 0x27, 0x5F);
        ang      = (s16)arg3;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
        prim->x0 = blk->sx + (u16)blk->dx;
        prim->x3 = blk->sx - (u16)blk->dx;
        prim->y0 = blk->sy - (u16)blk->dy;
        prim->y3 = blk->sy + (u16)blk->dy;
        ang      = ang + 0x400;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
        prim->x1 = blk->sx + (u16)blk->dx;
        prim->x2 = blk->sx - (u16)blk->dx;
        prim->y1 = blk->sy - (u16)blk->dy;
        prim->y2 = blk->sy + (u16)blk->dy;
        addPrim((u_long*)(((((u32)((GpEffFlareScratch*)(head - 0x1C))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(GpEffFlareScratch));
}

void Actor00400_Fn00A14(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_646 != 0) {
        if (!(work->field_646 & 7)) {
            s32 id  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4004000B;
            s32 pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        if (work->field_646 == 0x18 || work->field_646 == 0x30) {
            func_800FDB18(7, &arg0->extra.tmd->coords[1], NULL, &work->field_5DC);
        }
        if (work->field_646 == 0x16 && work->field_666 == 0) {
            work->obj_4DC.flags |= 0x8000;
        }
        if (--work->field_646 == 0) {
            work->obj_4DC.flags &= 0x7FFF;
        }
    }
}

void Actor00400_Fn00B48(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;
    GpEnemy*         obj;
    GpCoord*         coord;
    GpCoord*         coords;
    u16              hp;
    u8               slot;

    ctx                        = arg0->extra.tmd;
    work                       = arg0->work;
    obj                        = arg0->spawnArg2;
    ctx->lightMtx              = &work->field_59C;
    ctx->flags                 = 0;
    ctx->colorMtx              = &work->field_57C;
    coords                     = arg0->extra.tmd->coords;
    coord                      = ctx->coords;
    work->field_5DC.spawnArgLo = 0x600;
    work->field_5DC.spawnArgHi = 3;
    work->field_664            = 4;
    work->field_5DC.coord      = &coords[1];
    obj->field_4               = &coord->coord;
    obj->field_48              = 0;
    obj->bodyPos.vx            = 0;
    obj->bodyPos.vy            = 0;
    obj->bodyPos.vz            = 0;
    slot                       = work->field_664;
    obj->coord                 = &arg0->extra.tmd->coords[slot];
    ((void (*)(GpLinkNode*, s32))Gp_LinkNode)(&obj->node, slot);
    obj->node.state.b.flags = 1;
    obj->recs               = work->field_39C;
    obj->param              = &Actor00400_D0FDC8;
    hp                      = Actor00400_D0FDC8.hpMax;
    obj->hpMax              = hp;
    obj->hp                 = hp;
    coord->sub              = &gGfxViewCoord;
    func_800B3F84(&work->anim, Actor00400_D1604C, ctx, work->poses, work->slots);
    Actor00400_Fn019B4(arg0);
    work->field_556 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

void Actor00400_Fn00C84(Task* arg0)
{
    Task*             actor;
    Task*             player;
    Actor100400Work*  work;
    GpCoord*          coord;
    GpCoord*          pc;
    Actor100400Entry8 pos;
    u8                frame;

    actor  = arg0;
    player = Gp_ActorSlots[0];
    work   = actor->work;
    coord  = actor->extra.tmd->coords;
    if (work->field_628 != 4) {
        Actor00400_Fn088EC(actor, 4, 0x20, 0xA);
        Actor00400_Fn08814(actor);
    }
    frame = Actor00400_Fn086FC(actor, 0x39);
    if (Actor00400_Fn08908(actor)) {
        work->field_62A = 0;
    }
    if (work->field_62A == 0) {
        Actor00400_Fn0824C(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
        if (work->pad_65C[0] & 1) {
            s32 id  = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 12) << 8) | 0x40040002;
            s32 pan = (s8)Gp_GetObjPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(actor->extra.tmd->coords));
        } else {
            s32 id  = ((((GpEnemy*)actor->spawnArg2)->placeKey >> 12) << 8) | 0x40040003;
            s32 pan = (s8)Gp_GetObjPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(actor->extra.tmd->coords));
        }
        work->pad_65C[0]++;
    }
    if (work->field_62A >= 0 && frame >= work->field_62A) {
        pc          = player->extra.tmd->coords;
        pos.field_0 = pc->coord.t[0];
        pos.field_2 = pc->coord.t[1];
        pos.field_4 = pc->coord.t[2];
        Actor00400_Fn0875C(actor, &pos, 8, 0x100);
        Actor00400_Fn08464(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
    }
    coord->flg = 0;
}

/// Draws one textured, semi-transparent quad for the segment between model
/// parts `firstJoint` and `secondJoint`, laid flat at the view-space height
/// `height`. The quad is `width` wide on each side of the segment and
/// stretches half the segment's length past each end; it is tinted grey by
/// `shade` and skipped when the projection clips it. Equal parts draw nothing.
void Actor00400_Fn00E3C(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GpCoord*          secondCoord;
    GpCoord*          firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfZ;
    GpCoord*          coords;
    GpCoord*          view;
    POLY_FT4*         poly;

    coords      = actor->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_PUSH_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &s->secondMatrix);
        s->first.vy   = height;
        s->second.vy  = height;
        s->first.vx   = s->firstMatrix.t[0];
        s->first.vz   = s->firstMatrix.t[2];
        s->second.vx  = s->secondMatrix.t[0];
        s->second.vz  = s->secondMatrix.t[2];
        angle         = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX         = (s->first.vx - s->second.vx) / 2;
        halfZ         = (s->first.vz - s->second.vz) / 2;
        offset0       = rcos(angle) * width;
        s->corner0.vy = height;
        s->corner0.vx = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1       = rcos(angle) * width;
        s->corner1.vy = height;
        s->corner1.vx = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2       = rcos(angle) * width;
        s->corner2.vy = height;
        s->corner2.vx = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3       = rcos(angle) * width;
        s->corner3.vy = height;
        s->corner3.vx = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `Gfx_ViewWorldMtx`, whose high half the GTE loads below share. */
        view      = PARENT_OF(&Gfx_ViewWorldMtx, GpCoord, workm);
        view->flg = 0;
        Gp_UpdateCoord(view);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        SCRATCH_POP_BYTES(sizeof(ActorBeamScratch));
    }
}

void Actor00400_Fn012B0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    Actor00400_Fn00E3C(arg0, 1, 2, 0x258, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 2, 3, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 3, 4, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 4, 5, 0x1F4, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 6, 0x320, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 6, 7, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 7, 8, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 0xC, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xC, 0xD, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xD, 0xE, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 1, 9, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 9, 0xA, 0x12C, arg1, temp_s2);
    Actor00400_Fn00E3C(arg0, 0xA, 0xB, 0x12C, arg1, temp_s2);
}

/* Tracks the nearer of the two party members and stores the result in the
   actor's work block.

   `field_54C`..`field_550` snapshot the actor's own root translation. The
   second coordinate of the model (`coord[1]`) is taken into view space and
   each slot's root translation measured against it; the closer of the two
   lands in `field_5E4` with its XZ distance in `field_640`. The chosen
   offset is then normalised and turned into a yaw relative to the actor's
   own heading (`field_556`) in `field_634`. */
void Actor00400_Fn01454(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    GpCoord*         c0;
    GpCoord*         c1;
    Task*            player;
    GpCoord*         joint;
    SVECTOR          delta0;
    SVECTOR          delta1;
    SVECTOR          view;
    s32              dist0;
    s32              dist1;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    player          = Gp_ActorSlots[0];
    joint           = &coord[1];
    work->field_54C = coord->coord.t[0];
    work->field_54E = coord->coord.t[1];
    work->field_550 = coord->coord.t[2];
    if (player != NULL) {
        c0      = player->extra.tmd->coords;
        view.vx = 0;
        view.vy = 0;
        view.vz = 0;
        Actor00400_Fn0A08C(joint, &view);
        delta0.vx = c0->coord.t[0] - view.vx;
        delta0.vy = c0->coord.t[1] - view.vy;
        delta0.vz = c0->coord.t[2] - view.vz;
        dist0     = SquareRoot0(delta0.vx * delta0.vx + delta0.vz * delta0.vz);
        if (Gp_ActorSlots[1] == NULL) {
            work->field_5E4.vx = c0->coord.t[0];
            work->field_5E4.vy = c0->coord.t[1];
            work->field_5E4.vz = c0->coord.t[2];
            work->field_640    = dist0;
        } else {
            c1        = Gp_ActorSlots[1]->extra.tmd->coords;
            delta1.vx = c1->coord.t[0] - view.vx;
            delta1.vy = c1->coord.t[1] - view.vy;
            delta1.vz = c1->coord.t[2] - view.vz;
            dist1     = SquareRoot0(delta1.vx * delta1.vx + delta1.vz * delta1.vz);
            if (dist1 < dist0) {
                work->field_5E4.vx = c1->coord.t[0];
                work->field_5E4.vy = c1->coord.t[1];
                work->field_5E4.vz = c1->coord.t[2];
                delta0             = delta1;
                dist0              = dist1;
            } else {
                work->field_5E4.vx = c0->coord.t[0];
                work->field_5E4.vy = c0->coord.t[1];
                work->field_5E4.vz = c0->coord.t[2];
            }
            work->field_640 = dist0;
        }
        VectorNormalSS(&delta0, &delta0);
        work->field_634 = (ratan2(delta0.vx, delta0.vz) - work->field_556) & 0xFFF;
    }
}

/* Re-aims the two upper body coordinates at the target yaw held in
   `field_546` and folds the result back into the model root.

   With `arg1 == 0` the yaw chases the heading `Actor00400_Fn0814C` reports:
   it steps 0x18 per frame while the error is more than 0x20, and decays
   toward zero once the heading leaves +/-0x5FF. A non-zero `arg1` only
   decays, twice as fast. Each of the two coordinates then gets its pitch
   re-applied about X and a third of the yaw about Y, and the composed
   inverse of all three lands in `c4`. */
void Actor00400_Fn016A4(Task* arg0, s32 arg1)
{
    SVECTOR          euler;
    SVECTOR          rot1;
    SVECTOR          rot2;
    MATRIX           t1;
    MATRIX           t2;
    MATRIX           t3;
    Actor100400Mat   ma;
    Actor100400Mat   mb;
    Actor100400Mat   mc;
    GpMtxWords*      ia;
    GpMtxWords*      ib;
    GpMtxWords*      ic;
    GpCoord*         base;
    GpCoord*         c1;
    GpCoord*         c2;
    GpCoord*         c3;
    GpCoord*         c4;
    Actor100400Work* work;
    MATRIX*          m2;
    MATRIX*          m3;

    base = arg0->extra.tmd->coords;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    work = arg0->work;
    Actor00400_Fn0814C(arg0, 4, &euler, 0x600);
    if ((arg1 & 0xFF) == 0) {
        if ((u16)(euler.vy + 0x5FF) < 0xBFF) {
            if ((u32)((euler.vy - (s16)work->field_546) + 0x20) >= 0x41) {
                if ((s16)work->field_546 < euler.vy) {
                    work->field_546 = work->field_546 + 0x18;
                } else {
                    work->field_546 = work->field_546 - 0x18;
                }
            }
        } else {
            work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 8);
        }
    } else {
        work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 7);
    }

    m2 = &c2->coord;
    ia = &ma.ident;
    ib = &mb.ident;
    ic = &mc.ident;

    ma.ident.m00_m01 = 0x1000;
    ma.ident.m02_m10 = 0;
    ia->m11_m12      = 0x1000;
    ma.ident.m20_m21 = 0;
    ia->m22          = 0x1000;
    mb.ident.m00_m01 = 0x1000;
    mb.ident.m02_m10 = 0;
    ib->m11_m12      = 0x1000;
    mb.ident.m20_m21 = 0;
    ib->m22          = 0x1000;
    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    Gp_MtxToEuler(m2, &rot1);
    m3 = &c3->coord;
    Gp_MtxToEuler(m3, &rot2);
    RotMatrixX(rot1.vx, &ma.mat);
    RotMatrixX(rot2.vx, &mb.mat);
    Actor00400_Fn08A1C(&ma.mat, m2);
    Actor00400_Fn08A1C(&mb.mat, m3);
    Gp_UpdateCoord(c1);
    Gp_UpdateCoord(c2);
    Gp_UpdateCoord(c3);
    Actor00400_Fn03570(c2, (s16)work->field_546 / 3);
    Actor00400_Fn03570(c3, (s16)work->field_546 / 3);

    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    func_8004BFF8((s16)work->field_546 / 3, &mc.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(m2, &t2);
    TransposeMatrix(m3, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.mat);
    Actor00400_Fn08A1C(&t1, &c4->coord);
}

/* Links the actor's four collision objects and clears their record tables;
   `obj_42C` takes hit flag 0x4000 from `field_661`. */
void Actor00400_Fn019B4(Task* arg0)
{
    Actor100400Work* work = arg0->work;

    work->obj_35C.coord    = &arg0->extra.tmd->coords[1];
    work->obj_35C.ctx.recs = work->field_39C;
    work->obj_35C.pos.vx   = 0;
    work->obj_35C.pos.vy   = 0;
    work->obj_35C.pos.vz   = 0;
    work->obj_35C.key      = 0x30004;
    work->obj_35C.radius   = 0x300;
    work->obj_35C.flags    = 1;
    Gp_LinkObj(2, &work->obj_35C);
    Gp_InitRec18Table(work->field_39C, 6, 0);
    work->obj_35C.flags |= 0x8000;

    work->obj_37C.coord    = &arg0->extra.tmd->coords[4];
    work->obj_37C.ctx.recs = work->field_39C;
    work->obj_37C.pos.vx   = 0;
    work->obj_37C.pos.vy   = 0;
    work->obj_37C.pos.vz   = 0;
    work->obj_37C.key      = 0x30004;
    work->obj_37C.radius   = 0xC0;
    work->obj_37C.flags    = 1;
    Gp_LinkObj(2, &work->obj_37C);
    work->obj_37C.flags |= 0x8000;

    work->obj_4DC.coord    = &arg0->extra.tmd->coords[1];
    work->obj_4DC.ctx.recs = work->rec_4FC;
    work->obj_4DC.pos.vx   = 0;
    work->obj_4DC.pos.vy   = 0;
    work->obj_4DC.pos.vz   = 0;
    work->obj_4DC.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_4DC.radius   = 0x480;
    work->obj_4DC.flags    = 1;
    Gp_LinkObj(3, &work->obj_4DC);
    Gp_InitRec18Table(work->rec_4FC, 3, 0);
    work->obj_4DC.flags &= 0x7FFF;

    work->obj_42C.coord    = arg0->extra.tmd->coords;
    work->obj_42C.ctx.recs = work->field_44C;
    work->obj_42C.pos.vx   = 0;
    work->obj_42C.pos.vy   = 0;
    work->obj_42C.pos.vz   = 0;
    work->obj_42C.key      = 0x30004;
    work->obj_42C.radius   = 0x380;
    work->obj_42C.flags    = 1;
    Gp_LinkObj(2, &work->obj_42C);
    Gp_InitRec18Table(work->field_44C, 6, 0);
    if (work->field_661 != 0) {
        work->obj_42C.flags |= 0x4000;
    } else {
        work->obj_42C.flags &= 0xBFFF;
    }
}

/* Damage / knock-back tick: walks the six contact records, applies the hit
   the first one carries, then folds the accumulated push-back into the work
   position and the actor's coordinate. */
void Actor00400_Fn01B90(Task* arg0)
{
    Actor100400Work* work;
    GpEnemy*         obj;
    GpCoord*         coord;
    GpDeltaScratch   delta;
    s32              kind;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s32              tick;
    s32              i;

    kind            = 0;
    coord           = arg0->extra.tmd->coords;
    work            = arg0->work;
    obj             = arg0->spawnArg2;
    work->field_642 = 0;
    for (i = 0; i < 6; i++) {
        if ((work->field_39C[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_61C == 0) {
                work->field_642 = 1;
                work->field_65D = 1;
                dmg             = Gp_ComputeDamage(work->field_39C[i].key, work->field_640, 0, 0);
                amount          = dmg;
                work->field_61C = Gp_GetIdParam2(work->field_39C[i].key);
                if (Gp_RollEnemyChance(obj, work->field_39C[i].key, work->field_610) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->field_39C[i].key) & 0xFFFF,
                              &arg0->extra.tmd->coords[work->field_664], 0, &work->field_5DC);
                work->field_644 = (amount < 0x3C) ? 5 : 2;
                switch (Gp_GetIdParam0(work->field_39C[i].key) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(obj);
                        break;
                    case 2:
                        Gp_SetObjFlag2(obj, work->field_39C[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(obj, work->field_39C[i].key, 0);
                        break;
                    case 4:
                        work->field_644 = 4;
                        break;
                    case 5:
                        work->field_644 = 2;
                        break;
                    case 6:
                        work->field_644 = 4;
                        break;
                    case 7:
                        kind            = 2;
                        work->field_644 = 2;
                        amount         += amount;
                        break;
                    case 8:
                        work->field_644 = 0;
                        work->field_642 = 0;
                        break;
                    case 9:
                        work->field_644 = 1;
                        break;
                }
                if ((work->field_39C[i].key & 0x7F) == 0x1C && (work->field_39C[i].key & 0x8000) == 0) {
                    obj->reactionFlags &= 0xFE;
                    work->field_644     = 5;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[work->field_664], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[work->field_664], 2, 0);
                        break;
                }
                func_800E2C78(obj, work->field_39C[i].key, amount, 0);
                func_800DA6E8(&obj->node, amount, 0);
                obj->hp -= amount;
                if ((s16)obj->hp < 0) {
                    obj->hp = 0;
                }
            } else if ((Gp_GetIdParam1(work->field_39C[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], 0, &work->field_5DC);
            }
        }
        if (work->field_642 != 0) {
            break;
        }
    }

    if (obj->reactionFlags & 1) {
        obj->reactionFlags &= 0xFE;
        work->field_644     = 2;
    }
    if (obj->reactionFlags & 2) {
        obj->reactionFlags &= 0xFD;
        work->field_644     = 3;
    }
    if (obj->reactionFlags & 0xC) {
        tmp  = Gp_TickObjFlag4(obj);
        tick = (s16)tmp;
        if (tick != 0) {
            obj->hp -= tmp;
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            func_800DA6E8(&obj->node, tick, 0);
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            work->field_642 = 1;
            work->field_644 = 0;
        }
        if (Gp_ObjFlag4Expired(obj) != 0) {
            obj->reactionFlags &= 0xF3;
        }
    }

    switch (func_800E0C10(work->field_44C, &delta, 6, 0)) {
        case 0:
            break;
        case 1:
            tmp              = delta.vx.h.hi;
            work->field_564 += tmp;
            tmp              = delta.vz.h.hi;
            work->field_568 += tmp;
            if ((delta.vx.w & 0xFFFF) != 0) {
                if (delta.vx.w > 0) {
                    work->field_564++;
                } else {
                    work->field_564--;
                }
            }
            if ((delta.vz.w & 0xFFFF) != 0) {
                if (delta.vz.w > 0) {
                    work->field_568++;
                } else {
                    work->field_568--;
                }
            }
            tmp                = delta.vx.h.hi;
            coord->coord.t[0] += tmp;
            tmp                = delta.vz.h.hi;
            coord->coord.t[2] += tmp;
            if ((delta.vx.w & 0xFFFF) != 0) {
                if (delta.vx.w > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            if ((delta.vz.w & 0xFFFF) != 0) {
                if (delta.vz.w > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
            coord->flg = 0;
            break;
        case 2:
            coord->coord.t[0] = work->field_54C;
            coord->coord.t[2] = work->field_550;
            break;
    }

    Gp_ClearRec18Occupied(work->field_39C);
    Gp_ClearRec18Occupied(work->field_44C);
    if (work->field_61C > 0) {
        work->field_61C--;
    } else {
        work->field_61C = 0;
    }
}

s16 Actor00400_Fn02154(Task* arg0)
{
    Actor100400Work* work;
    s16              state;
    s16              req;

    work = arg0->work;
    if (work->field_642 != 1) {
        goto fail;
    }
    req = work->field_644;
    if (req == 1) {
        state = 7;
    } else if (req == 2) {
        state = 8;
    } else if (req == 3) {
        state = 9;
    } else if (req == 4) {
        state = 8;
    } else {
        goto other;
    }
    work->field_638 = state;
    work->field_63A = 0;
    work->field_644 = 0;
    goto ok;
other:
    if (req == 5) {
        Gp_StateF0.field_3 = 1;
        Gp_ArmStateF0(1);
        work->field_650 = 10;
        work->field_644 = 0;
        return 0;
    }
    work->field_644 = 0;
    goto fail;
ok:
    return 1;
fail:
    return 0;
}

s32 Actor00400_Fn02208(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = work->field_60C[work->field_65B].field_0 - coord->coord.t[0];
    vec.vy = work->field_60C[work->field_65B].field_2 - coord->coord.t[1];
    vec.vz = work->field_60C[work->field_65B].field_4 - coord->coord.t[2];
    if (work->field_628 != 3) {
        Actor00400_Fn088EC(arg0, 3, 0x10, 0xE);
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 400) {
        work->field_65B = (work->field_65B + 1) & 7;
        return 1;
    } else {
        Actor00400_Fn0875C(arg0, &work->field_60C[work->field_65B], 0x2C, 0x100);
        Actor00400_Fn0762C(arg0, 0x60, work->field_556);
        return 0;
    }
}

/* The random pick spawns in both arms rather than after the `if`: jump2
   cross-jumps the identical tails, which is what leaves the 0x20010 argument
   load ahead of the `D_800678F0` store in each arm. */
void Actor00400_Fn0237C(Task* arg0)
{
    GpEffWork* eff1;
    TmdObject* src1;
    TmdObject* dst1;
    GpEffWork* eff2;
    TmdObject* src2;
    TmdObject* dst2;
    GpEffWork* eff3;
    TmdObject* src3;
    TmdObject* dst3;
    GpEffWork* eff4;
    TmdObject* src4;
    TmdObject* dst4;
    GpEffWork* eff5;
    TmdObject* src5;
    TmdObject* dst5;

    D_800678F0[0] = Actor00400_D0E5B8;
    eff1          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[4], 0x200, NULL);
    if (eff1 != NULL) {
        src1        = arg0->extra.tmd;
        dst1        = eff1->task->extra.tmd;
        dst1->tpage = src1->tpage;
        dst1->clut  = src1->clut;
        if (dst1->buffer != NULL) {
            tmdProcessStream(dst1);
            tmdProcessStream(dst1);
        }
    }
    D_800678F0[0] = Actor00400_D0E970;
    eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[11], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = arg0->extra.tmd;
        dst2        = eff2->task->extra.tmd;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = Actor00400_D0ED28;
    eff3          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[14], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = arg0->extra.tmd;
        dst3        = eff3->task->extra.tmd;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = Actor00400_D0F25C;
        eff4          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = Actor00400_D0F790;
        eff4          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[8], 0x200, NULL);
    }
    if (eff4 != NULL) {
        src4        = arg0->extra.tmd;
        dst4        = eff4->task->extra.tmd;
        dst4->tpage = src4->tpage;
        dst4->clut  = src4->clut;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    D_800678F0[0] = Actor00400_D0FD9C;
    eff5          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff5 != NULL) {
        src5        = arg0->extra.tmd;
        dst5        = eff5->task->extra.tmd;
        dst5->tpage = src5->tpage;
        dst5->clut  = src5->clut;
        if (dst5->buffer != NULL) {
            tmdProcessStream(dst5);
            tmdProcessStream(dst5);
        }
    }
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x200, NULL);
}

/// Drives the two head/neck coordinates (`base[2]`, `base[3]`) and the aim
/// coordinate (`base[4]`) from `field_660`, the "actor is aiming" flag.
///
/// While aiming, `field_652` walks a small state machine: state 0 snaps the
/// five part coordinates to their bind pose and records the current Euler
/// angles, state 1 eases those angles back towards zero (and promotes to
/// state 2 once all six components are inside 0x30), and state 2 scales the
/// parts by `field_654` while the aim coordinate keeps its own rotation.
/// When the flag drops, `field_654` is eased back to 0x1000 with the same
/// scaling pass until it passes 0xF80, after which the stored angles are
/// blended halfway towards the live ones and the state resets to 0.
///
/// `invScale` is one function-scope variable rather than a local per arm on
/// purpose: with two assignments the pseudo has two deaths, so local-alloc
/// skips it and never ties the `divmodsi4` result to its dividend. That is
/// what leaves the quotient in the divisor's register (`mflo $v1`).
void Actor00400_Fn02648(Task* arg0, s32 arg1)
{
    VECTOR           scale;
    Actor100400Mat   rot;
    SVECTOR          euler0;
    Actor100400Mat   ma;
    SVECTOR          euler1;
    Actor100400Mat   mb;
    SVECTOR          euler2;
    Actor100400Mat   mc;
    Actor100400Work* work;
    GpCoord*         base;
    GpCoord*         c2;
    GpCoord*         c3;
    GpCoord*         c4;
    s32              invScale;

    base = arg0->extra.tmd->coords;
    work = arg0->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    if (work->field_660 != 0) {
        switch (work->field_652) {
            case 0:
                base[0].flg = 0;
                base[1].flg = 0;
                base[2].flg = 0;
                base[3].flg = 0;
                base[4].flg = 0;
                Gp_UpdateCoord(c4);
                Gp_MtxToEuler(&base[2].coord, &work->field_5EC);
                Gp_MtxToEuler(&base[3].coord, &work->field_5F4);
                work->field_652 = 1;
                work->field_654 = 0x1000;
                /* fallthrough */
            case 1: {
                GpMtxWords* ir;

                ir                 = &rot.ident;
                work->field_5EC.vx = (u16)work->field_5EC.vx + ((s32) - (work->field_5EC.vx * 0x10) >> 6);
                work->field_5EC.vy = (u16)work->field_5EC.vy + ((s32) - (work->field_5EC.vy * 0x10) >> 6);
                work->field_5EC.vz = (u16)work->field_5EC.vz + ((s32) - (work->field_5EC.vz * 0x10) >> 6);
                work->field_5F4.vx = (u16)work->field_5F4.vx + ((s32) - (work->field_5F4.vx * 0x10) >> 6);
                work->field_5F4.vy = (u16)work->field_5F4.vy + ((s32) - (work->field_5F4.vy * 0x10) >> 6);
                work->field_5F4.vz = (u16)work->field_5F4.vz + ((s32) - (work->field_5F4.vz * 0x10) >> 6);
                rot.ident.m00_m01  = 0x1000;
                rot.ident.m02_m10  = 0;
                ir->m11_m12        = 0x1000;
                rot.ident.m20_m21  = 0;
                ir->m22            = 0x1000;
                RotMatrix(&work->field_5EC, &rot.mat);
                Actor00400_Fn08A1C(&rot.mat, &c2->coord);
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                ir->m11_m12       = 0x1000;
                rot.ident.m20_m21 = 0;
                ir->m22           = 0x1000;
                RotMatrix(&work->field_5F4, &rot.mat);
                Actor00400_Fn08A1C(&rot.mat, &c3->coord);
                if ((abs(work->field_5EC.vx) < 0x30) && (abs(work->field_5EC.vy) < 0x30) && (abs(work->field_5EC.vz) < 0x30) &&
                    (abs(work->field_5F4.vx) < 0x30) && (abs(work->field_5F4.vy) < 0x30) && (abs(work->field_5F4.vz) < 0x30)) {
                    work->field_652 = 2;
                }
                c2->flg = 0;
                c3->flg = 0;
                c4->flg = 0;
                Gp_UpdateCoord(c4);
                break;
            }
            case 2: {
                GpMtxWords* ia;
                GpMtxWords* ib;
                GpMtxWords* ic;
                GpMtxWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler2);
                work->field_654  = (u16)work->field_654 + ((0x2AA - work->field_654) >> 3);
                ia               = &ma.ident;
                ma.ident.m00_m01 = 0x1000;
                ma.ident.m02_m10 = 0;
                ia->m11_m12      = 0x1000;
                ma.ident.m20_m21 = 0;
                ia->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = work->field_654;
                ScaleMatrix(&ma.mat, &scale);
                Actor00400_Fn08A1C(&ma.mat, &base[2].coord);
                ib               = &mb.ident;
                mb.ident.m00_m01 = 0x1000;
                mb.ident.m02_m10 = 0;
                ib->m11_m12      = 0x1000;
                mb.ident.m20_m21 = 0;
                ib->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = 0x1000;
                ScaleMatrix(&mb.mat, &scale);
                Actor00400_Fn08A1C(&mb.mat, &base[3].coord);
                ic               = &mc.ident;
                mc.ident.m00_m01 = 0x1000;
                mc.ident.m02_m10 = 0;
                ic->m11_m12      = 0x1000;
                mc.ident.m20_m21 = 0;
                ic->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                invScale         = 0x1000000 / work->field_654;
                scale.vz         = invScale;
                ScaleMatrix(&mc.mat, &scale);
                ir                = &rot.ident;
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                ir->m11_m12       = 0x1000;
                rot.ident.m20_m21 = 0;
                ir->m22           = 0x1000;
                RotMatrix(&euler2, &rot.mat);
                MulMatrix(&mc.mat, &rot.mat);
                Actor00400_Fn08A1C(&mc.mat, &c4->coord);
                base[2].flg = 0;
                base[3].flg = 0;
                base[4].flg = 0;
                Gp_UpdateCoord(c4);
                break;
            }
        }
    } else {
        base[0].flg = 0;
        base[1].flg = 0;
        base[2].flg = 0;
        base[3].flg = 0;
        base[4].flg = 0;
        Gp_UpdateCoord(c4);
        if (work->field_654 < 0xF80) {
            GpMtxWords* ia;
            GpMtxWords* ib;
            GpMtxWords* ic;
            GpMtxWords* ir;

            Gp_MtxToEuler(&c4->coord, &euler2);
            work->field_654  = (u16)work->field_654 + ((0x1000 - work->field_654) >> 3);
            ia               = &ma.ident;
            ma.ident.m00_m01 = 0x1000;
            ma.ident.m02_m10 = 0;
            ia->m11_m12      = 0x1000;
            ma.ident.m20_m21 = 0;
            ia->m22          = 0x1000;
            scale.vx         = 0x1000;
            scale.vy         = 0x1000;
            scale.vz         = work->field_654;
            ScaleMatrix(&ma.mat, &scale);
            Actor00400_Fn08A1C(&ma.mat, &base[2].coord);
            ib               = &mb.ident;
            mb.ident.m00_m01 = 0x1000;
            mb.ident.m02_m10 = 0;
            ib->m11_m12      = 0x1000;
            mb.ident.m20_m21 = 0;
            ib->m22          = 0x1000;
            scale.vx         = 0x1000;
            scale.vy         = 0x1000;
            scale.vz         = 0x1000;
            ScaleMatrix(&mb.mat, &scale);
            Actor00400_Fn08A1C(&mb.mat, &base[3].coord);
            ic               = &mc.ident;
            mc.ident.m00_m01 = 0x1000;
            mc.ident.m02_m10 = 0;
            ic->m11_m12      = 0x1000;
            mc.ident.m20_m21 = 0;
            ic->m22          = 0x1000;
            scale.vx         = 0x1000;
            scale.vy         = 0x1000;
            invScale         = 0x1000000 / work->field_654;
            scale.vz         = invScale;
            ScaleMatrix(&mc.mat, &scale);
            ir                = &rot.ident;
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            ir->m11_m12       = 0x1000;
            rot.ident.m20_m21 = 0;
            ir->m22           = 0x1000;
            RotMatrix(&euler2, &rot.mat);
            MulMatrix(&mc.mat, &rot.mat);
            Actor00400_Fn08A1C(&mc.mat, &c4->coord);
        } else {
            GpMtxWords* ir;
            MATRIX*     m2;
            MATRIX*     m3;

            m2 = &base[2].coord;
            Gp_MtxToEuler(m2, &euler0);
            m3 = &base[3].coord;
            Gp_MtxToEuler(m3, &euler1);
            work->field_5EC.vx = (u16)work->field_5EC.vx + ((euler0.vx - work->field_5EC.vx) >> 1);
            work->field_5EC.vy = (u16)work->field_5EC.vy + ((euler0.vy - work->field_5EC.vy) >> 1);
            work->field_5EC.vz = (u16)work->field_5EC.vz + ((euler0.vz - work->field_5EC.vz) >> 1);
            work->field_5F4.vx = (u16)work->field_5F4.vx + ((euler1.vx - work->field_5F4.vx) >> 1);
            work->field_5F4.vy = (u16)work->field_5F4.vy + ((euler1.vy - work->field_5F4.vy) >> 1);
            work->field_5F4.vz = (u16)work->field_5F4.vz + ((euler1.vz - work->field_5F4.vz) >> 1);
            ir                 = &rot.ident;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            ir->m11_m12        = 0x1000;
            rot.ident.m20_m21  = 0;
            ir->m22            = 0x1000;
            RotMatrix(&work->field_5EC, &rot.mat);
            Actor00400_Fn08A1C(&rot.mat, m2);
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            ir->m11_m12       = 0x1000;
            rot.ident.m20_m21 = 0;
            ir->m22           = 0x1000;
            RotMatrix(&work->field_5F4, &rot.mat);
            Actor00400_Fn08A1C(&rot.mat, m3);
        }
        base[2].flg = 0;
        base[3].flg = 0;
        base[4].flg = 0;
        Gp_UpdateCoord(c4);
        work->field_652 = 0;
    }
}

/// Per-frame callback of the marker task `Actor00400_SpawnMarker` starts: it
/// walks the marker up its stored view-space span, then decides whether the
/// marker should stop being drawn.
///
/// `hidden` is raised when either of the marker's two `GpRec18` slots reports
/// one of the three kinds 1/3/5, or when `func_800E0C10`'s push-back says the
/// marker is being crowded and the current stage/room is not one of the
/// exceptions. Once it is raised - or after 0x3C frames, or when
/// `Gp_StateF0.field_23` is set - the object's draw flags are cleared, the task's
/// state is bumped and the effect is spawned with kind 2 instead of 1.
///
/// `flg` is cleared through a scalar lvalue on purpose: written as a struct
/// member it is an in-struct MEM, and GCC 2.8.1's
/// `fixed_scalar_and_varying_struct_p` would then let the `Gp_StateF0.field_4` load
/// hoist above the store. See DECOMPILATION_LEARNINGS.md, "Struct-typing a
/// body changes GCC 2.8.1's aliasing".
void Actor00400_Fn02D48(Task* arg0)
{
    Actor100400MarkerWork* work;
    s32                    hidden;
    GpCoord*               coord;
    GpDeltaScratch         delta;
    s32                    mask;
    s32                    i;
    s32                    n;
    u16                    kind;

    hidden             = 0;
    work               = (Actor100400MarkerWork*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    *(u32*)&coord->flg = 0;
    kind               = 1;
    switch (Gp_StateF0.field_4) {
        case 0:
            work->field_60    += 1;
            work->field_5A    += 2;
            coord->coord.t[0] += work->field_58;
            coord->coord.t[1] += work->field_5A;
            coord->coord.t[2] += work->field_5C;
            if (Gp_FindRec18(work->recs, 0) != 0) {
                for (i = 0; i < 2; i++) {
                    switch (work->recs[i].key & 0xFFFF0000) {
                        case 0x10000:
                            hidden = 1;
                            break;
                        case 0x30000:
                            hidden = 1;
                            break;
                        case 0x50000:
                            hidden = 1;
                            break;
                    }
                }
            }
            n = func_800E0C10(work->recs, &delta, 2, &mask);
            if (n < 3) {
                if (n > 0) {
                    if (gGameSession->at4.loc.stage == 4 &&
                        (gGameSession->at4.loc.area == 0x21 || gGameSession->at4.loc.area == 0x2B ||
                         gGameSession->at4.loc.area == 0x2C || gGameSession->at4.loc.area == 0x2D ||
                         gGameSession->at4.loc.area == 0x22)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->at4.loc.stage == 5 &&
                               (gGameSession->at4.loc.area == 0xD || gGameSession->at4.loc.area == 0xE ||
                                gGameSession->at4.loc.area == 0x1B)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->at4.loc.area == 0x1E && gGameSession->at4.loc.stage == 5) {
                        if ((mask & 8) == 0) {
                            hidden = 1;
                        }
                    } else {
                        hidden = 1;
                    }
                }
            }
            Gp_ClearRec18Occupied(work->recs);
            if ((++arg0->killCountdown >= 0x3D) || (Gp_StateF0.field_23 != 0) || (hidden != 0)) {
                arg0->killCountdown = 0;
                work->obj.flags    &= 0x3FFF;
                kind                = 2;
                arg0->state        += 1;
            }
            Actor00400_Fn001AC(coord, work->field_60, kind, 0x1300);
            break;
    }
}

/// Same nearest-waypoint search as `Actor00400_Fn031A4`, but the winner is
/// stored into `field_56C` and then made current: the record `field_64A` used
/// to point at is cleared to kind 0 and the new one is marked kind 1.
void Actor00400_Fn02FF8(Task* arg0)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    Actor100400Record*         record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)   = head - 0x1C;
    scratch            = (Actor100400NearestScratch*)SCRATCH_HEAD(u8);
    work               = arg0->work;
    scratch->index     = 1;
    scratch->bestIndex = 0;
    scratch->best      = 0x7FFFFFFF;
    for (;;) {
        index  = scratch->index;
        record = (Actor100400Record*)(index * sizeof(Actor100400Record) + (u32)work->field_608);
        kind   = record->field_6;
        if (kind == -1) {
            goto done;
        }
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->field_0;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].field_4;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                work->field_56C.vx = work->field_608[scratch->index].field_0;
                work->field_56C.vz = work->field_608[scratch->index].field_4;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
    }
done:
    if (work->field_64A != scratch->bestIndex) {
        work->field_608[work->field_64A].field_6 = 0;
        work->field_64A                          = scratch->bestIndex;
        work->field_608[work->field_64A].field_6 = 1;
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Finds the nearest eligible waypoint record in `field_608` and returns its
/// XZ in `arg1`. Records with `field_6 == 1` are only considered when they are
/// the one `field_64A` points at, and the walk ends at the `-1` terminator.
void Actor00400_Fn031A4(Task* arg0, SVECTOR* arg1)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    Actor100400Record*         record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)   = head - 0x1C;
    scratch            = (Actor100400NearestScratch*)SCRATCH_HEAD(u8);
    work               = arg0->work;
    scratch->index     = 1;
    scratch->bestIndex = 0;
    scratch->best      = 0x7FFFFFFF;
loop:
    index  = scratch->index;
    record = (Actor100400Record*)(index * sizeof(Actor100400Record) + (u32)work->field_608);
    kind   = record->field_6;
    if (kind != -1) {
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->field_0;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].field_4;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                arg1->vx           = work->field_608[scratch->index].field_0;
                arg1->vz           = work->field_608[scratch->index].field_4;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
        goto loop;
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the four `corner` vertices through the view matrix and queues one
/// semi-transparent textured quad shaded grey `shade` (half intensity on red).
void Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade)
{
    Actor100400TextQuadScratch* s;
    POLY_FT4*                   poly;

    s                 = (Actor100400TextQuadScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor100400TextQuadScratch));
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    s->depth = RotTransPers4(corner0, corner1, corner2, corner3, &s->screen0, &s->screen1, &s->screen2, &s->screen3,
                             &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)poly + 0x28;
        setlen(poly, 9);
        poly->code       = 0x2E;
        *(s32*)&poly->x0 = s->screen0;
        *(s32*)&poly->x1 = s->screen1;
        *(s32*)&poly->x2 = s->screen2;
        *(s32*)&poly->x3 = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade >> 1, shade, shade);
        addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
    SCRATCH_POP_BYTES(sizeof(Actor100400TextQuadScratch));
}

/// Re-aim one joint by `yaw` about Y in world space: build the joint's
/// absolute rotation from its parent chain, turn it, then express the result
/// back in the parent's frame and write the 3x3 into the joint. The working
/// matrix is one 0x20-byte frame carved off the scratchpad head.
void Actor00400_Fn03570(GpCoord* coord, s16 yaw)
{
    MATRIX*  rotation;
    GpCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    Actor00400_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor00400_LocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/* The state tables below are defined among the functions, not with the other
   declarations, because `.rodata` follows source order: each sits between the
   jump tables of the functions around it. */

/// Kill-path states, indexed by `Task::state` in `Actor00400_Fn08004`.
const TaskFuncTable3 Actor00400_D0002C = { {
    Actor00400_Fn0A190,
    Actor00400_Fn02D48,
    Actor00400_Fn0A28C,
} };

/// The eight states `Actor00400_Fn08948` dispatches on `field_30`. The zero
/// word after it in the image is the alignment pad of
/// `Actor00400_Fn03920`'s jump table, not a terminator.
const TaskFuncTable8 Actor00400_D00038 = { {
    Actor00400_Fn03920,
    Actor00400_Fn04580,
    Actor00400_Fn04B48,
    Actor00400_Fn04E18,
    Actor00400_Fn040DC,
    Actor00400_Fn089C8,
    Actor00400_Fn06B7C,
    Actor00400_Fn070C0,
} };

/// Applies the row of `Actor00400_D15F20` that matches the session's current
/// area and room to the freshly allocated work block: the row can hide the root
/// coordinate, override the waypoint set the spawn argument selects, hand the
/// actor its record list and seed its height. Returns non-zero when the actor
/// does not belong in this room - no row matched, or the matching row's `flags`
/// bit 1 rejects it - which makes the entry state destroy the enemy instead.
static __inline__ s32 Actor00400_ApplyAreaConfig(Task* arg0)
{
    Actor100400AreaConfig* cfg;
    Actor100400Work*       work;
    GpAreaKey*             ses;
    u16                    flags;

    work = arg0->work;
    ses  = &gGameSession->at4.loc;
    cfg  = Actor00400_D15F20;
    while (cfg->area != 0xFF) {
        if ((ses->stage == cfg->area) && (ses->area == cfg->room)) {
            flags = cfg->flags;
            if (flags & 1) {
                return 1;
            }
            if (flags & 2) {
                work->field_661 = 1;
            }
            if (cfg->waypointSets != NULL) {
                work->field_60C = cfg->waypointSets[(arg0->spawnArg1 >> 4) & 3];
            }
            if (cfg->records != NULL) {
                work->field_608 = cfg->records;
            }
            if (cfg->height != NULL) {
                work->field_64E = *cfg->height;
            }
            return 0;
        }
        cfg++;
    }
    return 1;
}

/// Points the object at the second part coordinate of the model and clears its
/// pending-hit byte, then sets whether the root coordinate stays hidden. Shared
/// by the two spawn states that attach the actor to its head.
///
/// `hide` is `s32` rather than `u8` so its literal lands in a different CSE mode
/// class from the `1` the callers store into `field_664`, which is what makes
/// state 6 materialise the constant twice the way retail does.
static __inline__ void Actor00400_AttachHead(Task* arg0, GpEnemy* obj,
                                             Actor100400Work* work, s32 hide)
{
    obj->coord              = &arg0->extra.tmd->coords[1];
    obj->node.state.b.flags = 0;
    work->field_661         = hide;
}

void Actor00400_Fn03920(Task* arg0)
{
    Actor100400Work*     work;
    Actor100400Work*     w;
    Actor100400Work*     anim;
    GpEnemy*             obj;
    Actor100400QuadWork* quad;
    GpEnemy*             quadOwner;
    GpCoordPos*          pos;
    GpCoord*             coord;
    Task*                task;
    s32                  failed;
    s32                  nibble;
    s32                  index;
    u16                  y;
    s32*                 spawnArg;

    spawnArg              = &arg0->spawnArg1;
    gStageSceneMusicEntry = 0xB;
    obj                   = arg0->spawnArg2;
    coord                 = arg0->extra.tmd->coords;
    if ((*spawnArg >> 16) & 1) {
        Gp_DestroyEnemy(obj, arg0);
        return;
    }
    arg0->work = memCalloc(sizeof(Actor100400Work), 0);
    work       = arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(obj, arg0);
        return;
    }

    failed = Actor00400_ApplyAreaConfig(arg0);
    if (failed) {
        Gp_DestroyEnemy(obj, arg0);
        return;
    }

    if ((arg0->spawnArg1 & 0xF) == 0) {
        work->field_661 = 1;
    }
    Actor00400_Fn00B48(arg0);
    arg0->msgTable = &Actor00400_D16010;
    switch (arg0->spawnArg1 & 0xF) {
        case 7:
            work->field_658 = 0xC8;
            work->field_666 = 1;
            work->field_664 = 1;
            work->field_63E = (u16)work->field_64E;
            Actor00400_AttachHead(arg0, obj, work, 0);
            w            = arg0->work;
            w->field_632 = 0x10;
            w->field_628 = 0x10;
            w->field_624 = 2;
            w            = arg0->work;
            arg0->state  = 7;
            w->field_638 = 0;
            w->field_63A = 0;
            Gp_IncStateF0Ref(0);
            obj->hp = (s16)obj->hpMax / 8;
            break;
        case 6:
            work->field_666 = 0;
            work->field_664 = 1;
            Actor00400_AttachHead(arg0, obj, work, 1);
            w            = arg0->work;
            w->field_632 = 0x10;
            w->field_628 = 0xF;
            w->field_624 = 2;
            w            = arg0->work;
            arg0->state  = 6;
            w->field_638 = 0;
            w->field_63A = 0;
            Gp_IncStateF0Ref(0);
            obj->hp   = (s16)obj->hpMax / 8;
            pos       = (GpCoordPos*)arg0->extra.tmd->coords;
            quadOwner = arg0->spawnArg2;
            y         = pos->y;
            task      = Task_SpawnFromTable(&Actor00400_D16028, 2, 0, 0);
            if (task != NULL) {
                quad = memCalloc(sizeof(Actor100400QuadWork), 0);
                if (quad == NULL) {
                    taskKill(task);
                } else {
                    task->work           = (TaskIdMap*)quad;
                    quad->vertices[0].vx = pos->x - 0x5DC;
                    quad->vertices[0].vy = y;
                    quad->vertices[0].vz = pos->z - 0x5DC;
                    quad->vertices[1].vx = pos->x + 0x5DC;
                    quad->vertices[1].vy = y;
                    quad->vertices[1].vz = pos->z - 0x5DC;
                    quad->vertices[2].vx = pos->x - 0x5DC;
                    quad->vertices[2].vy = y;
                    quad->vertices[2].vz = pos->z + 0x5DC;
                    quad->vertices[3].vx = pos->x + 0x5DC;
                    quad->vertices[3].vy = y;
                    quad->vertices[3].vz = pos->z + 0x5DC;
                    quad->intensity      = 0xFF;
                    quad->field_0        = quadOwner;
                }
            }
            break;
        case 0:
            work->field_666 = 0;
            work->field_661 = 1;
            w               = arg0->work;
            w->field_632    = 0x10;
            w->field_628    = 2;
            w->field_624    = 2;
            arg0->state     = arg0->state + 1;
            break;
        case 4:
            work->field_666 = 1;
            nibble          = GameFlag_GetNibble(0xEB);
            if (nibble != 2) {
                obj->node.state.b.flags = 1;
                w                       = arg0->work;
                w->field_632            = 0x10;
                w->field_628            = 1;
                w->field_624            = 2;
                w                       = arg0->work;
                arg0->state             = 3;
                w->field_638            = 0;
                w->field_63A            = 0;
                w                       = arg0->work;
                w->field_638            = 0xD;
                w->field_63A            = 0;
            } else {
                w            = arg0->work;
                w->field_632 = 0x10;
                w->field_628 = 1;
                w->field_624 = nibble;
                w            = arg0->work;
                arg0->state  = 3;
                w->field_638 = 0;
                w->field_63A = 0;
            }
            break;
        case 5:
            work->field_666 = 1;
            nibble          = GameFlag_GetNibble(0xEB);
            if (nibble != 2) {
                obj->node.state.b.flags = 1;
                work->field_666         = 1;
                w                       = arg0->work;
                w->field_632            = 0x10;
                w->field_628            = 1;
                w->field_624            = 2;
                w                       = arg0->work;
                arg0->state             = 3;
                w->field_638            = 0;
                w->field_63A            = 0;
                w                       = arg0->work;
                w->field_638            = 0xE;
                w->field_63A            = 0;
            } else {
                w            = arg0->work;
                w->field_632 = 0x10;
                w->field_628 = 1;
                w->field_624 = nibble;
                w            = arg0->work;
                arg0->state  = 3;
                w->field_638 = 0;
                w->field_63A = 0;
            }
            break;
        case 1:
            if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 45, 0, 0)) {
                if (GameFlag_GetNibble(0xB7) == 0) {
                    work->field_666 = 1;
                    w               = arg0->work;
                    w->field_632    = 0x10;
                    w->field_628    = 1;
                    w->field_624    = 2;
                    w               = arg0->work;
                    arg0->state     = 3;
                    w->field_638    = 0;
                    w->field_63A    = 0;
                } else {
                    work->field_661   = 1;
                    w                 = arg0->work;
                    w->field_632      = 0x10;
                    w->field_628      = 2;
                    w->field_624      = 2;
                    work->field_666   = 0;
                    coord->coord.t[1] = 0;
                    w                 = arg0->work;
                    arg0->state       = 1;
                    w->field_638      = 0;
                    w->field_63A      = 0;
                }
            } else {
                work->field_666 = 1;
                w               = arg0->work;
                w->field_632    = 0x10;
                w->field_628    = 1;
                w->field_624    = 2;
                w               = arg0->work;
                arg0->state     = 3;
                w->field_638    = 0;
                w->field_63A    = 0;
            }
            break;
        case 3:
            work->field_666 = 1;
            w               = arg0->work;
            w->field_632    = 0x10;
            w->field_628    = 3;
            w->field_624    = 2;
            Actor00400_Fn02FF8(arg0);
            coord->coord.t[0] = work->field_56C.vx;
            work->field_63E   = (u16)work->field_64E;
            coord->coord.t[1] = work->field_56C.vy + work->field_64E + 0x7D0;
            coord->coord.t[2] = work->field_56C.vz;
            Gp_IncStateF0Ref(0);
            w            = arg0->work;
            arg0->state  = 3;
            w->field_638 = 0;
            w->field_63A = 0;
            w            = arg0->work;
            w->field_638 = 4;
            w->field_63A = 0;
            break;
        case 2:
            work->field_666 = 1;
            work->field_65F = 1;
            w               = arg0->work;
            w->field_632    = 0x10;
            w->field_628    = 3;
            w->field_624    = 2;
            w               = arg0->work;
            arg0->state     = 3;
            w->field_638    = 0;
            w->field_63A    = 0;
            if ((arg0->spawnArg1 & 0xF0) == 0) {
                w            = arg0->work;
                w->field_638 = 0xC;
                w->field_63A = 0;
            } else {
                w            = arg0->work;
                w->field_638 = 0xB;
                w->field_63A = 0;
            }
            break;
    }

    anim = arg0->work;
    if (anim->field_624 == 1) {
        if (anim->field_626 != anim->field_628) {
            anim->field_62A = 0;
        } else {
            anim->field_62A = Actor00400_Fn086FC(arg0, anim->field_62A);
        }
        Actor00400_Fn08624(arg0);
        anim->field_624 = 3;
    } else if (anim->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        anim->field_624 = 3;
        anim->field_62A = 0;
    } else if (anim->field_624 == 3) {
        anim->field_62A = (u16)anim->field_62A + 1;
    }
    for (index = 1; index < 0xF; index++) {
        Gp_AnimTickIndex((GpAnimCtx*)anim, index);
    }
    work->field_620 = 0x1000;
    work->field_622 = 0x1000;
}

/// Colours the actor from the second attach coordinate of its model through a
/// 0x10-byte `VECTOR` taken off `G_SCRATCH_HEAD`, then hides the root
/// coordinate while `field_65F` is set.
static __inline__ void Actor00400_UpdateColor(Task* arg0, GpCoord* coord,
                                              Actor100400Work* work, TmdObject* ctx)
{
    VECTOR* block = (VECTOR*)(SCRATCH_HEAD(u8) - 0x10);

    block->vx            = coord->workm.t[0];
    block->vy            = coord->workm.t[1];
    block->vz            = coord->workm.t[2];
    SCRATCH_HEAD(VECTOR) = block;
    Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
    if (work->field_65F != 0) {
        Gp_SetObjTrans(ctx, 0, 0, 0);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// States `Actor00400_Fn040DC` dispatches on `Actor100400Work.field_638`.
const TaskFuncTable11 Actor00400_D0007C = { {
    Actor00400_Fn042C0,
    Actor00400_Fn04414,
    Actor00400_Fn07CC4,
    Actor00400_Fn07DE0,
    Actor00400_Fn07E20,
    Actor00400_Fn07E74,
    Actor00400_Fn07EE8,
    Actor00400_Fn07F18,
    Actor00400_Fn07F44,
    Actor00400_Fn07F88,
    Actor00400_Fn07FEC,
} };

/// Per-frame callback for the text actor's second task. Same frame gate as
/// `Actor00400_Fn04B48`: `Gp_StateF0.field_4` 2 only flags the model hidden, 0 runs
/// this frame's state handler before falling through to the draw half, and 1
/// is the draw half on its own.
void Actor00400_Fn040DC(Task* arg0)
{
    TaskFuncTable11  fns;
    Actor100400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    Actor100400Work* work2;
    GpCoord*         coord;
    s32              y;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    fns   = Actor00400_D0007C;
    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638](arg0);
            work->flags_62C.half = work->slots[1].flags;
            if (work->field_644 != 4) {
                work->field_660 = 1;
                Actor00400_Fn02648(arg0, 1);
            }
            y                 = coord->coord.t[1];
            coord->coord.t[1] = y + ((work->field_63E + (s16)work->field_658 - y) >> 4);
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            break;
    }
}

void Actor00400_Fn042C0(Task* arg0)
{
    Actor100400Work* work;
    GpEnemy*         obj;
    s32              id;

    work = arg0->work;
    obj  = arg0->spawnArg2;
    if (arg0->extra.tmd->coords->coord.t[1] - work->field_64E < 0x320) {
        work->field_63E = work->field_64E;
    }
    Gp_UnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    obj->recs = NULL;
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    Gp_UnlinkObj(&work->obj_42C);
    work->field_636 = 0;
    if (work->field_644 == 4) {
        Actor100400Work* w = arg0->work;
        w->field_638       = 7;
        w->field_63A       = 0;
        return;
    }
    if (arg0->spawnArg1 != 7) {
        Actor100400Work* w;
        id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        w            = arg0->work;
        w->field_632 = 0x30;
        w->field_63C = 4;
        w->field_628 = 1;
        w->field_624 = 1;
    }
    work->field_638++;
}

void Actor00400_Fn04414(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    Actor100400Work* w2;
    s32              i;
    s32              cond;

    work = arg0->work;
    w    = arg0->work;
    if (w->field_624 == 1) {
        if (w->field_626 != w->field_628) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->field_624 = 3;
    } else if (w->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        w->field_624 = 3;
        w->field_62A = 0;
    } else if (w->field_624 == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)w, i);
        i++;
    } while (i < 0xF);
    if (arg0->spawnArg1 != 7) {
        w2 = arg0->work;
        if ((w2->flags_62C.half & 1) || (w2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond == 0) {
            return;
        }
        w2            = arg0->work;
        w2->field_63C = 4;
        w2->field_632 = 0x10;
        w2->field_628 = 0xE;
        w2->field_624 = 1;
    }
    work->field_638++;
}

/// States `Actor00400_Fn04580` dispatches on `Actor100400Work.field_638`.
const TaskFuncTable10 Actor00400_D000A8 = { {
    Actor00400_Fn090B4,
    Actor00400_Fn09124,
    Actor00400_Fn091F8,
    Actor00400_Fn09260,
    Actor00400_Fn092D4,
    Actor00400_Fn09348,
    Actor00400_Fn093BC,
    Actor00400_Fn093C4,
    Actor00400_Fn09418,
    Actor00400_Fn0946C,
} };

void Actor00400_Fn04580(Task* arg0)
{
    Actor100400Work* work = arg0->work;
    GpEnemy*         obj  = arg0->spawnArg2;
    TmdObject*       ctx  = arg0->extra.tmd;
    TaskFuncTable10  fns;
    Actor100400Mat   m;
    GpMtxWords*      ia;
    Actor100400Work* w;
    Actor100400Work* w2;
    Actor100400Work* w3;
    Actor100400Work* work2;
    TmdObject*       ctx2;
    GpCoord*         coord;
    MATRIX*          dst;
    s32              i;

    fns = Actor00400_D000A8;
    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->field_638](arg0);
            Actor00400_Fn00A14(arg0);
            w = arg0->work;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex((GpAnimCtx*)w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            Actor00400_Fn016A4(arg0, (u8)work->field_665);
            w2              = arg0->work;
            coord           = arg0->extra.tmd->coords;
            ia              = &m.ident;
            m.ident.m00_m01 = 0x1000;
            m.ident.m02_m10 = 0;
            ia->m11_m12     = 0x1000;
            m.ident.m20_m21 = 0;
            ia->m22         = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 2;
                w3->field_638 = 0;
                w3->field_63A = 0;
            }
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~0x80;
            break;
    }
}

void Actor00400_Fn04900(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_632 = 0x10;
        work->field_628 = 0xC;
        work->field_624 = 2;
        id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

void Actor00400_Fn04A1C(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 2) {
        id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work2            = arg0->work;
        work2->field_63C = 6;
        work2->field_632 = 0x10;
        work2->field_628 = 0xD;
        work2->field_624 = 1;
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

/// States `Actor00400_Fn04B48` dispatches on `Actor100400Work.field_638`.
const TaskFuncTable10 Actor00400_D000D0 = { {
    Actor00400_Fn04CF8,
    Actor00400_Fn08C54,
    Actor00400_Fn08D70,
    Actor00400_Fn08DFC,
    Actor00400_Fn08E50,
    Actor00400_Fn08FB0,
    Actor00400_Fn08FC8,
    Actor00400_Fn08FF4,
    Actor00400_Fn09038,
    Actor00400_Fn0909C,
} };

/// Per-frame callback for the main actor task. `Gp_StateF0.field_4` gates the frame:
/// 2 only flags the model hidden, 0 runs this frame's state handler before
/// falling through to the draw half, and 1 is the draw half on its own.
void Actor00400_Fn04B48(Task* arg0)
{
    TaskFuncTable10  fns;
    Actor100400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    Actor100400Work* work2;
    GpCoord*         coord;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    fns  = Actor00400_D000D0;
    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638](arg0);
            work->flags_62C.half = work->slots[1].flags;
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            coord = &ctx2->coords[1];
            Actor00400_UpdateColor(arg0, coord, work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], (u8)work->field_648);
            break;
    }
}

void Actor00400_Fn04CF8(Task* arg0)
{
    Actor100400Work* work;
    GpEnemy*         obj;
    s32              id;
    Actor100400Work* w;

    work      = arg0->work;
    obj       = arg0->spawnArg2;
    obj->recs = NULL;
    Gp_UnlinkObj(&work->obj_42C);
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    Gp_UnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    work->field_648 = 0x80;
    if (work->field_644 == 4) {
        w            = arg0->work;
        w->field_638 = 6;
        w->field_63A = 0;
        return;
    }
    w               = arg0->work;
    w->field_63C    = 8;
    w->field_632    = 0x10;
    w->field_628    = 0xF;
    w->field_624    = 1;
    work->field_636 = 0;
    id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                        (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_638++;
}

/// States `Actor00400_Fn04E18` dispatches on `Actor100400Work.field_638`.
const TaskFuncTable15 Actor00400_D000F8 = { {
    Actor00400_Fn07738,
    Actor00400_Fn077F4,
    Actor00400_Fn05728,
    Actor00400_Fn078C8,
    Actor00400_Fn0793C,
    Actor00400_Fn07998,
    Actor00400_Fn079A0,
    Actor00400_Fn079A8,
    Actor00400_Fn079FC,
    Actor00400_Fn07ABC,
    Actor00400_Fn07B10,
    Actor00400_Fn07B98,
    Actor00400_Fn07C04,
    Actor00400_Fn09C04,
    Actor00400_Fn09C84,
} };

/// Per-frame callback for the boss task. Same `Gp_StateF0.field_4` frame gate as
/// `Actor00400_Fn04580`, with the model's Y bobbed by two `rsin` terms and the
/// display object re-pointed at the part coordinate `field_664` selects; the
/// tail hides the model again while the session sits in the two area-0xA/0xB
/// rooms of area 0x21.
void Actor00400_Fn04E18(Task* arg0)
{
    Actor100400Work* work   = arg0->work;
    GpCoord*         coord0 = arg0->extra.tmd->coords;
    GpEnemy*         obj    = arg0->spawnArg2;
    TmdObject*       ctx    = arg0->extra.tmd;
    TaskFuncTable15  fns;
    Actor100400Mat   m;
    GpMtxWords*      ia;
    Actor100400Work* w;
    Actor100400Work* wA;
    Actor100400Work* w2;
    Actor100400Work* w3;
    Actor100400Work* w4;
    Actor100400Work* work2;
    GpEnemy*         obj2;
    TmdObject*       ctx2;
    TmdObject*       ctx3;
    TmdObject*       ctxN;
    GpAreaKey*       sess;
    GpCoord*         coord;
    GpCoord*         coordN;
    MATRIX*          dst;
    s32              i;

    fns = Actor00400_D000F8;
    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            if (work->field_663 != 0) {
                return;
            }
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->field_638](arg0);
            Actor00400_Fn00A14(arg0);
            wA = arg0->work;
            if (wA->field_64C != 0) {
                wA->field_64C--;
            }
            w = arg0->work;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex((GpAnimCtx*)w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            Actor00400_Fn02648(arg0, work->field_660);
            w2              = arg0->work;
            coord           = arg0->extra.tmd->coords;
            ia              = &m.ident;
            m.ident.m00_m01 = 0x1000;
            m.ident.m02_m10 = 0;
            ia->m11_m12     = 0x1000;
            m.ident.m20_m21 = 0;
            ia->m22         = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 4;
                w3->field_638 = 0;
                w3->field_63A = 0;
            }
            coord0->coord.t[1] += (work->field_63E - coord0->coord.t[1]) >> 4;
            if (work->field_638 < 0xB) {
                coord0->coord.t[1] += (rsin(work->flags_62C.hi.field_62E << 6) * 0x10) >> 12;
            }
            if (work->field_650 != 0) {
                work->field_650--;
                coord0->coord.t[1] += (rsin(work->field_630 << 0xA) * 0x10) >> 0xA;
            }
            obj->coord = &arg0->extra.tmd->coords[work->field_664];
            if (work->field_638 < 0xB) {
                w4       = arg0->work;
                ctxN     = arg0->extra.tmd;
                obj2     = arg0->spawnArg2;
                coordN   = &ctxN->coords[w4->field_664];
                m.vec.vx = 0;
                m.vec.vy = 0;
                m.vec.vz = 0;
                Actor00400_Fn0A08C(coordN, &m.vec);
                if (w4->field_64E + 0x190 < m.vec.vy) {
                    obj2->node.state.b.flags = 1;
                } else {
                    obj2->node.state.b.flags = 0;
                }
            }
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~0x80;
            break;
    }
    sess = &gGameSession->at4.loc;
    ctx3 = arg0->extra.tmd;
    if (sess->stage == 4 && sess->area == 0x21 && (u32)(gGameSession->at4.loc.view - 0xA) < 2U) {
        ctx3->flags |= 0x80;
    }
}

static inline void Actor00400_TurnToward(Task* arg0, SVECTOR* target, s32 step, s32 range)
{
    Actor100400Work* work = arg0->work;
    GpCoord*         coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords      = arg0->extra.tmd->coords;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > range) {
        work->field_556 = angle - step;
    } else if (diff < -range) {
        work->field_556 = angle + step;
    }
}

/// Spawns the 16-way ring of `0x01202148` effects the boss uses when it lands
/// and when it is knocked down: one per 1/16 turn, at the height `field_64E`
/// gives above the root coordinate.
static inline void Actor00400_SpawnRing(Task* arg0, Actor100400Work* work, GpCoord* coord)
{
    GpCoord* coord2;
    SVECTOR  vec;
    s32      i;
    s16      y;

    i      = 0;
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
}

void Actor00400_Fn05320(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w1;
    Actor100400Work* w2;
    Actor100400Work* w4;
    Actor100400Work* w5;
    GpCoord*         coord;
    s8               armed;
    s32              cond;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              sound3;
    s32              pan3;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_636++;
    if (work->field_636 >= 0x14) {
        w1    = arg0->work;
        armed = 0;
        if (w1->field_640 < 0xDAC && (u32)(w1->field_634 - 0x600) >= 0x400U) {
            Gp_StateF0.field_3 = 1;
            Gp_ArmStateF0(1);
            armed         = 1;
            w2            = arg0->work;
            w2->field_638 = 4;
            w2->field_63A = 0;
        }
        if (armed) {
            return;
        }
        Actor00400_TurnToward(arg0, (SVECTOR*)&work->field_60C[work->field_65B & 7], 0x20, 0x30);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040007;
        pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->field_636 == 0x14) {
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040004;
        pan2   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    w4 = arg0->work;
    if ((w4->flags_62C.half & 1) || (w4->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        Actor00400_SpawnRing(arg0, work, coord);
        sound3 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040008;
        pan3   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound3, pan3, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
        w5              = arg0->work;
        w5->field_63C   = 4;
        w5->field_632   = 0x10;
        w5->field_628   = 1;
        w5->field_624   = 1;
        work->field_63A++;
    }
}

void Actor00400_Fn05728(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* state;
    Actor100400Work* state2;
    Actor100400Work* state3;
    u32              random;
    s16              next;
    u8               idx;
    u8               idx2;

    work = arg0->work;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (work->field_640 < 0x2710 && (u32)(work->field_634 - 0xC0) >= 0xE81U) {
            random      = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = random;
            if ((random >> 16) & 1) {
                work2                              = arg0->work;
                work2->field_638                   = 0xA;
                work2->field_63A                   = 0;
                work2->field_614[work2->field_65A] = work2->field_638;
                next                               = 4;
                if (work2->field_614[0] == work2->field_614[1] &&
                    work2->field_614[0] == work2->field_614[2] && work2->field_614[0] == 0xA) {
                    state                              = arg0->work;
                    state->field_638                   = next;
                    state->field_63A                   = 0;
                    work2->field_614[work2->field_65A] = next;
                    work2->field_64C                   = 0x5A;
                }
                idx              = work2->field_65A + 1;
                work2->field_65A = idx;
                if (idx >= 3U) {
                    work2->field_65A = 0;
                }
            } else {
                work->field_64C   = 0x5A;
                state2            = arg0->work;
                state2->field_638 = 4;
                state2->field_63A = 0;
            }
        } else {
            state3                           = arg0->work;
            state3->field_638                = 4;
            state3->field_63A                = 0;
            work->field_614[work->field_65A] = work->field_638;
            idx2                             = work->field_65A + 1;
            work->field_65A                  = idx2;
            if (idx2 >= 3U) {
                work->field_65A = 0;
            }
        }
    }
}

void Actor00400_Fn058C4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* state;
    Actor100400Work* state2;
    GpCoord*         coord;
    SVECTOR          delta;
    SVECTOR          vec2;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s16              next;
    u8               idx;
    u8               idx2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_636++;
    coord->coord.t[0] += (work->field_574.vx - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += (work->field_574.vz - coord->coord.t[2]) >> 4;
    work->field_63E    = work->field_64E + 0x64;
    if (work->field_636 == 8) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040007;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->field_636 == 0x14) {
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040004;
        pan2   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 < 0x15) {
        return;
    }
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x18, 0x30);
    if (work->field_636 < 0x1F) {
        return;
    }
    if (work->field_640 < 0x2710 && (u32)(work->field_634 - 0xC0) >= 0xE81U) {
        work2                              = arg0->work;
        work2->field_638                   = 0xA;
        work2->field_63A                   = 0;
        work2->field_614[work2->field_65A] = work2->field_638;
        next                               = 4;
        if (work2->field_614[0] == work2->field_614[1] &&
            work2->field_614[0] == work2->field_614[2] && work2->field_614[0] == 0xA) {
            state                              = arg0->work;
            state->field_638                   = next;
            state->field_63A                   = 0;
            work2->field_614[work2->field_65A] = next;
            work2->field_64C                   = 0x5A;
        }
        idx              = work2->field_65A + 1;
        work2->field_65A = idx;
        if (idx >= 3U) {
            work2->field_65A = 0;
        }
    } else {
        vec2.vx = vec2.vy = vec2.vz = 0;
        Actor00400_Fn031A4(arg0, &vec2);
        delta.vx = vec2.vx - coord->coord.t[0];
        delta.vy = 0;
        delta.vz = vec2.vz - coord->coord.t[2];
        if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) >= 0xDAC) {
            Actor00400_Fn02FF8(arg0);
            state2            = arg0->work;
            state2->field_638 = 4;
            state2->field_63A = 0;
        }
        work->field_614[work->field_65A] = work->field_638;
        idx2                             = work->field_65A + 1;
        work->field_65A                  = idx2;
        if (idx2 >= 3U) {
            work->field_65A = 0;
        }
    }
}

void Actor00400_Fn05D00(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    GpCoord*         coord;
    GpCoord*         coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    coord           = arg0->extra.tmd->coords;
    work            = arg0->work;
    work->field_660 = 1;
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    Gp_SetLightMode(arg0->spawnArg2, 2);
    if (work->field_628 != 3) {
        i      = 0;
        y      = work->field_64E - coord->coord.t[1] + 0xFA;
        coord2 = arg0->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 8) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 8) >> 3;
            Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
            i++;
        } while (i < 16);
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040008;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        w            = arg0->work;
        w->field_63C = 4;
        w->field_632 = 0x10;
        w->field_628 = 1;
        w->field_624 = 1;
    } else {
        work->field_63A++;
    }
    work->field_63A++;
}

void Actor00400_Fn05EA4(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    SVECTOR          vec;
    s32              id;
    s32              pan;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Actor00400_Fn02FF8(arg0);
    vec.vx          = work->field_56C.vx - coord->coord.t[0];
    vec.vy          = 0;
    vec.vz          = work->field_56C.vz - coord->coord.t[2];
    work->field_63E = work->field_60C[work->field_65B].field_2 + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 800 && work->field_64C == 0) {
        Actor100400Work* w;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        work->field_636 = 0;
        w               = arg0->work;
        w->field_638    = 3;
        w->field_63A    = 0;
        return;
    }
    if (work->field_628 != 3) {
        Actor100400Work* w;
        w            = arg0->work;
        w->field_63C = 10;
        w->field_632 = 0x10;
        w->field_628 = 3;
        w->field_624 = 1;
    }
    Actor00400_TurnToward(arg0, &work->field_56C, 0x30, 0x100);
    Actor00400_Fn0762C(arg0, 0x60, work->field_556);
    if (!(work->field_630 & 0xF)) {
        id  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040001;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
}

void Actor00400_Fn060CC(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_632 = 0x20;
        work->field_628 = 0xA;
        work->field_624 = 2;
        id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->field_63A = 0;
        }
    }
}

void Actor00400_Fn061E8(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    GpCoord*         coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              i;
    s16              y;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_63C = 3;
    work->field_632 = 0x10;
    work->field_628 = 0xB;
    work->field_624 = 1;
    sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    i               = 0;
    pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
    sound2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040008;
    pan2   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_63A++;
}

void Actor00400_Fn06380(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;

    work = arg0->work;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 1) {
        work->field_646 = 0x18;
    }
    if (work->field_636 >= 0x24) {
        work->field_636 = 0;
        w               = arg0->work;
        w->field_63C    = 8;
        w->field_632    = 0x10;
        w->field_628    = 7;
        w->field_624    = 1;
        work->field_63A++;
    }
}

/// Spawns the marker task from `Actor00400_D16028[1]` and hands it a 0x64-byte
/// work block: coordinate 5 gives the task's root translation, and the view
/// space span from coordinate 4's base to the same point raised by `height` -
/// the per-enemy value `Actor00400_D1609C` selects - is stored in the work.
static inline void Actor00400_SpawnMarker(Task* arg0)
{
    GpAreaPlace*           params;
    Actor100400MarkerWork* marker;
    GpCoord*               coords;
    GpCoord*               origin;
    GpCoord*               span;
    GpCoord*               dst;
    Task*                  task;
    SVECTOR                pos;
    SVECTOR                base;
    SVECTOR                tip;
    u16                    height;

    params = ((GpEnemy*)arg0->spawnArg2)->place;
    if (params != NULL) {
        height = Actor00400_D1609C[params->rowIndex & 7];
    } else {
        height = 0xBE;
    }
    coords = arg0->extra.tmd->coords;
    origin = &coords[5];
    span   = &coords[4];
    task   = Task_SpawnFromTable(&Actor00400_D16028, 1, 0, 0);
    if (task != NULL) {
        marker = memCalloc(sizeof(Actor100400MarkerWork), false);
        if (marker == NULL) {
            taskKill(task);
        } else {
            base.vx = 0;
            base.vy = 0;
            base.vz = 0;
            tip.vx  = 0;
            tip.vy  = 0;
            tip.vz  = height;
            Actor00400_Fn0A08C(span, &base);
            Actor00400_Fn0A08C(span, &tip);
            task->work = (TaskIdMap*)marker;
            dst        = task->extra.tmd->coords;
            pos.vx     = 0;
            pos.vy     = 0;
            pos.vz     = 0;
            Actor00400_Fn0A08C(origin, &pos);
            dst->coord.t[0]  = pos.vx;
            dst->coord.t[1]  = pos.vy;
            dst->coord.t[2]  = pos.vz;
            marker->field_58 = tip.vx - base.vx;
            marker->field_5A = tip.vy - base.vy;
            marker->field_5C = tip.vz - base.vz;
        }
    }
}

void Actor00400_Fn064B0(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              pan;
    s32              cond;

    work = arg0->work;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 0x29) {
        id  = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x4004000A;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0x2B) {
        Actor00400_SpawnMarker(arg0);
    }
    work2 = arg0->work;
    if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = arg0->work;
        work2->field_638 = 2;
        work2->field_63A = 0;
    }
}

void Actor00400_Fn06798(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = work->field_60C[work->field_65B].field_0 - coord->coord.t[0];
    vec.vy = work->field_60C[work->field_65B].field_2 - coord->coord.t[1];
    vec.vz = work->field_60C[work->field_65B].field_4 - coord->coord.t[2];

    work->field_63E = work->field_60C[work->field_65B].field_2;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 1000) {
        work->field_65B = (work->field_65B + 1) & 7;
        return;
    }
    if (work->field_628 != 3) {
        Actor100400Work* w;
        Actor100400Work* a;
        s32              i;

        w            = arg0->work;
        w->field_63C = 10;
        w->field_632 = 0x10;
        w->field_628 = 3;
        w->field_624 = 1;

        a = arg0->work;
        if (a->field_624 == 1) {
            if (a->field_626 != a->field_628) {
                a->field_62A = 0;
            } else {
                a->field_62A = Actor00400_Fn086FC(arg0, a->field_62A);
            }
            Actor00400_Fn08624(arg0);
            a->field_624 = 3;
        } else if (a->field_624 == 2) {
            Actor00400_Fn085B8(arg0);
            a->field_624 = 3;
            a->field_62A = 0;
        } else if (a->field_624 == 3) {
            a->field_62A++;
        }
        i = 1;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)a, i);
            i++;
        } while (i < 0xF);
    }
    Actor00400_TurnToward(arg0, (SVECTOR*)&work->field_60C[work->field_65B], 0x2C, 0x100);
    Actor00400_Fn0762C(arg0, 0x60, work->field_556);
    Gp_SetLightMode(arg0->spawnArg2, 2);
}

void Actor00400_Fn06A44(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              id;
    s32              pan;

    work = arg0->work;
    work->field_636++;
    if (work->field_636 == 1) {
        SndEvt_EnqueueType6(((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x54220005, 0, 0);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040007;
        pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0x14) {
        work->field_646 = 0x18;
        w               = arg0->work;
        w->field_63C    = 4;
        w->field_632    = 0x10;
        w->field_628    = 7;
        w->field_624    = 1;
        work->field_636 = 0;
        work->field_63A++;
    }
}

/// Per-frame callback for the text actor's third task, with the same
/// `Gp_StateF0.field_4` frame gate as `Actor00400_Fn04B48`: 2 only flags the model
/// hidden, 0 runs this frame's state handler and rebuilds the root rotation
/// before falling through to the draw half, and 1 is the draw half on its own.
void Actor00400_Fn06B7C(Task* arg0)
{
    Actor100400Work* work             = arg0->work;
    GpEnemy*         obj              = arg0->spawnArg2;
    TmdObject*       ctx              = arg0->extra.tmd;
    void             (*fns[2])(Task*) = { Actor00400_Fn08A88, Actor00400_Fn08B40 };
    Actor100400Mat   m;
    GpMtxWords*      ia;
    Actor100400Work* w;
    Actor100400Work* w2;
    Actor100400Work* w3;
    Actor100400Work* work2;
    TmdObject*       ctx2;
    GpCoord*         coord;
    MATRIX*          dst;
    s32              i;

    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns[work->field_638](arg0);
            w = arg0->work;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex((GpAnimCtx*)w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            w2                   = arg0->work;
            coord                = arg0->extra.tmd->coords;
            ia                   = &m.ident;
            m.ident.m00_m01      = 0x1000;
            m.ident.m02_m10      = 0;
            ia->m11_m12          = 0x1000;
            m.ident.m20_m21      = 0;
            ia->m22              = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 2;
                w3->field_638 = 0;
                w3->field_63A = 0;
            }
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~0x80;
            break;
    }
}

static inline s32 Actor00400_ConsumeStateRequest(Actor100400Work* work)
{
    s16 req;
    s32 state;

    state = work->field_642;
    if (state != 1) {
        return 0;
    }
    req = work->field_644;
    if (req == 1)
        goto set;
    if (req == 2)
        goto set;
    if (req == 3)
        goto set;
    if (req != 4)
        goto other;
set:
    /* The do/while(0) is load-bearing: flow.c weights REG_N_REFS by loop
       depth, and the two extra references it buys `work` are what let the
       pointer outrank `req` in global.c's allocation order. */
    do {
        work->field_638 = state;
        work->field_63A = 0;
    } while (0);
other:
    work->field_644 = 0;
    return 1;
}

void Actor00400_Fn06EA4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              cond;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->work;
        if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_63C = 8;
            work2->field_632 = 4;
            work2->field_628 = 0xF;
            work2->field_624 = 1;
        }
    }
}

void Actor00400_Fn06F64(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_63C = 2;
        work->field_632 = 0x10;
        work->field_628 = 0x13;
        work->field_624 = 1;
        id              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(arg0->extra.tmd->coords),
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        return;
    }
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->work;
        if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 0;
            work2->field_63A = 0;
        }
    }
}

void Actor00400_Fn0A468(Task* arg0);
void Actor00400_Fn0A4BC(Task* arg0);

/// Per-frame callback for the text actor's fourth task. Same frame gate as
/// `Actor00400_Fn06B7C`, but the draw half only recolours the actor: case 0
/// runs this frame's state handler, lerps the root coordinate's height a
/// sixteenth of the way towards `field_63E` and falls through.
void Actor00400_Fn070C0(Task* arg0)
{
    Actor100400Work* work             = arg0->work;
    TmdObject*       ctx              = arg0->extra.tmd;
    GpEnemy*         obj              = arg0->spawnArg2;
    GpCoord*         coord0           = ctx->coords;
    void             (*fns[2])(Task*) = { Actor00400_Fn0A468, Actor00400_Fn0A4BC };
    Actor100400Mat   m;
    GpMtxWords*      ia;
    Actor100400Work* w;
    Actor100400Work* w2;
    Actor100400Work* w3;
    Actor100400Work* work2;
    TmdObject*       ctx2;
    GpCoord*         coord;
    MATRIX*          dst;
    s32              i;

    switch (Gp_StateF0.field_4) {
        case 2:
            ctx->flags |= 0x80;
            break;
        case 0:
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns[work->field_638](arg0);
            w = arg0->work;
            if (w->field_624 == 1) {
                if (w->field_626 != w->field_628) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->field_624 = 3;
            } else if (w->field_624 == 2) {
                Actor00400_Fn085B8(arg0);
                w->field_624 = 3;
                w->field_62A = 0;
            } else if (w->field_624 == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                Gp_AnimTickIndex((GpAnimCtx*)w, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            w2                   = arg0->work;
            coord                = arg0->extra.tmd->coords;
            ia                   = &m.ident;
            m.ident.m00_m01      = 0x1000;
            m.ident.m02_m10      = 0;
            ia->m11_m12          = 0x1000;
            m.ident.m20_m21      = 0;
            ia->m22              = 0x1000;
            RotMatrixZ(w2->field_558, &m.mat);
            func_8004BFF8(w2->field_556, &m.mat);
            dst          = &coord->coord;
            dst->m[0][0] = m.mat.m[0][0];
            dst->m[0][1] = m.mat.m[0][1];
            dst->m[0][2] = m.mat.m[0][2];
            dst->m[1][0] = m.mat.m[1][0];
            dst->m[1][1] = m.mat.m[1][1];
            dst->m[1][2] = m.mat.m[1][2];
            dst->m[2][0] = m.mat.m[2][0];
            dst->m[2][1] = m.mat.m[2][1];
            dst->m[2][2] = m.mat.m[2][2];
            coord->flg   = 0;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 4;
                w3->field_638 = 0;
                w3->field_63A = 0;
            }
            coord0->coord.t[1] += (work->field_63E - coord0->coord.t[1]) >> 4;
            /* fallthrough */
        case 1:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~0x80;
            break;
    }
}

void Actor00400_Fn07400(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* work3;
    s32              phase;
    s32              cond;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 10) * 0x10) >> 10));
        work2           = arg0->work;
        if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work3            = arg0->work;
            work3->field_63C = 8;
            work3->field_632 = 2;
            work3->field_628 = 0x10;
            work3->field_624 = 1;
        }
    }
}

void Actor00400_Fn07518(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              phase;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->field_63C = 2;
        work->field_632 = 0x10;
        work->field_628 = 0x12;
        work->field_624 = 1;
    }
    if (Actor00400_ConsumeStateRequest(arg0->work) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 9) * 0x10) >> 9));
        if (work->field_636 >= 0x79) {
            work2            = arg0->work;
            work2->field_638 = 0;
            work2->field_63A = 0;
        }
    }
}

/// Steps the actor's root coordinate `arg0->field_2C->field_8` along the
/// heading `arg2` in the XZ plane by `arg1` units and marks it dirty.
///
/// `coord.t[0]` gains `rsin(arg2) * arg1` and `coord.t[2]` `rcos(arg2) * arg1`;
/// the `<< 4` on the trig result and the `>> 16` after the multiply are one
/// `>> 12` split in two. Clearing `flg` is what makes `GsGetLw` rebuild the
/// matrix from `coord`.
void Actor00400_Fn0762C(Task* arg0, s16 arg1, s16 arg2)
{
    arg0->extra.tmd->coords->coord.t[0] += ((rsin(arg2) << 4) * arg1) >> 16;
    arg0->extra.tmd->coords->coord.t[2] += ((rcos(arg2) << 4) * arg1) >> 16;
    arg0->extra.tmd->coords->flg         = 0;
}

void Actor00400_Fn0A2F4(Task* arg0);
void Actor00400_Fn0A364(Task* arg0);

/// Two-state dispatcher over a handler table built on the stack.
void Actor00400_Fn076E8(Task* task)
{
    TaskFunc funcs[2] = {
        Actor00400_Fn0A2F4,
        Actor00400_Fn0A364,
    };

    funcs[task->state](task);
}

/// Draws two LCG values into the work's `field_62E`/`field_630`, resets the
/// state counters and copies the root coordinate's `t[1]` into `field_63E`.
void Actor00400_Fn07738(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    GpCoord*         coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Gp_IncStateF0Ref(0);
    Gp_LcgState                  = Gp_LcgState * 5 + 0x71357911;
    work->flags_62C.hi.field_62E = Gp_LcgState >> 16;
    Gp_LcgState                  = Gp_LcgState * 5 + 0x71357911;
    work->field_630              = Gp_LcgState >> 16;
    state                        = arg0->work;
    state->field_632             = 0x10;
    state->field_628             = 1;
    state->field_624             = 2;
    state2                       = arg0->work;
    state2->field_638            = 1;
    state2->field_63A            = 0;
    work->field_63E              = coord->coord.t[1];
}

/// States `Actor00400_Fn077F4` dispatches on `Actor100400Work.field_63A`.
const TaskFuncTable4 Actor00400_D00134 = { {
    Actor00400_Fn094C0,
    Actor00400_Fn094DC,
    Actor00400_Fn05320,
    Actor00400_Fn095D8,
} };

void Actor00400_Fn077F4(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable4   fns;
    Actor100400Work* work2;

    work = arg0->work;
    fns  = Actor00400_D00134;
    if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
        Gp_StateF0.field_3 = 1;
        Gp_ArmStateF0(1);
    } else if (Gp_StateF0.field_3 != 0) {
        work2            = arg0->work;
        work2->field_638 = 4;
        work2->field_63A = 0;
    } else {
        fns.funcs[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn0962C(Task* arg0);
void Actor00400_Fn058C4(Task* arg0);

void Actor00400_Fn078C8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0962C,
        Actor00400_Fn058C4,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

/// States `Actor00400_Fn0793C` and `Actor00400_Fn09C04` dispatch on `Actor100400Work.field_63A`.
const TaskFuncTable3 Actor00400_D00144 = { {
    Actor00400_Fn05D00,
    Actor00400_Fn096C0,
    Actor00400_Fn05EA4,
} };

void Actor00400_Fn0793C(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00144;
    fns.funcs[(s16)work->field_63A](arg0);
}

void Actor00400_Fn07998(Task* task)
{
}

void Actor00400_Fn079A0(Task* task)
{
}

void Actor00400_Fn079A8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn09714,
        Actor00400_Fn060CC,
    };

    states[(s16)work->field_63A](arg0);
}

static inline s32 Actor00400_TakeStateRequest(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_642 == 1) {
        switch (work->field_644) {
            case 2:
                work->field_638 = 8;
                work->field_63A = 0;
                work->field_644 = 0;
                return 1;
            case 3:
                work->field_638 = 9;
                work->field_63A = 0;
                work->field_644 = 0;
                return 1;
        }
    }
    work->field_644 = 0;
    return 0;
}

void Actor00400_Fn079FC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn061E8,
        Actor00400_Fn097C8,
    };
    s16 taken;

    taken = Actor00400_TakeStateRequest(arg0);
    if (taken == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn07ABC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn098A8,
        Actor00400_Fn09924,
    };

    states[(s16)work->field_63A](arg0);
}

/// States `Actor00400_Fn07B10` dispatches on `Actor100400Work.field_63A`.
const TaskFuncTable3 Actor00400_D00150 = { {
    Actor00400_Fn09A1C,
    Actor00400_Fn06380,
    Actor00400_Fn064B0,
} };

void Actor00400_Fn07B10(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00150;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->field_63A](arg0);
    }
}

/// States `Actor00400_Fn07B98` dispatches on `Actor100400Work.field_63A`.
const TaskFuncTable3 Actor00400_D0015C = { {
    Actor00400_Fn09A48,
    Actor00400_Fn09A8C,
    Actor00400_Fn06798,
} };

void Actor00400_Fn07B98(Task* arg0)
{
    GpEnemy*         obj;
    Actor100400Work* work;
    TaskFuncTable3   fns;

    obj                     = arg0->spawnArg2;
    work                    = arg0->work;
    fns                     = Actor00400_D0015C;
    work->field_660         = 1;
    obj->node.state.b.flags = 1;
    fns.funcs[(s16)work->field_63A](arg0);
}

/// States `Actor00400_Fn07C04` dispatches on `Actor100400Work.field_63A`.
const TaskFuncTable4 Actor00400_D00168 = { {
    Actor00400_Fn09AE0,
    Actor00400_Fn09B44,
    Actor00400_Fn09B74,
    Actor00400_Fn09BDC,
} };

void Actor00400_Fn07C04(Task* arg0)
{
    GpEnemy*         obj;
    Actor100400Work* work;
    TaskFuncTable4   fns;
    Actor100400Work* work2;

    obj  = arg0->spawnArg2;
    work = arg0->work;
    fns  = Actor00400_D00168;
    if (GameFlag_GetNibble(0xBC) != 0) {
        work2            = arg0->work;
        work2->field_638 = 0xB;
        work2->field_63A = 0;
    } else {
        work->field_660         = 1;
        obj->node.state.b.flags = 1;
        fns.funcs[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn07CC4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              i;

    work = arg0->work;
    work->field_636++;
    w = arg0->work;
    if (w->field_624 == 1) {
        if (w->field_626 != w->field_628) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->field_624 = 3;
    } else if (w->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        w->field_624 = 3;
        w->field_62A = 0;
    } else if (w->field_624 == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)w, i);
        i++;
    } while (i < 0xF);
    if (work->field_636 >= 0x3C) {
        work->field_638++;
    }
}

void Actor00400_Fn07DE0(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    Gp_SetLightMode(arg0->spawnArg2, 1);
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn07E20(Task* arg0)
{
    Actor100400Work* work;
    TmdObject*       ctx;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->field_636 >= 0x18) {
        ctx->flags     |= 2;
        work->field_636 = 0;
        work->field_638++;
    }
}

void Actor00400_Fn07E74(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if (work->field_636 > 0x20) {
        work->field_638++;
    }
}

void Actor00400_Fn07EE8(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    ctx->flags     |= 0x80;
    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn07F18(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    work            = arg0->work;
    ctx->flags     |= 0x80;
    work->field_636 = 0;
    work->field_638++;
}

void Actor00400_Fn07F44(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

void Actor00400_Fn07F88(Task* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn07FEC(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn08004(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Actor00400_D0002C;
    sp.funcs[arg0->state](arg0);
}

/// States `Actor00400_Fn09C04` dispatches on `Actor100400Work.field_63A`.
const TaskFuncTable7 Actor00400_D00178 = { {
    Actor00400_Fn09CCC,
    Actor00400_Fn09D3C,
    Actor00400_Fn09D98,
    Actor00400_Fn09E70,
    Actor00400_Fn06A44,
    Actor00400_Fn09F18,
    Actor00400_Fn09FDC,
} };

void Actor00400_Fn0805C(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor100400Work* work;
    GpEnemy*         obj;
    GpCoord*         coord;
    Actor100400Work* state;

    work  = arg0->work;
    obj   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    switch (arg2->command) {
        case 1:
            work->field_65E = 1;
            break;
        case 2:
            work->field_65E = 2;
            break;
        case 3:
            work->field_65E = 3;
            break;
        case 4:
            work->field_65E = 4;
            break;
        case 5:
            work->field_65E = 5;
            break;
        case 6:
            obj->node.state.b.flags = 0;
            Gp_SetLightMode(arg0->spawnArg2, 0);
            work->field_666   = 0;
            work->field_65E   = 6;
            coord->coord.t[1] = 0;
            arg0->state       = 1;
            state             = arg0->work;
            state->field_638  = 0;
            state->field_63A  = 0;
            state             = arg0->work;
            state->field_638  = 2;
            state->field_63A  = 0;
            break;
    }
}

void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3)
{
    MATRIX           m;
    VECTOR           d;
    VECTOR           r;
    GpCoord*         coords;
    Actor100400Work* work;

    coords = arg0->extra.tmd->coords;
    work   = arg0->work;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[arg1].workm, &m);
    d.vx = work->field_5E4.vx - m.t[0];
    d.vy = work->field_5E4.vy - arg3 - m.t[1];
    d.vz = work->field_5E4.vz - m.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &d, &r);
    arg2->vx = ratan2(-r.vy, r.vz) << 20 >> 20;
    arg2->vy = ratan2(r.vx, r.vz) << 20 >> 20;
    arg2->vz = 0;
}

void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX   a;
    MATRIX   b;
    GpCoord* coordA;
    GpCoord* coordB;
    GpCoord* coords;

    coords            = arg0->extra.tmd->coords;
    gGfxViewCoord.flg = 0;
    coordA            = &coords[arg1];
    coordB            = &coords[arg2];
    Gp_UpdateCoord(&gGfxViewCoord);
    coordA->flg = 0;
    coordB->flg = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coordA->workm, &a);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coordB->workm, &b);
    arg3->vx    = (a.t[0] + b.t[0]) / 2;
    arg3->vz    = (a.t[2] + b.t[2]) / 2;
    coordA->flg = 0;
    coordB->flg = 0;
}

/// Reaction to message 0x7D5: `arg2` toggles the "big" flag (0x80) on the
/// actor's context. Turning it on is unconditional; turning it off first checks
/// whether the current state / animation combination still wants it held.
///
/// GCC 2.8.1 decides the store to `Gp_StateF0`, at a fixed address, cannot
/// alias the struct fields reached through `ctx` / `work`, so without the
/// barrier the scheduler sinks this `sb` past the traffic that follows it.
void Actor00400_Fn08354(Task* arg0, s32 arg1, s32 arg2)
{
    Actor100400Work* work;
    TmdObject*       ctx;
    s32              state;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            Gp_StateF0.field_23 = 1;
            SOFT_BARRIER();
            ctx->flags     |= 0x80;
            work->field_663 = 1;
            break;
        case 1:
            Gp_StateF0.field_23 = 0;
            SOFT_BARRIER();
            state = arg0->state;
            if (((state == 2) || (state == 4)) && (work->field_644 == 4)) {
                ctx->flags |= 0x80;
            } else if ((arg0->state == 2) && ((work->field_638 == 4) || (work->field_638 == 5))) {
                ctx->flags |= 0x80;
            } else if ((arg0->state == 4) && (work->field_638 == 6)) {
                ctx->flags |= 0x80;
            } else if (arg0->state == 5) {
                ctx->flags |= 0x80;
            } else {
                ctx->flags &= ~0x80;
            }
            work->field_663 = 0;
            break;
    }
}

void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX   root;
    MATRIX   a;
    MATRIX   b;
    GpCoord* coordA;
    GpCoord* coordB;
    GpCoord* coords;

    coords            = arg0->extra.tmd->coords;
    gGfxViewCoord.flg = 0;
    coordA            = &coords[arg1];
    coordB            = &coords[arg2];
    Gp_UpdateCoord(&gGfxViewCoord);
    coordA->flg = 0;
    coordB->flg = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coordA->workm, &a);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coordB->workm, &b);
    coords[0].coord.t[0] = arg3->vx - ((a.t[0] + b.t[0]) / 2 - root.t[0]);
    coords[0].coord.t[2] = arg3->vz - ((a.t[2] + b.t[2]) / 2 - root.t[2]);
    coords[0].flg        = 0;
    coordA->flg          = 0;
    coordB->flg          = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_UpdateCoord(coords);
}

void Actor00400_Fn085B8(Task* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->work;
    i    = 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_628);
        work->slots[i].rate = (u8)work->field_632;
        i++;
    } while (i < 0xF);
    work->field_626 = (u16)work->field_628;
}

/// Like `Actor00400_Fn085B8`, but restarts every slot through `func_800B4114`
/// with the pending blend value `field_63C`, which is consumed (cleared) only
/// when the requested clip `field_628` differs from the current `field_626`.
void Actor00400_Fn08624(Task* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->work;
    if (work->field_626 == work->field_628) {
        i = 1;
        do {
            work->slots[i].rate = (u8)work->field_632;
            func_800B4114(work, i, work->field_628, 0, work->field_63C);
            i++;
        } while (i < 0xF);
    } else {
        i = 1;
        do {
            work->slots[i].rate = (u8)work->field_632;
            func_800B4114(work, i, work->field_628, 0, work->field_63C);
            i++;
        } while (i < 0xF);
        work->field_63C = 0;
    }
    work->field_626 = (u16)work->field_628;
}

/// Scales `arg1` (a 12-bit angle) by the ratio `work->field_632`, returning 0
/// while that field is unset.
s16 Actor00400_Fn086FC(Task* arg0, s16 arg1)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_632 == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_632 << 12) >> 16;
}

/// Turns `work->field_556` toward `arg1` by at most `arg2` per call, but only
/// once the shortest signed 12-bit angle difference leaves the deadband
/// `arg3 & 0x7FF`. The two conditions share one arm rather than nesting, which
/// collapses the arms into the entry block: `work` then lives over 32 insns,
/// exactly tying its allocator priority with `range`'s, and the tie falls
/// through to the declaration order - hence `range` is declared ahead of `work`.
void Actor00400_Fn0875C(Task* arg0, Actor100400Entry8* arg1, s32 arg2, s32 arg3)
{
    s32              range;
    Actor100400Work* work;
    GpCoord*         coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    work        = arg0->work;
    coords      = arg0->extra.tmd->coords;
    coords->flg = 0;
    range       = arg3 & 0x7FF;
    vec.vx      = arg1->field_0 - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = arg1->field_4 - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if ((diff > range) || (diff < -range)) {
        work->field_556 = (diff > range) ? (angle - arg2) : (angle + arg2);
    }
}

/// One animation-step: state 1 starts the clip `field_628` (or advances the
/// current one through `Actor00400_Fn086FC` when it is already in place, and
/// resets `field_62A` when it is not), state 2 finishes the old clip and state
/// 3 counts `field_62A` up a frame at a time. All three land in state 3 and
/// then tick animation slots 1..14.
void Actor00400_Fn08814(Task* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->work;
    if (work->field_624 == 1) {
        if (work->field_626 != work->field_628) {
            work->field_62A = 0;
        } else {
            work->field_62A = Actor00400_Fn086FC(arg0, work->field_62A);
        }
        Actor00400_Fn08624(arg0);
        work->field_624 = 3;
    } else if (work->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        work->field_624 = 3;
        work->field_62A = 0;
    } else if (work->field_624 == 3) {
        work->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0xF);
}

void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = arg3;
    work->field_632 = arg2;
    work->field_628 = arg1;
    work->field_624 = 1;
}

/// Same body as src/lib/actors_shared_8016974c.c.
s16 Actor00400_Fn08908(Task* arg0)
{
    Actor100400Work* work = arg0->work;

    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        return 1;
    }
    return 0;
}

void Actor00400_Fn08948(Task* arg0)
{
    TaskFuncTable8 fns;

    fns = Actor00400_D00038;
    fns.funcs[arg0->state](arg0);
}

void Actor00400_Fn089C8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A3D4,
        Actor00400_Fn0A414,
    };

    states[work->field_638](arg0);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. Same body as src/lib/actors_shared_80132c4c.c.
void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

void Actor00400_Fn08A88(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08ADC,
        Actor00400_Fn06EA4,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn08ADC(Task* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work             = arg0->work;
    random           = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState      = random;
    state            = arg0->work;
    state->field_63C = 8;
    state->field_632 = ((random >> 16) & 3) + 3;
    state->field_628 = 0xF;
    state->field_624 = 1;
    work->field_636  = 0;
    work->field_63A++;
}

void Actor00400_Fn08B40(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08B94,
        Actor00400_Fn06F64,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn08B94(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->work;
    sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    state            = arg0->work;
    state->field_63C = 2;
    state->field_632 = 0x10;
    state->field_628 = 0x13;
    state->field_624 = 1;
    work->field_63A++;
}

void Actor00400_Fn08C54(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s16              mode;
    s32              i;

    work = arg0->work;
    work->field_636++;

    state = arg0->work;
    mode  = state->field_624;
    if (mode == 1) {
        if (state->field_626 != state->field_628) {
            state->field_62A = 0;
        } else {
            state->field_62A = Actor00400_Fn086FC(arg0, state->field_62A);
        }
        Actor00400_Fn08624(arg0);
        state->field_624 = 3;
    } else if (mode == 2) {
        Actor00400_Fn085B8(arg0);
        state->field_624 = 3;
        state->field_62A = 0;
    } else if (mode == 3) {
        state->field_62A++;
    }

    for (i = 1; i < 15; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)state, i);
    }

    if (work->field_636 >= 0x1E) {
        work->field_638++;
    }
}

void Actor00400_Fn08D70(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_61E = 0x1000;
    work->field_5BC = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2, 1);
    work->field_636 = 0;
    work->field_638 = work->field_638 + 1;
}

void Actor00400_Fn08DFC(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->field_636 >= 0x18) {
        ctx->flags     |= 2;
        work->field_636 = 0;
        work->field_638++;
    }
}

void Actor00400_Fn08E50(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;
    GpCoord*         coord;
    VECTOR           scale;
    SVECTOR          pos;

    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    coord = ctx->coords;

    work->field_648 -= 7;
    if (work->field_648 < 0) {
        work->field_648 = 0;
    }
    work->field_61E -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = work->field_61E;
    scale.vz         = 0x1000;
    coord->coord     = work->field_5BC;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;
    if (++work->field_636 == 4) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 4, &pos);
    }
    if (work->field_636 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if (work->field_636 >= 0x21) {
        ctx->flags |= 0x80;
        work->field_638++;
    }
}

void Actor00400_Fn08FB0(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn08FC8(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    work            = arg0->work;
    ctx->flags     |= 0x80;
    work->field_636 = 0;
    work->field_638++;
}

void Actor00400_Fn08FF4(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

void Actor00400_Fn09038(Task* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn0909C(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn090B4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;

    work                                            = arg0->work;
    ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
    Gp_IncStateF0Ref(0);
    work->flags_62C.hi.field_62E = 0;
    work->field_630              = 0x174B;
    state                        = arg0->work;
    state->field_632             = 0x10;
    state->field_628             = 2;
    state->field_624             = 2;
    state2                       = arg0->work;
    state2->field_638            = 1;
    state2->field_63A            = 0;
}

/// Every path out of the range test funnels through `set`, where the arm flag
/// is copied for the test below: the in-range edge arrives with the same value
/// (0), so the copy is redundant there and cse drops it, which leaves `done`
/// defined only here - and reorg then fills the branch's delay slot with a copy
/// of that one move.
void Actor00400_Fn09124(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    s32              active;
    s32              done;

    work   = arg0->work;
    active = 0;
    if (work->field_640 >= 0xDAC) {
        goto set;
    }
    done = 0;
    if ((u32)(work->field_634 - 0x600) >= 0x400U) {
        Gp_StateF0.field_3 = 1;
        Gp_ArmStateF0(1);
        active           = 1;
        state            = arg0->work;
        state->field_638 = 2;
        state->field_63A = 0;
    }
set:
    done = active;
    if (done == 0) {
        if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
            Gp_ArmStateF0(1);
            return;
        }
        if (work->field_642 != 0) {
            state2            = arg0->work;
            state2->field_638 = 2;
            state2->field_63A = 0;
        }
    }
}

void Actor00400_Fn091F8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A5B8,
    };

    if (Actor00400_Fn02154(arg0) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn09260(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A680,
        Actor00400_Fn0A6B0,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn092D4(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A704,
        Actor00400_Fn0A760,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn09348(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A7F0,
        Actor00400_Fn0A82C,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn093BC(Task* task)
{
}

void Actor00400_Fn093C4(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A880,
        Actor00400_Fn04900,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn09418(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A940,
        Actor00400_Fn04A1C,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0946C(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A9F4,
        Actor00400_Fn0AA40,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn094C0(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_65B = 0;
    work->field_63A = work->field_63A + 1;
}

void Actor00400_Fn094DC(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              pan;
    s32              sound;

    work = arg0->work;
    if ((Actor00400_Fn02208(arg0) << 0x10) != 0) {
        if (work->field_628 != 5) {
            work2            = arg0->work;
            work2->field_63C = 0x10;
            work2->field_632 = 0x10;
            work2->field_628 = 5;
            work2->field_624 = 1;
        }
        work->field_63E = work->field_64E;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        work->field_636 = 0;
        work->field_63A = work->field_63A + 1;
        return;
    }
    work->field_660 = 1;
    if (!(work->field_630 & 0xF)) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040001;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
}

void Actor00400_Fn095D8(Task* arg0)
{
    s32              cond;
    Actor100400Work* work;
    Actor100400Work* work2;

    work            = arg0->work;
    work->field_660 = 1;
    work2           = arg0->work;
    if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_63A = 1;
    }
}

void Actor00400_Fn0962C(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    GpCoord*         coord;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_574.vx = work->field_56C.vx;
    work->field_636    = 0;
    work->field_574.vy = work->field_56C.vy;
    work->field_574.vz = work->field_56C.vz;
    coord->coord.t[0] += ((s16)work->field_574.vx - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += ((s16)work->field_574.vz - coord->coord.t[2]) >> 2;
    state              = arg0->work;
    state->field_63C   = 0xA;
    state->field_632   = 0x10;
    state->field_628   = 1;
    state->field_624   = 1;
    work->field_63A    = work->field_63A + 1;
}

void Actor00400_Fn096C0(Task* arg0)
{
    s32              cond;
    Actor100400Work* work;

    work = arg0->work;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09714(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 0xA;
    work->field_624 = 1;
    sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_63A++;
}

void Actor00400_Fn097C8(Task* arg0)
{
    s32              cond;
    s32              pan;
    s32              sound;
    Actor100400Work* work;
    Actor100400Work* state;

    work = arg0->work;
    if (work->field_642 != 0) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    state = arg0->work;
    if ((state->flags_62C.half & 1) || (state->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        state            = arg0->work;
        state->field_638 = 2;
        state->field_63A = 0;
    }
}

void Actor00400_Fn098A8(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 8;
    work->field_632 = 0x10;
    work->field_628 = 0xE;
    work->field_624 = 1;
    work->field_63E = (u16)work->field_64E + 0x64;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    work->field_610 = 0x64;
    work->field_636 = 0;
    work->field_664 = 1;
    work->field_63A = work->field_63A + 1;
}

void Actor00400_Fn09924(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s32              cond;
    s32              mode;

    work = arg0->work;
    mode = work->field_642;
    work->field_636++;
    if (mode == 1) {
        state            = arg0->work;
        state->field_63C = 2;
        state->field_632 = 0x10;
        state->field_628 = 0x12;
        state->field_624 = mode;
    } else {
        state = arg0->work;
        if ((state->flags_62C.half & 1) || (state->flags_62C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            state            = arg0->work;
            state->field_63C = 8;
            state->field_632 = 0x10;
            state->field_628 = 0x10;
            state->field_624 = 1;
        }
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
        work->field_610  = 0;
        work->field_664  = 4;
        state            = arg0->work;
        state->field_638 = 4;
        state->field_63A = 0;
    }
}

void Actor00400_Fn09A1C(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_660 = 0;
    work->field_636 = 0;
    work->field_63E = (u16)work->field_64E + 0x64;
    work->field_63A = work->field_63A + 1;
}

void Actor00400_Fn09A48(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work             = arg0->work;
    work->field_65B  = 0;
    state            = arg0->work;
    state->field_63C = 0xA;
    state->field_632 = 0x10;
    state->field_628 = 3;
    state->field_624 = 1;
    work->field_63A  = work->field_63A + 1;
}

void Actor00400_Fn09A8C(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 2 || GameFlag_GetNibble(0xBC) != 0) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09AE0(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    Actor100400Work* state;

    work              = arg0->work;
    coord             = arg0->extra.tmd->coords;
    coord->coord.t[0] = 0x10E0;
    coord->coord.t[1] = 0x178;
    work->field_63E   = 0x178;
    coord->coord.t[2] = -0xDAC;
    work->field_554   = 0;
    work->field_556   = 0;
    work->field_558   = 0;
    state             = arg0->work;
    state->field_632  = 0x10;
    state->field_628  = 3;
    state->field_624  = 2;
    work->field_636   = 0;
    work->field_63A   = work->field_63A + 1;
}

void Actor00400_Fn09B44(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 1) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09B74(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 < 0x30) {
        Actor00400_Fn0762C(arg0, 0xA0, work->field_556);
        return;
    }
    work->field_63A++;
}

void Actor00400_Fn09BDC(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 2) {
        work->field_638 = 0xB;
        work->field_63A = 0;
    }
}

void Actor00400_Fn09C04(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable7   fns;

    work = arg0->work;
    fns  = Actor00400_D00178;
    fns.funcs[(s16)work->field_63A](arg0);
}

void Actor00400_Fn09C84(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A034,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn09CCC(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    Actor100400Work* state;

    work              = arg0->work;
    coord             = arg0->extra.tmd->coords;
    work->field_660   = 1;
    coord->coord.t[0] = -0x6C0;
    coord->coord.t[1] = 0x3E8;
    work->field_63E   = 0x3E8;
    coord->coord.t[2] = -0xBB8;
    work->field_554   = 0;
    work->field_556   = 0x800;
    work->field_558   = 0;
    state             = arg0->work;
    state->field_632  = 0x10;
    state->field_628  = 3;
    state->field_624  = 2;
    work->field_636   = 0;
    work->field_63A   = work->field_63A + 1;
}

void Actor00400_Fn09D3C(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (GameFlag_GetNibble(0xEB) == 1) {
        work->field_63A = 3;
    } else if (work->field_65E == 3) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09D98(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count < 0x30) {
        Actor00400_Fn0762C(arg0, 0x60, work->field_556);
        if (!(work->field_630 & 0xF)) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040001;
            pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->field_63A += 1;
    }
}

void Actor00400_Fn09E70(Task* arg0)
{
    Actor100400Work* work;
    GpCoord*         coord;
    Actor100400Work* state;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_65E == 4) {
        work->field_636   = 0;
        work->field_63E   = work->field_64E;
        coord->coord.t[0] = -0x6C0;
        coord->coord.t[2] = -0x2008;
        work->field_554   = 0;
        work->field_556   = 0;
        work->field_558   = 0;
        state             = arg0->work;
        state->field_63C  = 8;
        state->field_632  = 0x10;
        state->field_628  = 1;
        state->field_624  = 1;
        work->field_63A   = work->field_63A + 1;
    } else {
        work->field_636   = 0;
        coord->coord.t[0] = -0x6C0;
        coord->coord.t[2] = -0x2008;
        work->field_554   = 0;
        work->field_556   = 0;
        work->field_558   = 0;
        coord->coord.t[1] = 0x3E8;
        work->field_63E   = 0x3E8;
    }
}

void Actor00400_Fn09F18(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count == 0x26) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x54220006;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_636 == 0x30) {
        work->field_63A += 1;
    }
}

void Actor00400_Fn09FDC(Task* arg0)
{
    GpEnemy*         obj;
    Actor100400Work* work;

    obj = arg0->spawnArg2;
    if (((Actor100400Work*)arg0->work)->field_65E == 5) {
        obj->node.state.b.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->work;
        work->field_638 = 4;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A034(Task* arg0)
{
    GpEnemy*         obj;
    Actor100400Work* work;

    obj = arg0->spawnArg2;
    if (((Actor100400Work*)arg0->work)->field_65E == 5) {
        obj->node.state.b.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->work;
        work->field_638 = 4;
        work->field_63A = 0;
    }
}

/// Carries `pos` from `coord`'s space along the `sub` links, applying each
/// coordinate's matrix in turn, until the walk reaches the view coordinate
/// `gGfxViewCoord`; the result is written back to `pos` and 1 returned. A
/// chain that ends before reaching the view returns 0 and leaves `pos` as it
/// was.
s32 Actor00400_Fn0A08C(GpCoord* coord, SVECTOR* pos)
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

/// First kill-path state, entered the frame the marker task is spawned:
/// `Actor00400_Fn02D48` walks it afterwards and `Actor00400_Fn0A28C` retires it.
///
/// `task->work` is the 0x64-byte `Actor100400MarkerWork` block
/// `Actor00400_SpawnMarker` allocated, and `task->extra` the `TmdObject`
/// whose `coords` is the coordinate the marker is drawn at. That coordinate is
/// re-parented to `gGfxViewCoord` here, and the object is linked to it with its
/// two `GpRec18` slots zeroed, so the state `Actor00400_Fn02D48` runs can report
/// what the marker collides with. `field_5A` is seeded with the negative span
/// the spawner's tip overshot by, and the object's draw scale with 0x100.
///
/// The `task->extra` walk is repeated for `work->obj.coord` rather than reusing
/// `coord`: the original re-reads it, which is what the second `lw` chain in
/// the target shows.
///
/// `coord` is assigned before `work` on purpose. sched1 emits each load where
/// its source order puts it, and that position is the quantity's `birth`:
/// writing `coord` second lands its `lw` one insn later, shortening its span
/// from 70 to 68 and raising its `QTY_CMP_PRI` from 1428 to 1470 — above the
/// task pointer's 1458 — so local-alloc hands the coordinate `$s1` and the task
/// pointer `$s2` instead of the reverse. See DECOMPILATION_LEARNINGS.md,
/// "A parameter competes in local-alloc on its raw span, not its doubled
/// `REG_LIVE_LENGTH`".
void Actor00400_Fn0A190(Task* task)
{
    Actor100400MarkerWork* work;
    GpCoord*               coord;

    coord               = task->extra.tmd->coords;
    work                = (Actor100400MarkerWork*)task->work;
    task->killCountdown = 0;
    work->field_60      = 0;
    coord->sub          = &gGfxViewCoord;
    coord->flg          = 0;
    work->obj.key       = Gp_PackPair(&Actor00400_D0FDC0, 1);
    work->obj.coord     = task->extra.tmd->coords;
    work->obj.ctx.recs  = work->recs;
    work->obj.pos.vx    = 0;
    work->obj.pos.vy    = 0;
    work->obj.pos.vz    = 0;
    work->obj.radius    = 0x100;
    work->obj.flags     = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(work->recs, 2, 0);
    work->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    work->field_5A = -0x14;
    Actor00400_Fn001AC(coord, (u16)work->field_60, 0, 0x1300);
    task->state++;
}

/// Last kill-path state of the marker task. Each frame it clears the marker
/// coordinate's `flg` and counts `killCountdown` up; on the twelfth frame it
/// unlinks the marker's display object and kills the task.
void Actor00400_Fn0A28C(Task* task)
{
    Actor100400MarkerWork* work;
    TmdObject*             ctx;
    u16                    countdown;

    work                = (Actor100400MarkerWork*)task->work;
    ctx                 = task->extra.tmd;
    ctx->coords->flg    = 0;
    countdown           = task->killCountdown + 1;
    task->killCountdown = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        taskKill(task);
    }
}

void Actor00400_Fn0A2F4(Task* arg0)
{
    Actor100400QuadWork* work;
    GpEnemy*             object;

    work   = (Actor100400QuadWork*)arg0->work;
    object = work->field_0;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    if ((s16)object->hp <= 0) {
        arg0->state++;
    }
}

void Actor00400_Fn0A364(Task* arg0)
{
    Actor100400QuadWork* work;
    u8                   intensity;

    work = (Actor100400QuadWork*)arg0->work;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    intensity       = work->intensity - 1;
    work->intensity = intensity;
    if (intensity == 0) {
        taskKill(arg0);
    }
}

void Actor00400_Fn0A3D4(Task* arg0)
{
    Actor100400Work*   work;
    Actor100400Record* record;

    work   = arg0->work;
    record = (Actor100400Record*)(work->field_64A * sizeof(Actor100400Record) + (u32)work->field_608);
    if (record->field_6 != 0) {
        record->field_6 = 0;
    }
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn0A414(Task* arg0)
{
    Actor100400Work* work;
    u16              frame;

    work            = arg0->work;
    frame           = (u16)work->field_636 + 1;
    work->field_636 = frame;
    if ((s16)frame >= 0x12D) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void Actor00400_Fn0A468(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A510,
        Actor00400_Fn07400,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0A4BC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A57C,
        Actor00400_Fn07518,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0A510(Task* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work             = arg0->work;
    random           = Gp_LcgState * 5 + 0x71357911;
    work->field_63E  = work->field_64E;
    state            = arg0->work;
    state->field_63C = 8;
    state->field_632 = ((random >> 16) & 3) + 3;
    state->field_628 = 0x10;
    state->field_624 = 1;
    Gp_LcgState      = random;
    work->field_636  = 0;
    work->field_63A++;
}

void Actor00400_Fn0A57C(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 2;
    work->field_632 = 0x10;
    work->field_628 = 0x12;
    work->field_624 = 1;
    work->field_636 = 0;
    work->field_63A++;
}

void Actor00400_Fn0A5B8(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work = arg0->work;
    if (work->field_640 < 0x4E2) {
        work->field_638                  = 5;
        work->field_63A                  = 0;
        work->field_614[work->field_65A] = work->field_638;
        if (work->field_614[0] == work->field_614[1] &&
            work->field_614[0] == work->field_614[2] &&
            work->field_614[0] == 5) {
            state                            = arg0->work;
            state->field_638                 = 4;
            state->field_63A                 = 0;
            work->field_614[work->field_65A] = work->field_638;
        }
    } else {
        work->field_638                  = 4;
        work->field_63A                  = 0;
        work->field_614[work->field_65A] = work->field_638;
    }
    work->field_65A++;
    if (work->field_65A >= 3U) {
        work->field_65A = 0;
    }
}

void Actor00400_Fn0A680(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 6;
    work->field_624 = 1;
    work->field_63A++;
}

void Actor00400_Fn0A6B0(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A704(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->field_63A = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work->field_63A++;
}

void Actor00400_Fn0A760(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->field_63A = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work = arg0->work;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A7F0(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 9;
    work->field_624 = 1;
    work->field_646 = 0x18;
    work->field_63A++;
}

void Actor00400_Fn0A82C(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A880(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->work;
    sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    state            = arg0->work;
    state->field_63C = 6;
    state->field_632 = 0x10;
    state->field_628 = 0xC;
    state->field_624 = 1;
    work->field_63A++;
}

void Actor00400_Fn0A940(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    work->field_63C = 4;
    work->field_632 = 0x10;
    work->field_628 = 0xD;
    work->field_624 = 1;
    sound           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_63A++;
}

void Actor00400_Fn0A9F4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work             = arg0->work;
    work->field_665  = 1;
    state            = arg0->work;
    state->field_63C = 8;
    state->field_632 = 0x10;
    state->field_628 = 0xF;
    state->field_624 = 1;
    work->field_610  = 0x64;
    work->field_636  = 0;
    work->field_63A++;
}

void Actor00400_Fn0AA40(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s32              cond;

    work = arg0->work;
    work->field_636++;
    state = arg0->work;
    if ((state->flags_62C.half & 1) || (state->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        state            = arg0->work;
        state->field_63C = 8;
        state->field_632 = 0x10;
        state->field_628 = 0x11;
        state->field_624 = 1;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2)) {
        work->field_610  = 0;
        work->field_665  = 0;
        state            = arg0->work;
        state->field_638 = 2;
        state->field_63A = 0;
    }
}
