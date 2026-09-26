#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Private work block of the actor 01900 task, hanging off `Task::work`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` list nodes unlinked by the
/// destroy callback and the two child tasks it kills. `Actor01900_Fn0A764`
/// masks `field_A08.flags` and `field_B48.flags`, which is what fixes those
/// two offsets as `GpObj` rather than opaque padding.
/// XZ patrol point in `Actor01900Work.field_C`; `field_14` selects which one
/// `Actor01900_Fn06F40` walks toward.
typedef struct Actor01900Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor01900Waypoint;

typedef struct Actor01900Work {
    /* 0x000 */ s16                field_0;
    /* 0x002 */ s16                field_2;
    /* 0x004 */ s16                field_4;
    /* 0x006 */ s16                field_6;
    /* 0x008 */ s16                field_8;
    /* 0x00A */ byte               pad_A[2];
    /* 0x00C */ Actor01900Waypoint field_C[2];
    /* 0x014 */ s16                field_14;
    /* 0x016 */ byte               pad_16[0x44];
    /* 0x05A */ u16                field_5A;
    /* 0x05C */ byte               pad_5C[0xC];
    /* 0x068 */ u16                field_68;
    /* 0x06A */ byte               pad_6A[0x82A];
    /* 0x894 */ s32                field_894;
    /* 0x898 */ s16                field_898;
    /* 0x89A */ s16                field_89A;
    /* 0x89C */ byte               pad_89C[2];
    /* 0x89E */ s16                field_89E;
    /* 0x8A0 */ byte               pad_8A0[2];
    /* 0x8A2 */ s16                field_8A2;
    /* 0x8A4 */ s16                field_8A4;
    /* 0x8A6 */ s16                field_8A6;
    /* 0x8A8 */ s16                field_8A8;
    /* 0x8AA */ byte               pad_8AA[4];
    /* 0x8AE */ s16                field_8AE;
    /* 0x8B0 */ s16                field_8B0;
    /* 0x8B2 */ byte               pad_8B2[2];
    /* 0x8B4 */ s32                field_8B4;
    /* 0x8B8 */ GpEffArg           field_8B8;
    /* 0x8C0 */ byte               pad_8C0[8];
    /* 0x8C8 */ GpObj              field_8C8;
    /* 0x8E8 */ GpRec18            field_8E8;
    /* 0x900 */ byte               pad_900[0x108];
    /* 0xA08 */ GpObj              field_A08;
    /* 0xA28 */ GpRec18            field_A28;
    /* 0xA40 */ byte               pad_A40[0x108];
    /* 0xB48 */ GpObj              field_B48;
    /* 0xB68 */ GpRec18            field_B68;
    /* 0xB80 */ byte               pad_B80[0x30];
    /* 0xBB0 */ MATRIX             field_BB0; // `TmdObject.lightMtx` light matrix
    /* 0xBD0 */ MATRIX             field_BD0; // `TmdObject.colorMtx` color matrix
    /* 0xBF0 */ byte               pad_BF0[0x20];
    /* 0xC10 */ s16                field_C10;
    /* 0xC12 */ s16                field_C12;
    /* 0xC14 */ s16                field_C14;
    /* 0xC16 */ byte               pad_C16[2];
    /* 0xC18 */ SVECTOR            field_C18;
    /* 0xC20 */ s16                field_C20;
    /* 0xC22 */ s16                field_C22;
    /* 0xC24 */ s16                field_C24;
    /* 0xC26 */ s16                field_C26;
    /* 0xC28 */ s16                field_C28;
    /* 0xC2A */ s16                field_C2A;
    /* 0xC2C */ s16                field_C2C;
    /* 0xC2E */ s16                field_C2E;
    /* 0xC30 */ s16                field_C30;
    /* 0xC32 */ s16                field_C32;
    /* 0xC34 */ u8                 field_C34[3];
    /* 0xC37 */ u8                 field_C37;
    /* 0xC38 */ Task*              field_C38;
    /* 0xC3C */ Task*              field_C3C;
    /* 0xC40 */ s16                field_C40;
    /* 0xC42 */ s16                field_C42;
    /* 0xC44 */ s16                field_C44;
    /* 0xC46 */ byte               pad_C46[2];
    /// Ring of the last seven view-space positions `Actor01900_Fn09D3C`
    /// records, one per step; `field_C98` is the write cursor.
    /* 0xC48 */ SVECTOR field_C48[7];
    /* 0xC80 */ byte    pad_C80[0x18];
    /* 0xC98 */ s16     field_C98;
} Actor01900Work;

/// Animation view of the same task work block: the two animation contexts and
/// their slot arrays, which `Actor01900Work` covers with padding because it
/// names other fields inside that range. The tail from 0x898 on is the same
/// run of halfwords `Actor01900Work` describes, repeated here so the animation
/// driver can reach both through one pointer. The bytes the arrays do not
/// cover are not yet described.
typedef struct Actor01900AnimWork {
    /* 0x000 */ byte           pad_0[0x1C];
    /* 0x01C */ ActorAnimRig19 rig;
    /* 0x458 */ ActorAnimRig19 blend;
    /* 0x894 */ byte           pad_894[0x4];
    /* 0x898 */ s16            field_898;
    /* 0x89A */ s16            field_89A;
    /* 0x89C */ s16            field_89C;
    /* 0x89E */ s16            field_89E;
    /* 0x8A0 */ u16            field_8A0;
    /* 0x8A2 */ s16            field_8A2;
    /* 0x8A4 */ byte           pad_8A4[2];
    /* 0x8A6 */ s16            field_8A6;
    /* 0x8A8 */ s16            field_8A8;
    /* 0x8AA */ s16            field_8AA;
    /* 0x8AC */ s16            field_8AC;
    /* 0x8AE */ s16            field_8AE;
    /* 0x8B0 */ s16            field_8B0;
    /* 0x8B2 */ byte           pad_8B2[2];
    /* 0x8B4 */ s32            field_8B4;
} Actor01900AnimWork;

/// The actor's state handlers, indexed by `Actor01900Work::field_0`.
/// `Actor01900_Fn09D3C` copies the table to its frame before dispatching.
typedef struct Actor01900StateTable {
    TaskFunc fn[32];
} Actor01900StateTable;
STATIC_ASSERT_SIZEOF(Actor01900StateTable, 0x80);

extern GpPairSrcE         Actor01900_D0AC54;
extern ActorSpawnParamRow Actor01900_D0AC64[];
extern u8                 Actor01900_D17174[];
extern void*              Actor01900_D1728C;
extern char               Actor01900_D16960;
extern void*              Actor01900_D171B4;
extern ActorHeightClamp   Actor01900_D172CC[];
/// Twelve preset hit-reaction directions `Actor01900_Fn02664` copies from;
/// `pad` carries the index of the coordinate the effect is attached to.
extern SVECTOR Actor01900_D1722C[];
extern char    Actor01900_D10B68;
extern s16     Actor01900_D172FC;
extern void*   D_80114B78[1];

s32  Actor01900_Fn00E00(GpCoord* coord, GpRec18* rec, s32 arg2);
void Actor01900_Fn02A50(Task* arg0);
void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id);
s32  Actor01900_Fn016F0(Task* arg0);
void Actor01900_Fn01C94(Task* arg0);
void Actor01900_Fn0AB1C(Task* arg0);
void Actor01900_Fn0A6CC(Task* task);
s32  Actor01900_Fn03FF8(Task* arg0, GpRec18* recs, s16 count);
void Actor01900_Fn08724(Task* arg0);
void Actor01900_Fn0A7C0(Task* arg0);
void Actor01900_Fn03C04(GpAreaKey* session, GpCoord* coord);
s32  Actor01900_Fn0A31C(Task* arg0, s32 arg1, GpAnimArg* arg2);
s32  Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2);
s32  Actor01900_Fn0A38C(Task* arg0, s32 arg1, s32 arg2);

/* Inline bodies behind `Actor01900_Fn080A8`. Same shapes as
 * `actor_400100_facing.h` and `ActorsShared80135a60`; inlining is what keeps
 * each `G_SCRATCH_HEAD` access out of a register CSE would share. */

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `actorMoveForwardNonzero`.
static __inline__ void Actor01900_MoveForward(GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (Mc_SaveData.field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR);
        vec                   = head - 1;
        SCRATCH_HEAD(SVECTOR) = vec;
        gteVec                = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        SCRATCH_POP(SVECTOR);
    }
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen, without `Actor01900_MoveForward`'s zero-amount guard. Same body as
/// `actorMoveForward`.
static __inline__ void Actor01900_StepForward(GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (Mc_SaveData.field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR);
        vec                   = head - 1;
        SCRATCH_HEAD(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0] += vec->vx;
        coord->coord.t[1] += vec->vy;
        coord->coord.t[2] += vec->vz;
        coord->flg         = 0;
        SCRATCH_POP(SVECTOR);
    }
}

/// `Actor01900_StepForward` with the X component read back through `head`,
/// as `Actor01900_MoveForward` does.
static __inline__ void Actor01900_StepForwardHead(GpCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (Mc_SaveData.field_5C1 != 1) {
        head                  = SCRATCH_HEAD(SVECTOR);
        vec                   = head - 1;
        SCRATCH_HEAD(SVECTOR) = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0] += head[-1].vx;
        coord->coord.t[1] += vec->vy;
        coord->coord.t[2] += vec->vz;
        coord->flg         = 0;
        SCRATCH_POP(SVECTOR);
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale.
static __inline__ void Actor01900_ResetYaw(GpCoord* coord)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    coord->coord.m[2][2] = (u16)blk->m.m[2][2];
    coord->flg           = 0;
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Cross-fade lengths in frames, indexed by the clip being left and the clip
/// being entered. `Actor01900_Fn01C94` reads one entry per animation change.
extern s8      Actor01900_D16988[][0x2D];
extern SVECTOR Actor01900_D1730C;

extern const Actor01900StateTable Actor01900_D001BC;

void Actor01900_Fn03710(Task* arg0);
void Actor01900_Fn03854(Task* arg0);
void Actor01900_Fn042BC(Task* arg0);
void Actor01900_Fn04D14(Task* arg0);
void Actor01900_Fn0551C(Task* arg0);
void Actor01900_Fn05B4C(Task* arg0);
void Actor01900_Fn05F38(Task* arg0);
void Actor01900_Fn06100(Task* arg0);
void Actor01900_Fn06634(Task* arg0);
void Actor01900_Fn06904(Task* arg0);
void Actor01900_Fn06B4C(Task* arg0);
void Actor01900_Fn06F40(Task* arg0);
void Actor01900_Fn07810(Task* arg0);
void Actor01900_Fn07BA8(Task* arg0);
void Actor01900_Fn080A8(Task* arg0);
void Actor01900_Fn083E8(Task* arg0);
void Actor01900_Fn0892C(Task* arg0);
void Actor01900_Fn09694(Task* arg0);
void Actor01900_Fn09BE8(Task* arg0);
void Actor01900_Fn0A764(Task* arg0);
void Actor01900_Fn0A868(Task* arg0);
void Actor01900_Fn0A914(Task* arg0);
void Actor01900_Fn0A9C0(Task* arg0);
void Actor01900_Fn0AA78(Task* arg0);
void Actor01900_Fn0ABA0(GpEnemy* enemy, Task* task);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void Actor01900_Fn00260(GpCoord* coord, s16 yaw)
{
    MATRIX*  rotation;
    GpCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 Actor01900_Fn0056C(GpCoord* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                      = 0;
    head                            = SCRATCH_HEAD(ActorRepelScratch);
    blk                             = head - 1;
    SCRATCH_HEAD(ActorRepelScratch) = blk;
    s                               = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg = 0;
    SCRATCH_POP(ActorRepelScratch);
    return s->hit;
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it. `*pos`
/// accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1`
/// is 1.
s32 Actor01900_Fn008B4(GpCoord* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    pos->vz          = 0;
    pos->vy          = 0;
    pos->vx          = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)coord->workm.t[0];
    s->eye.vy = (u16)coord->workm.t[1];
    s->eye.vz = (u16)coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

s32 Actor01900_Fn00E00(GpCoord* coord, GpRec18* rec, s32 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]   += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]   += s->delta.vz.h.hi;
        Actor01900_D1730C.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor01900_D1730C.vy = s->delta.vy.w >> 16;
        Actor01900_D1730C.vz = s->delta.vz.w >> 16;
        val                  = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor01900_D1730C.vx++;
            } else {
                coord->coord.t[0]--;
                Actor01900_D1730C.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor01900_D1730C.vz++;
            } else {
                coord->coord.t[2]--;
                Actor01900_D1730C.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
s32 Actor01900_Fn00FA4(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, void);
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx                     = (u16)coord->coord.t[0];
    st->eye.vy                     = (u16)coord->coord.t[1];
    vz                             = (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = st;
    st->eye.vz                     = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            dz               = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            dz               = (u16)st->aim.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = SCRATCH_HEAD_ADDR;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Rotates the slot-3 player's and this actor's raised root positions into
/// world space and returns `func_800E0308` on the pair.
s32 Actor01900_Fn016F0(Task* arg0)
{
    Task*              player;
    u8*                head;
    ActorSightScratch* s;
    SVECTOR*           local;
    SVECTOR*           v;
    SVECTOR*           out;

    player           = gameGetPtrSlot(3);
    head             = SCRATCH_HEAD(u8);
    local            = (SVECTOR*)(head - 0xC);
    s                = (ActorSightScratch*)(head - 0x1C);
    s->local.vx      = player->extra.tmd->coords->coord.t[0];
    s->local.vy      = player->extra.tmd->coords->coord.t[1] - 1000;
    SCRATCH_HEAD(u8) = (u8*)s;
    s->local.vz      = player->extra.tmd->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = arg0->extra.tmd->coords->coord.t[0];
    s->local.vy = arg0->extra.tmd->coords->coord.t[1] - 1000;
    s->local.vz = arg0->extra.tmd->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx += gGfxViewCoord.workm.t[0];
    s->from.vy += gGfxViewCoord.workm.t[1];
    s->from.vz += gGfxViewCoord.workm.t[2];
    s->hit      = func_800E0308(&s->out, out);
    SCRATCH_POP_BYTES(0x1C);
    return s->hit;
}

/// Advances the actor's animation one tick. Joints 1-10 are sampled from both
/// the main and the blend animation and passed to `Gp_AnimWritePoseCopy` with
/// weights `field_8AC` and 0x1000 - `field_8AC`; joints 11-18 tick the main
/// animation alone. The per-joint rates come from `field_8A2` and `field_8AA`.
void Actor01900_Fn01950(Task* arg0)
{
    GpAnimPose          pose;
    GpAnimPose          blendPose;
    GpAnimCtx*          anim;
    s16                 weight;
    s16                 i;
    Actor01900AnimWork* work;

    work   = (Actor01900AnimWork*)((Actor01900Work*)arg0->work);
    weight = work->field_8AC;
    anim   = &work->rig.anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = (u8)work->field_8AA;
            work->rig.slots[i].rate   = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blend.anim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
}

s32 Actor01900_Fn01A7C(Actor01900Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0xF) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x14) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x24) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x2C) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xC && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            work->field_8B4 = work->field_5A & 0x3FF;
            break;
        default:
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

/// Per-frame animation driver: services a pending clip change, advances the
/// body and blend animations, eases the head toward its target yaw and emits
/// whatever sound event the current clip has reached.
///
/// `field_898` is the pending-change request: 1 cross-fades into `field_89E`
/// over the table's frame count, 2 restarts it outright, and both settle to 3.
/// `field_8A6` does the same for the blend animation and `field_8A8`.
void Actor01900_Fn01C94(Task* arg0)
{
    Actor01900AnimWork* work;
    Actor01900AnimWork* w1;
    Actor01900AnimWork* w2;
    Actor01900AnimWork* w3;
    GpEnemy*            enemy;
    s16                 cur;
    s16                 dst;
    s16                 raw;
    s32                 clamped;
    s32                 i;
    s32                 i2;
    s32                 i3;
    s32                 i4;
    s32                 snd;
    s32                 id;
    s32                 pan;
    u16                 cur_u;
    u16                 dst_u;

    work  = (Actor01900AnimWork*)((Actor01900Work*)arg0->work);
    enemy = arg0->spawnArg2;
    if (work->field_898 == 1) {
        w1 = work;
        if (work->field_89C != work->field_89E) {
            for (i = 1; i < 0x13; i++) {
                w1->rig.slots[i].rate = (u8)w1->field_8A2;
                func_800B4114(&w1->rig.anim, i, w1->field_89E, 0,
                              (s32)Actor01900_D16988[w1->field_89C][w1->field_89E]);
            }
            /* Keeps this store from being merged with the identical one the
               `field_898 == 2` path makes just below. */
            w1->field_89C = (s16)(u16)w1->field_89E;
        }
        goto block_9;
    }
    if (work->field_898 == 2) {
        w2 = work;
        i2 = 1;
        do {
            w2->rig.slots[i2].rate = (u8)w2->field_8A2;
            Gp_AnimResetSlot(&w2->rig.anim, i2, w2->field_89E);
            i2++;
        } while (i2 < 0x13);
        w2->field_89C = (s16)(u16)w2->field_89E;
    block_9:
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    }
    if (work->field_8A6 == 2) {
        i3            = 1;
        w1            = (Actor01900AnimWork*)((Actor01900Work*)arg0->work);
        w1->field_8AA = 0x30;
        w1->field_8AC = 0x800;
        do {
            w1->rig.slots[i3].rate = (u8)w1->field_8AA;
            Gp_AnimResetSlot(&w1->blend.anim, i3, w1->field_8A8);
            i3++;
        } while (i3 < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if (work->field_89A == 0) {
        w3 = (Actor01900AnimWork*)((Actor01900Work*)arg0->work);
        i4 = 1;
        do {
            w3->rig.slots[i4].rate = (u8)w3->field_8A2;
            Gp_AnimTickIndex(&w3->rig.anim, i4);
            i4++;
        } while (i4 < 0x13);
    } else {
        Actor01900_Fn01950(arg0);
        if (work->blend.slots[1].flags & 0x100) {
            work->field_89A = 0;
        }
    }
    dst   = work->field_8AE;
    cur   = work->field_8B0;
    dst_u = (u16)work->field_8AE;
    cur_u = (u16)work->field_8B0;
    if (dst > cur) {
        if ((dst - cur) >= 0x101) {
            work->field_8B0 = cur_u + 0x100;
        } else {
            goto block_25;
        }
    } else if ((cur - dst) >= 0x101) {
        work->field_8B0 = cur_u - 0x100;
    } else {
    block_25:
        work->field_8B0 = (s16)dst_u;
    }
    raw     = work->field_8B0;
    clamped = (u16)work->field_8B0;
    if (raw != 0) {
        if (raw >= 0x401) {
            clamped = 0x400;
        }
        if (raw < -0x400) {
            clamped = -0x400;
        }
        Actor01900_Fn00260(arg0->extra.tmd->coords + 5, (s16)(((s16)clamped * 2) / 3));
        Actor01900_Fn00260(arg0->extra.tmd->coords + 2,
                           (s16)((s32)((s16)clamped + ((u32)(clamped << 0x10) >> 0x1F)) >> 1));
        arg0->extra.tmd->coords[5].flg = 0;
        arg0->extra.tmd->coords[4].flg = 0;
        arg0->extra.tmd->coords[3].flg = 0;
        arg0->extra.tmd->coords[2].flg = 0;
    }
    snd = Actor01900_Fn01A7C((Actor01900Work*)work);
    if (snd != 0) {
        id  = snd | ((enemy->placeKey >> 0xC) << 8);
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
}

/// Binds the actor model's light and colour matrices to the pair kept in its
/// work block.
static __inline__ void Actor01900_BindMatrices(Task* actor)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->field_BB0;
    obj->colorMtx = &work->field_BD0;
}

/// Enemy init: allocates the work block, sets up both animation contexts,
/// the three hit/body `GpObj` nodes and the patrol points, then picks the
/// starting state from the spawn flags and rescales the model.
void Actor01900_Fn02018(GpEnemy* enemy, Task* actor)
{
    SVECTOR         dir;
    VECTOR          pos;
    SVECTOR*        v;
    TmdObject*      obj;
    GpCoord*        root;
    Actor01900Work* work;
    GpObj*          body;
    GpObj*          head;
    s32             kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(0xC9C, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->exitCallback = Actor01900_Fn0A6CC;
    Actor01900_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->reactionFlags      = 0;
    enemy->hp                 = (s16)Actor01900_D0AC54.hpMax;
    enemy->param              = &Actor01900_D0AC54;
    enemy->recs               = &work->field_8E8;
    func_800B3F84(&((Actor01900AnimWork*)work)->rig.anim, Actor01900_D17174, obj,
                  ((Actor01900AnimWork*)work)->rig.poses, ((Actor01900AnimWork*)work)->rig.slots);
    func_800B3F84(&((Actor01900AnimWork*)work)->blend.anim, Actor01900_D17174, obj,
                  ((Actor01900AnimWork*)work)->blend.poses, ((Actor01900AnimWork*)work)->blend.slots);
    work->field_898 = 2;
    work->field_89A = 0;
    work->field_89E = 2;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    Actor01900_Fn01C94(actor);

    work->field_A08.ctx.recs = &work->field_A28;
    work->field_A08.coord    = root;
    work->field_A08.pos.vx   = 0;
    work->field_A08.pos.vy   = -0x100;
    work->field_A08.pos.vz   = 0;
    work->field_A08.key      = 0x30013;
    work->field_A08.radius   = 0x180;
    work->field_A08.flags    = 1;
    Gp_LinkObj(2, &work->field_A08);
    work->field_C10       = 0;
    work->field_A08.flags = (work->field_A08.flags | 0x4000) & 0x7FFF;
    Gp_InitRec18Table(work->field_A08.ctx.recs, 0xC, 0);

    body           = &work->field_8C8;
    body->coord    = &actor->extra.tmd->coords[2];
    body->ctx.recs = &work->field_8E8;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x30000;
    body->radius   = 0x180;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B48;
    head->coord    = &actor->extra.tmd->coords[4];
    head->ctx.recs = &work->field_B68;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x180;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);
    work->field_B48.key = Gp_PackObjPair(enemy, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->extra.tmd->coords->coord.t[0];
    work->field_C[0].z = actor->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable = &Actor01900_D1728C;
    root->sub       = &gGfxViewCoord;
    root->flg       = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &actor->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    kind                       = actor->spawnArg1 >> 16;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x17;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1 & 0xF) {
        case 2:
            work->field_C2C = Actor01900_D0AC64[0].field_0;
            work->field_C2E = Actor01900_D0AC64[0].field_2;
            work->field_C30 = Actor01900_D0AC64[0].field_4;
            work->field_C32 = Actor01900_D0AC64[0].field_6;
            break;
        case 1:
            work->field_C2C = Actor01900_D0AC64[2].field_0;
            work->field_C2E = Actor01900_D0AC64[2].field_2;
            work->field_C30 = Actor01900_D0AC64[2].field_4;
            work->field_C32 = Actor01900_D0AC64[2].field_6;
            break;
        case 0:
        default:
            work->field_C2C = Actor01900_D0AC64[1].field_0;
            work->field_C2E = Actor01900_D0AC64[1].field_2;
            work->field_C30 = Actor01900_D0AC64[1].field_4;
            work->field_C32 = Actor01900_D0AC64[1].field_6;
            break;
    }

    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->field_C98 = 0;
    actor->state++;
}

/// Spawns the hit-reaction effect for a blow arriving at `yaw`: carves one
/// `SVECTOR` off the scratch head, fills it with one of the twelve presets in
/// `Actor01900_D1722C` picked from the magnitude and sign of `yaw` plus a
/// random draw, hands it to `func_800FDB18` together with the parameter of
/// `id`, and releases the scratch again.
void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id)
{
    SVECTOR*        dir;
    s32             absAng;
    Actor01900Work* work;

    dir    = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    absAng = (yaw >= 0) ? yaw : -yaw;
    work   = arg0->work;
    if (absAng < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *dir = Actor01900_D1722C[0];
                break;
            case 1:
                *dir = Actor01900_D1722C[1];
                break;
            case 2:
                *dir = Actor01900_D1722C[2];
                break;
            case 3:
                *dir = Actor01900_D1722C[3];
                break;
            default:
                *dir = Actor01900_D1722C[4];
                break;
        }
    } else if (absAng > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *dir = Actor01900_D1722C[5];
                break;
            case 1:
                *dir = Actor01900_D1722C[6];
                break;
            default:
                *dir = Actor01900_D1722C[7];
                break;
        }
    } else if (yaw > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[8];
        } else {
            *dir = Actor01900_D1722C[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[10];
        } else {
            *dir = Actor01900_D1722C[11];
        }
    }
    work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, &arg0->extra.tmd->coords[dir->pad], dir, &work->field_8B8);
    SCRATCH_POP_BYTES(8);
}

/// First `GpRec18` among the twelve at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor01900_FindHit(GpRec18* records, SVECTOR* pos)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key;
        }
    }
    return 0;
}

void Actor01900_Fn02A50(Task* arg0)
{
    PlayerStatus*    config = &Player_Status;
    Actor01900Work*  work;
    GpEnemy*         enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GpCoord*         coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s16              state;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = SCRATCH_HEAD(ActorHitScratch);
        s     = (SCRATCH_HEAD(ActorHitScratch) = head - 1);
        s->id = Actor01900_FindHit(&work->field_8E8, &head[-1].hitPos);
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetPtrSlot(3);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            work->field_C40              = 0;
            work->field_C42              = 0;
            arg0->extra.tmd->coords->flg = 0;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            s->dir.vx = arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = arg0->extra.tmd->coords->workm.t[1];
            s->dir.vz = arg0->extra.tmd->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->extra.tmd->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            Actor01900_Fn02664(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                s->m = arg0->extra.tmd->coords->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_C14 > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0] += s->dir.vx;
                arg0->extra.tmd->coords->coord.t[1] += s->dir.vy;
                arg0->extra.tmd->coords->coord.t[2] += s->dir.vz;
                arg0->extra.tmd->coords->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13 && state != 0x11 && state != 0x1F && state != 0xF && state != 4) {
                    damage    = s->damage * 2;
                    s->damage = damage;
                    if (damage != 0) {
                        s->effect = 4;
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_C12 += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[2], effect, NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD) && config->hp > 0 && work->field_C44 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0008;
                deathPan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0007;
                hitPan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            }
            work->field_C10 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    state = work->field_0;
                    if (state == 0x13 || state == 0xF || state == 4 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else if (work->field_C12 >= 0x4C || s->crit == 1) {
                        work->field_B48.flags &= 0x7FFF;
                        state                  = work->field_0;
                        if (state != 0x13 && state != 0x1F && state != 0x11) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    work->field_B48.flags &= 0x7FFF;
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state != 0x11 && state != 4) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    } else {
                        work->field_0 = 4;
                    }
                    break;
                case 3:
                    work->field_B48.flags &= 0x7FFF;
                    if (work->field_0 == 0x17 || work->field_0 == 0x18) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags  &= 0xFE;
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
                case 8:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x1F && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                work->field_0 = 0x13;
                            }
                        }
                    }
                    break;
                case 9:
                    work->field_B48.flags &= 0x7FFF;
                    state                  = work->field_0;
                    if (state != 0x13 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            work->field_0 = 0x13;
                        }
                    }
                    break;
            }
            work->field_C14 = 5;
        } else if (work->field_C14 <= 0) {
            work->field_C12 = 0;
        } else {
            work->field_C14--;
        }
        if (enemy->reactionFlags & 0xC) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
            if (s->damage != 0) {
                work->field_B48.flags &= 0x7FFF;
                enemy->hp             -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                state = work->field_0;
                if (state == 7 || state == 0xB || state == 0x12 || state == 0x1B) {
                    work->field_0 = 5;
                } else if (state == 4) {
                    work->field_2 = -1;
                } else if (state != 0xF) {
                    if (state == 0x13 || state == 0x11) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                        work->field_8A6 = 2;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0xD;
                        work->field_8A6 = 2;
                    }
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    if (work->field_89E == 2 || work->field_89E == 3) {
                        work->field_0 = 0x1E;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 == 4) {
                    work->field_0 = 0x1F;
                } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11) {
                    work->field_0 = 0x13;
                }
            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                work->field_0 = 0x1F;
            } else if (work->field_0 != 0x13 && work->field_0 != 0x1F && work->field_0 != 0x11 && work->field_0 != 0x15 && work->field_0 != 0 && work->field_0 != 0x1D && work->field_0 != 0x1E) {
                work->field_0 = 0x13;
            }
        }
        SCRATCH_POP(ActorHitScratch);
    }
}

void Actor01900_Fn03710(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    s32             step;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                       = arg0->extra.tmd;
        enemy->node.state.b.flags = 0;
        obj->flags                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x17;
        work->field_A08.flags |= 0x4000;
        do {
            Actor01900_Fn01C94(arg0);
        } while ((u32)(work->field_5A & 0x3FF) < 6U);
        work->field_8A2 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->flg = 0;
    step                         = (s16)work->field_8A2 / 2;
    work->field_8A2              = (u16)step;
    if (step == 1) {
        work->field_8A2 = -0x10;
    }
    if ((s16)work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    Actor01900_Fn01C94(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= 0xFD;
        work->field_0         = 0x11;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

/// Raises the player's weapon when the player is not already in state 2 and
/// stands within 0x1F4 of the actor in Y. Nonzero when it armed.
static __inline__ s32 Actor01900_ArmIfPlayerLevel(Task* arg0)
{
    Task* player;
    s32   dy;

    player = gameGetPtrSlot(3);
    if (((GameActor*)player->work)->field_954 != 2) {
        dy = arg0->extra.tmd->coords->coord.t[1] - player->extra.tmd->coords->coord.t[1];
        if (ABS(dy) < 0x1F4) {
            Gp_ArmStateF0(1);
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers, arms the player if
/// they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x10 and rescales it by 0x1194.
void Actor01900_Fn03854(Task* arg0)
{
    Actor01900Work*    work;
    ActorChaseScratch* yaw;
    GpCoord*           coord;
    TmdObject*         obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 1;
        work->field_8A2       = 0x10;
        work->field_89E       = 9;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
        work->field_8C8.radius = 0x180;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
    } else {
        SCRATCH_PUSH(ActorChaseScratch);
        yaw                          = SCRATCH_HEAD(ActorChaseScratch);
        arg0->extra.tmd->coords->flg = 0;
        if (work->field_68 & 0x100) {
            work->field_0 = 7;
        }
        yaw->turn       = actorPositionYaw(arg0, &yaw->delta, &Player_Status);
        work->field_8AE = yaw->turn;
        if (yaw->turn >= 0x11) {
            yaw->turn = 0x10;
        }
        if (yaw->turn < -0x10) {
            yaw->turn = -0x10;
        }
        coord      = arg0->extra.tmd->coords;
        yaw->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, yaw->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        Actor01900_Fn01C94(arg0);
        SCRATCH_POP(ActorChaseScratch);
    }
}

void Actor01900_Fn03C04(GpAreaKey* session, GpCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

/// `Actor01900_Fn03C04`'s row scan without the clamp: nonzero when the
/// current room has an `Actor01900_D172CC` row.
static __inline__ s32 Actor01900_HasHeightClamp(GpAreaKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

s32 Actor01900_Fn03C98(GpCoord* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    ActorStepDelta* head;
    ActorStepDelta* s;
    ActorStepDelta* blk;
    s16             vy;
    SVECTOR*        step;

    if (Mc_SaveData.field_5C1 == 1) {
        return 0;
    }
    head                         = SCRATCH_HEAD(ActorStepDelta);
    blk                          = head - 1;
    SCRATCH_HEAD(ActorStepDelta) = blk;
    s                            = blk;
    s->moved                     = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor01900_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x180) {
                s->step.vy = (vy <= 0) ? -0x180 : 0x180;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xC0) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xC0);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor01900_HasHeightClamp(&gGameSession->at4.loc.view)) {
        Actor01900_Fn03C04(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP(ActorStepDelta);
    return s->moved;
}

/// Pushes the actor's root coordinate by half of each nearby kind 0x10000 /
/// 0x30000 record's offset, walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update is load-bearing: loop.c counts both copies
/// before cross-jumping merges them, which keeps `count`'s sign extension in the loop.
s32 Actor01900_Fn03FF8(Task* arg0, GpRec18* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (Mc_SaveData.field_5C1 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].flg = 0;
    head                           = SCRATCH_HEAD(ActorPushScratch);
    blk                            = head - 1;
    SCRATCH_HEAD(ActorPushScratch) = blk;
    s                              = blk;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
    s->pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0xC0) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0xC0);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            }
            arg0->extra.tmd->coords->flg = 0;
        }
    }
    SCRATCH_POP(ActorPushScratch);
    return s->hit;
}

/// Circling state: turns toward the player at most 0x30 per step while walking,
/// switching to state 0xA when lined up and far enough, 0xB when close and in
/// front, or 0x1B after 0x5B steps.
void Actor01900_Fn042BC(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    GpCoord*           facing;
    ActorChaseScratch* s;
    s32                diff;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x42;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    work->field_6++;
    work->field_8++;
    SCRATCH_PUSH(ActorChaseScratch);
    s = SCRATCH_HEAD(ActorChaseScratch);
    if (Actor01900_Fn03C98(arg0->extra.tmd->coords, &work->field_A28, 0xC, 0x60) != 1) {
        if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_8E8, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
    arg0->extra.tmd->coords->flg = 0;
    Actor01900_Fn01C94(arg0);
    s->playerYaw = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                          (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
    s->yaw          = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw          = actorNormalizeYaw(s->yaw);
    coord           = arg0->extra.tmd->coords;
    s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->turn;
    diff            = s->yaw - s->playerYaw;
    if (ABS(diff) < 0x44 && work->field_C30 + work->field_C42 / 2 < work->field_6 && ABS(s->turn) < 0x80) {
        if (overlayOutOfRange(&s->delta, 0x708)) {
            work->field_0 = 0xA;
        }
    }
    if (Actor01900_Fn016F0(arg0) != 1) {
        work->field_6++;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!overlayOutOfRange(&s->delta, 0x2BC)) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C28 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C28 = -1;
            } else {
                work->field_C28 = 1;
            }
        }
        if (work->field_C28 == 1) {
            s->turn += 0x300;
        } else {
            s->turn -= 0x300;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C28 = -work->field_C28;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = arg0->extra.tmd->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, s->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
        } else {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
        }
    } else if (work->field_68 & 0x100) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_C37 != 0) {
        work->field_C37--;
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn04D14(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    GpCoord*           facing;
    ActorChaseScratch* s;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0xC0;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C26   = 8;
        work->field_6     = 0;
        work->field_8     = 0;
        Actor01900_D172FC = 0;
        work->field_C40++;
        return;
    }
    SCRATCH_PUSH(ActorChaseScratch);
    s                            = SCRATCH_HEAD(ActorChaseScratch);
    arg0->extra.tmd->coords->flg = 0;
    Actor01900_Fn01C94(arg0);
    if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                               (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = actorNormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->extra.tmd->coords;
    s->turn = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                        = arg0->extra.tmd->coords;
    work->field_8AE              = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->flg = 0;
    work->field_C24              = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C24 = work->field_C24 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C24 = 2;
    }
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->field_C24);
    Actor01900_D172FC += work->field_C24;
    if (work->field_C26 == 8 && work->field_8A2 >= 0x18) {
        work->field_C26 = -1;
    }
    if (work->field_C26 == -1 && work->field_8A2 == 0x12) {
        work->field_C26 = 0;
        work->field_6   = 0;
    }
    if (work->field_C26 == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-(gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][0],
                                  (gameGetPtrSlot(3))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = actorNormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw <= 0x400) {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += work->field_C26;
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn0551C(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    GpCoord*           facing;
    ActorChaseScratch* head;
    ActorChaseScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                            = SCRATCH_HEAD(ActorChaseScratch);
        obj                                             = arg0->extra.tmd;
        SCRATCH_HEAD(ActorChaseScratch)                 = head - 1;
        s                                               = head - 1;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing          = arg0->extra.tmd->coords;
        s->angle        = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C20 = s->angle;
        work->field_C22 = s->angle + (u16)s->turn * 2;
        SCRATCH_POP(ActorChaseScratch);
        return;
    }
    head                            = SCRATCH_HEAD(ActorChaseScratch);
    SCRATCH_HEAD(ActorChaseScratch) = head - 1;
    s                               = head - 1;
    Actor01900_Fn01C94(arg0);
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
    if (work->field_C20 == work->field_C22) {
        if (work->field_C40 < 2 || overlayOutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        }
    }
    if (work->field_C20 > work->field_C22) {
        work->field_C20 -= 0x89;
        if (work->field_C20 < work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    if (work->field_C20 < work->field_C22) {
        work->field_C20 += 0x89;
        if (work->field_C20 > work->field_C22) {
            work->field_C20 = work->field_C22;
        }
    }
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, work->field_C20, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn05B4C(Task* arg0)
{
    Actor01900Work*    work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GpCoord*           coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;

    head                            = SCRATCH_HEAD(ActorChaseScratch);
    work                            = arg0->work;
    SCRATCH_HEAD(ActorChaseScratch) = head - 1;
    aim                             = head - 1;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0xC0;
        work->field_6          = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C28 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C28 = 1;
            } else {
                work->field_C28 = -1;
            }
        }
        if (work->field_C28 == 1) {
            work->field_89E = 0x15;
            if (work->field_C42 == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->field_C2E + angle;
            } else {
                aim->turn += work->field_C2E;
            }
            work->field_C28 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C42 == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->field_C2E;
            } else {
                aim->turn -= work->field_C2E;
            }
            work->field_C28 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        Actor01900_Fn01C94(arg0);
        Gfx_RotMatrixY(&mat, aim->turn, 1);
        dir = &work->field_C18;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C2A = 0xDE;
        work->field_C42++;
    }
    arg0->extra.tmd->coords->flg = 0;
    Actor01900_Fn01C94(arg0);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C2A);
        gte_ldsv(&work->field_C18);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C2A >> 1);
        gte_ldsv(&work->field_C18);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 0) {
            work->field_C2A >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
        work->field_2 = -1;
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn05F38(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    GpCoord*        coord;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        work->field_6   = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        coord = arg0->extra.tmd->coords;
        Actor01900_StepForward(coord, -0x78);
        Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC);
        arg0->extra.tmd->coords->flg = 0;
    }
    if (work->field_68 & 0x100) {
        if (enemy->node.state.b.targeted == 1) {
            work->field_0 = 10;
        } else {
            work->field_0 = 6;
        }
    }
}

void Actor01900_Fn06100(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    GpCoord*           facing;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 8;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        return;
    }
    SCRATCH_PUSH(ActorChaseScratch);
    aim = SCRATCH_HEAD(ActorChaseScratch);
    if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->flg = 0;
    Actor01900_Fn01C94(arg0);
    coord           = arg0->extra.tmd->coords;
    aim->turn       = actorNormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = aim->turn;
    if (aim->turn < 0x200) {
        overlayOutOfRange(&aim->delta, 0x384);
    }
    if (aim->turn > 0x40) {
        aim->turn = 0x40;
    }
    if (aim->turn < -0x40) {
        aim->turn = -0x40;
    }
    facing     = arg0->extra.tmd->coords;
    aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn06634(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_8C8.radius    = 0x180;
        work->field_B48.flags    &= 0x7FFF;
        work->field_A08.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_898           = 1;
        work->field_89E           = 0xA;
        work->field_89A           = 0;
        work->field_8A2           = 0x10;
        work->field_8B0           = 0;
        work->field_8AE           = 0;
        if (enemy->hp < 0 && work->field_C34[0] != 1 && work->field_C34[1] != 3 && work->field_C34[2] != 2) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    if (work->field_89E == 0xA) {
        Actor01900_StepForwardHead(arg0->extra.tmd->coords, -0x57);
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_8E8, 0xC);
    Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_68 & 0x100) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            Actor01900_Fn01C94(arg0);
        }
        if ((work->field_68 & 0x100) && work->field_89E == 0xB) {
            work->field_8C8.flags &= 0xBFFF;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & 2) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

void Actor01900_Fn06904(Task* arg0)
{
    Actor01900Work*       work;
    GpEnemy*              enemy;
    TmdObject*            obj;
    GpCoord*              coord;
    ActorScaleRotScratch* blk;
    u8*                   head;
    u8*                   tail;
    void*                 scratch_base;
    s16                   temp_v0;
    s16                   ang;
    s16                   cur;
    s32                   k;
    s32                   sy;
    u16                   temp_v1;
    u16                   m22;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags                = 0;
        work->field_B48.flags     = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags     = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.state.b.flags = 1;
        work->field_6             = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0x13);
                break;
            case 5:
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = 2;
                break;
            case 17:
                Gp_SetLightMode(enemy, 2);
                break;
            case 39:
                arg0->extra.tmd->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                  = 0x1194;
            head                               = scratch_base;
            head                               = *(u8**)(head + 0x3FC);
            coord                              = arg0->extra.tmd->coords;
            blk                                = (ActorScaleRotScratch*)(head - 0x34);
            sy                                 = k - (cur - 0x14) * 0xB;
            SCRATCH_HEAD(ActorScaleRotScratch) = blk;
            ang                                = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                         = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = (u16)((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = (u16)blk->m.m[0][1];
            coord->coord.m[0][2] = (u16)blk->m.m[0][2];
            coord->coord.m[1][0] = (u16)blk->m.m[1][0];
            coord->coord.m[1][1] = (u16)blk->m.m[1][1];
            coord->coord.m[1][2] = (u16)blk->m.m[1][2];
            coord->coord.m[2][0] = (u16)blk->m.m[2][0];
            coord->coord.m[2][1] = (u16)blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = (u16)blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Arms `Gp_StateF0` and returns 1 when the player is within 500 units of the
/// actor's height (and not in `field_954` state 2).
void Actor01900_Fn06B4C(Task* arg0)
{
    SVECTOR         delta;
    SVECTOR*        d;
    Actor01900Work* work;
    GpEnemy*        enemy;
    TmdObject*      obj;
    GpCoord*        coord;
    s32             sound;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj               = arg0->extra.tmd;
        Actor01900_D171B4 = &Actor01900_D16960;
        work->field_89E   = 0x10;
        work->field_898   = 2;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius    = 0x180;
        work->field_B48.flags    &= 0x7FFF;
        work->field_A08.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_8B0           = 0;
        work->field_8A2           = 0x10;
        work->field_8AE           = 0;
        work->field_6             = 0;
        work->field_894           = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_5A & 0x3FF) == 0xF && work->field_894 != (work->field_5A & 0x3FF) &&
        (GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 9, 0, 0)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sound       = 0x51090009;
        if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
            sound = 0x51090008;
        }
        switch ((u8)Gp_GetViewIndex()) {
            case 2:
                SndEvt_EnqueueType6(sound, 0x64, 0);
                break;
            case 3:
                SndEvt_EnqueueType6(sound, 0x50, 0x1F);
                break;
            case 4:
            default:
                SndEvt_EnqueueType6(sound, 0x40, 0x4C);
                break;
        }
    }
    if ((work->field_5A & 0x3FF) == 5 && work->field_894 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = arg0->extra.tmd->coords + 1;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(1, 3, 0, 0) || (u8)Gp_GetViewIndex() != 0x10) {
            func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->field_8B8);
        }
    }
    work->field_894 = work->field_5A & 0x3FF;
    coord           = arg0->extra.tmd->coords;
    d               = &delta;
    delta.vx        = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    d->vy           = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    d->vz           = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, work->field_C32)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
            work->field_0 = 6;
        }
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x20 per step and swapping waypoints on arrival or after 0x15 steps; switches
/// to state 6 when the player comes within `field_C32`, or within 0xFA0 and in
/// front.
void Actor01900_Fn06F40(Task* arg0)
{
    Actor01900Work*   work;
    TmdObject*        obj;
    GpCoord*          coord;
    ActorTurnScratch* s;
    GpCoord*          facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    SCRATCH_PUSH(ActorTurnScratch);
    s           = SCRATCH_HEAD(ActorTurnScratch);
    s->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    s->delta.vy = 0;
    s->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    Actor01900_Fn01C94(arg0);
    coord           = arg0->extra.tmd->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, s->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->field_89A == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
    }
    if ((arg0->spawnArg1 >> 16) != 0x10) {
        if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) == 1 && ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    } else {
        if ((Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) == 1 ||
             Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_8E8, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
    }
    arg0->extra.tmd->coords->flg = 0;
    if (Actor01900_Fn016F0(arg0) != 1) {
        actorConfigPositionDelta(&Player_Status, arg0->extra.tmd->coords, &s->delta);
        if (!overlayOutOfRange(&s->delta, work->field_C32)) {
            if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                work->field_0 = 6;
            }
        } else if (!overlayOutOfRange(&s->delta, 0xFA0)) {
            coord    = arg0->extra.tmd->coords;
            s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                    work->field_0 = 6;
                }
            }
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    SCRATCH_POP(ActorTurnScratch);
}

void Actor01900_Fn07810(Task* arg0)
{
    Actor01900Work*   work;
    GpEnemy*          enemy;
    TmdObject*        obj;
    GpCoord*          coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2;
        obj             = arg0->extra.tmd;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius    = 0x180;
        work->field_B48.flags    &= 0x7FFF;
        work->field_A08.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_8B0           = 0;
        work->field_8A2           = 0x1E;
    }
    SCRATCH_PUSH(ActorTurnScratch);
    turn            = SCRATCH_HEAD(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8AE = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
        Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
    }
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->field_C24);
    if (work->field_C24 > 0) {
        next            = work->field_C24 - 0xA;
        work->field_C24 = next;
        if ((s16)next < 0) {
            work->field_C24 = 0;
        }
    }
    Actor01900_Fn01C94(arg0);
    if ((work->field_68 & 0x100) || work->field_C24 == 0) {
        work->field_0 = 9;
    }
    SCRATCH_POP(ActorTurnScratch);
}

void Actor01900_Fn07BA8(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        return;
    }
    Actor01900_Fn01C94(arg0);
    SCRATCH_PUSH(ActorChaseScratch);
    aim             = SCRATCH_HEAD(ActorChaseScratch);
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        Actor01900_Fn01C94(arg0);
    }
    if (aim->turn > 0x80) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        Actor01900_StepForward(arg0->extra.tmd->coords, -0x10);
        if (Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC) != 1) {
            Actor01900_Fn03FF8(arg0, &work->field_8E8, 0xC);
        }
        arg0->extra.tmd->coords->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn080A8(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        Actor01900_Fn01C94(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    SCRATCH_PUSH(ActorChaseScratch);
    aim                          = SCRATCH_HEAD(ActorChaseScratch);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->turn;
    if (aim->turn > 0) {
        aim->turn = 0;
    }
    if (aim->turn < 0) {
        aim->turn = 0;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    Actor01900_Fn01C94(arg0);
    SCRATCH_POP(ActorChaseScratch);
}

/// Turn the actor toward the player at up to 0x28 per call. Takes a 0x10-byte
/// scratch block from `G_SCRATCH_HEAD` for the offset to the player and the
/// yaw, steps `field_8AE` toward that yaw, then rebuilds the root coordinate's
/// Y rotation from its own facing. The `field_4` branch is the state's entry.
void Actor01900_Fn083E8(Task* arg0)
{
    Actor01900Work*     work;
    TmdObject*          obj;
    GpCoord*            coord;
    GpCoord*            facing;
    GpCoord*            yawCoord;
    PlayerStatus*       cfg;
    ActorChaseScratch*  head;
    ActorChaseScratch*  aim;
    ActorChaseScratch*  next;
    ActorChaseScratch** slot;
    s16                 z;
    s16                 ang;
    s16                 delta;
    s16                 wrapped;
    s16                 yaw16;
    s32                 angle;
    s32                 yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 1;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        Actor01900_Fn01C94(arg0);
        Actor01900_Fn01C94(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }

    cfg  = &Player_Status;
    slot = (ActorChaseScratch**)SCRATCH_HEAD_ADDR;
    head = SCRATCH_HEAD_AT(slot, ActorChaseScratch);

    coord             = arg0->extra.tmd->coords;
    next              = head - 1;
    head[-1].delta.vx = (u16)cfg->coordMtx->t[0] - (u16)coord->coord.t[0];
    SOFT_USE_REG(next);
    aim                                      = next;
    aim->delta.vy                            = (u16)cfg->coordMtx->t[1] - (u16)coord->coord.t[1];
    z                                        = (u16)cfg->coordMtx->t[2] - (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(slot, ActorChaseScratch) = aim;
    aim->delta.vz                            = z;

    facing  = arg0->extra.tmd->coords;
    angle   = ratan2((s32)head[-1].delta.vx, (s32)z);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;
    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    yaw       = wrapped;
    yaw16     = yaw;
    aim->turn = yaw;
    if (work->field_8AE < yaw16) {
        if ((yaw16 - work->field_8AE) >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = yaw;
        }
    } else if ((work->field_8AE - yaw16) >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = yaw;
    }

    yawCoord  = arg0->extra.tmd->coords;
    ang       = ratan2((s32)-yawCoord->coord.m[2][0], (s32)yawCoord->coord.m[2][2]);
    aim->turn = ang;
    Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, (s32)ang, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);

    work->field_898 = 2;
    Actor01900_Fn01C94(arg0);
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn08724(Task* arg0)
{
    SVECTOR         vec;
    GpAreaKey       key;
    GpAreaKey*      sessionKey;
    GpAreaKey*      keyPtr;
    u8              areaByte0;
    GpAreaRec*      rec;
    GpAreaPlace*    entry;
    GpEffWork*      eff;
    TmdObject*      model;
    s32             idx;
    u32             raw;
    u16             next;
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0x80;
        work->field_8C8.radius    = 0x180;
        work->field_A08.flags     = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.state.b.flags = 1;
        work->field_8AE           = 0;
        work->field_6             = 0U;
        vec.vx                    = 0x64;
        vec.vz                    = 0;
        vec.vy                    = 0;
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0x13);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch ((s16)next) {
        case 3:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vz        = 0x64;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 9, 0x200, &vec);
            goto body;
        case 4:
            D_80114B78[0] = &Actor01900_D10B68;
            vec.vy        = 0;
            vec.vx        = 0;
            eff           = Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 12, 0x200, &vec);
        body:
            if (eff != NULL) {
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                raw        = enemy->placeKey;
                model      = eff->task->extra.tmd;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = gGameSession->at4.loc.view;
                idx        = raw >> 12;
                /* Both calls take `&key`. CSE of that address across the first
                   jal costs a callee-saved register; the ROM rematerializes
                   `addiu a0, sp, key` for each call. Same shape as
                   Actor02000_Fn0251C. */
                SOFT_BARRIER();
                keyPtr = &key;
                TOUCH_REG(keyPtr);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyPtr);
                rec          = Gp_GetNestedAreaRec(&key);
                entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
                model->tpage = entry->tpage;
                model->clut  = entry->clut;
                if (model->buffer != NULL) {
                    tmdProcessStream(model);
                    tmdProcessStream(model);
                }
            }
            break;
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

void Actor01900_Fn0892C(Task* arg0)
{
    SVECTOR         vec;
    GpEffWork*      eff;
    Actor01900Work* work;
    GpEnemy*        enemy;
    s16             cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8C8.radius    = 0x180;
        work->field_A08.flags     = (u16)(work->field_A08.flags | 0x4000);
        enemy->node.state.b.flags = 1;
        work->field_8AE           = 0;
        work->field_6             = 0;
        vec.vx                    = 0x64;
        vec.vz                    = 0;
        vec.vy                    = 0;
        work->field_89E           = 2;
        work->field_898           = 1;
        work->field_8A2           = 0x10;
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    work->field_6++;
    switch (work->field_89E) {
        case 2:
            if (work->field_6 >= 0x10 && (work->field_68 & 2)) {
                work->field_89E = 0x18;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            Actor01900_StepForwardHead(arg0->extra.tmd->coords, 0xA);
            Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &Actor01900_D10B68;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                eff           = Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 9, 0x200, &vec);
                actorTintEffect(eff, enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &Actor01900_D10B68;
                eff           = Gp_SpawnEff(0xA0005, arg0->extra.tmd->coords + 1, 0x200, NULL);
                actorTintEffect(eff, enemy);
            }
            break;
        case 0x18:
            if (!(work->field_68 & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0x13);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, 1);
                    Gp_SpawnEff(0x600A5, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = 2;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 39:
                    arg0->extra.tmd->flags = 0x80;
                    work->field_0          = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 2);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 3);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 4);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 5);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 6);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 7);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 8);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 9);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 10);
}

/// Whether any of the three `GpRec18` at `records` carries an id with high
/// word 1, stopping at the first empty record.
static __inline__ s32 Actor01900_HasHit(GpRec18* records)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers and arms the player
/// if they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x30, rescales it by 0x1194, and once the
/// actor is out of range of the player hands the work state on.
void Actor01900_Fn09694(Task* arg0)
{
    Actor01900Work*    work;
    TmdObject*         obj;
    GpCoord*           coord;
    GpCoord*           facing;
    GpCoord*           src;
    ActorChaseScratch* aim;
    ActorChaseScratch* head;
    ActorChaseScratch* next;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x180;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 4;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor01900_Fn01C94(arg0);
        work->field_C40 = 0;
        if (*(u16*)work->field_C34 != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->field_6          = 0;
        work->field_8          = 0;
        work->field_B48.flags &= 0x7FFF;
        return;
    }
    work->field_6++;
    if (work->field_6 == 0x16) {
        work->field_B48.flags |= 0x8000;
    }
    if (work->field_6 == 0x1D) {
        work->field_B48.flags &= 0x7FFF;
    }
    if (Actor01900_HasHit(&work->field_B68) == 1) {
        work->field_B48.flags &= 0x7FFF;
    }

    head              = SCRATCH_HEAD(ActorChaseScratch);
    next              = head - 1;
    src               = arg0->extra.tmd->coords;
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - src->coord.t[0];
    SOFT_USE_REG(next);
    aim            = next;
    next->delta.vy = Player_Status.coordMtx->t[1] - src->coord.t[1];
    next->delta.vz = Player_Status.coordMtx->t[2] - src->coord.t[2];

    SCRATCH_HEAD(ActorChaseScratch) = next;
    arg0->extra.tmd->coords->flg    = 0;
    Actor01900_Fn01C94(arg0);
    if (work->field_6 < 0xE) {
        coord = arg0->extra.tmd->coords;
        aim->turn =
            actorNormalizeYaw(ratan2(next->delta.vx, next->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        facing     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    }
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_68 & 0x100) {
        if (overlayOutOfRange(&aim->delta, 0x2BC)) {
            work->field_0 = 6;
        } else {
            work->field_0 = 0xE;
        }
    }
    SCRATCH_POP(ActorChaseScratch);
}

void Actor01900_Fn09BE8(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_8C8.radius    = 0x180;
        work->field_B48.flags    &= 0x7FFF;
        work->field_A08.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_898           = 2;
        work->field_89E           = 0xB;
        work->field_8A2           = 0x10;
        work->field_8B0           = 0;
        work->field_8AE           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    Actor01900_Fn01C94(arg0);
    Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_8E8, 0xC);
    Actor01900_Fn00E00(arg0->extra.tmd->coords, &work->field_A28, 0xC);
    arg0->extra.tmd->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

void Actor01900_Fn09D3C(GpEnemy* enemy, Task* actor)
{
    VECTOR               pos;
    Actor01900StateTable states;
    Actor01900Work*      work;
    ActorViewScratch*    scratch;
    ActorViewScratch*    head;
    s32                  state;

    work   = actor->work;
    states = Actor01900_D001BC;

    actor->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (Gp_StateF0.field_4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x1E)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x1E) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
        case 2:
            actor->extra.tmd->flags = 0x80;
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            Gp_ClearRec18Occupied(&work->field_B68);
            return;
    }

    head                           = SCRATCH_HEAD(ActorViewScratch);
    SCRATCH_HEAD(ActorViewScratch) = head - 1;
    scratch                        = head - 1;

    if (work->field_C10 > 0) {
        work->field_C10 = (s16)((u16)work->field_C10 - 1);
    } else {
        Actor01900_Fn02A50(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    state         = work->field_0;
    if ((state == 0x1C) || (state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x1E)) {
        work->field_8C8.flags &= 0x7FFF;
        work->field_A08.flags &= 0x7FFF;
    } else {
        work->field_8C8.flags |= 0x8000;
    }
    states.fn[work->field_0](actor);
    Gp_ClearRec18Occupied(&work->field_A28);
    Gp_ClearRec18Occupied(&work->field_8E8);
    Gp_ClearRec18Occupied(&work->field_B68);
    if ((Gp_StateF0.field_3 == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);

    work->field_C48[work->field_C98].vx = scratch->pos.vx;
    work->field_C48[work->field_C98].vy = scratch->pos.vy;
    work->field_C48[work->field_C98].vz = scratch->pos.vz;

    SCRATCH_POP_BYTES(0x18);
    work->field_C98 = (u16)work->field_C98 + 1;
    if (work->field_C98 == 7) {
        work->field_C98 = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_C48[work->field_C98].vx;
        enemy->bodyPos.vy = work->field_C48[work->field_C98].vy;
        enemy->bodyPos.vz = work->field_C48[work->field_C98].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

void Actor01900_Fn0A314(void)
{
}

/// The actor's state handlers, indexed by `Actor01900Work::field_0`; empty
/// slots are states the actor never enters. `Actor01900_Fn09D3C` copies the
/// table to its frame before dispatching.
const Actor01900StateTable Actor01900_D001BC = { {
    Actor01900_Fn0A764,
    Actor01900_Fn0A7C0,
    Actor01900_Fn0A868,
    Actor01900_Fn0A914,
    Actor01900_Fn03710,
    Actor01900_Fn0A9C0,
    Actor01900_Fn03854,
    Actor01900_Fn042BC,
    Actor01900_Fn04D14,
    Actor01900_Fn0551C,
    Actor01900_Fn05B4C,
    Actor01900_Fn09694,
    NULL,
    NULL,
    Actor01900_Fn05F38,
    Actor01900_Fn0AA78,
    NULL,
    Actor01900_Fn0AB1C,
    Actor01900_Fn06100,
    Actor01900_Fn06634,
    NULL,
    Actor01900_Fn06904,
    NULL,
    Actor01900_Fn06B4C,
    Actor01900_Fn06F40,
    Actor01900_Fn07BA8,
    Actor01900_Fn07810,
    Actor01900_Fn080A8,
    Actor01900_Fn083E8,
    Actor01900_Fn08724,
    Actor01900_Fn0892C,
    Actor01900_Fn09BE8,
} };

/// The actor task's dispatcher table, indexed by `Task::state`: spawn
/// (`Actor01900_Fn02018`), a three-frame wait (`Actor01900_Fn0ABA0`), the
/// per-frame tick (`Actor01900_Fn09D3C`) and teardown.
const GpEnemyTaskFuncTable4 Actor01900_D0023C = { {
    Actor01900_Fn02018,
    Actor01900_Fn0ABA0,
    Actor01900_Fn09D3C,
    Gp_DestroyEnemy,
} };

s32 Actor01900_Fn0A31C(Task* arg0, s32 arg1, GpAnimArg* arg2)
{
    Actor01900Work* work = arg0->work;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

s32 Actor01900_Fn0A38C(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject*      obj  = arg0->extra.tmd;
    Actor01900Work* work = arg0->work;

    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

s32 Actor01900_Fn0A44C(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = task->extra.tmd->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

s32 Actor01900_Fn0A49C(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*                  coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                = (ActorsShared80169f74Work*)task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->flg = 0;
    coord                        = task->extra.tmd->coords;
    mx                           = coord->coord.m[2][0];
    mz                           = coord->coord.m[2][2];
    work->yaw                    = ratan2(-mx, mz);
    return 1;
}

s32 Actor01900_Fn0A59C(void)
{
    return 1;
}

s32 Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2)
{
    u16             room;
    u16             state;
    u16             state2;
    Actor01900Work* work;

    work               = arg0->work;
    work->field_C34[0] = ((u8*)arg2)[0];
    work->field_C34[1] = ((u8*)arg2)[1];
    work->field_C34[2] = ((u8*)arg2)[2];
    room               = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                       = 0x1C;
                arg0->extra.tmd->coords->coord.t[0] = -0x595;
                arg0->extra.tmd->coords->coord.t[1] = 0;
                arg0->extra.tmd->coords->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                arg0->extra.tmd->coords->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

void Actor01900_Fn0A6CC(Task* task)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = (Actor01900Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C38 != NULL) {
            taskKill(work->field_C38);
        }
        if (work->field_C3C != NULL) {
            taskKill(work->field_C3C);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void Actor01900_Fn0A764(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 1;
        obj->flags                                      = (u16)(obj->flags | 0x80);
        work->field_B48.flags                           = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags                           = (u16)(work->field_A08.flags & 0xBFFF);
    }
}

void Actor01900_Fn0A7C0(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A868(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A914(Task* arg0)
{
    TmdObject*      obj;
    Actor01900Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->flg = 0;
        Actor01900_Fn01C94(arg0);
    }
}

void Actor01900_Fn0A9C0(Task* arg0)
{
    Actor01900Work* work;
    TmdObject*      obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                             = arg0->extra.tmd;
        ((GpEnemy*)arg0->spawnArg2)->node.state.b.flags = 0;
        obj->flags                                      = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 2;
        work->field_8A2        = 0x12;
        work->field_89E        = 0xD;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
    }
    arg0->extra.tmd->coords->flg = 0;
    Actor01900_Fn01C94(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
}

void Actor01900_Fn0AA78(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags    = 0;
        work->field_8C8.radius    = 0x180;
        work->field_B48.flags    &= 0x7FFF;
        work->field_A08.flags    |= 0x4000;
        enemy->node.state.b.flags = 0;
        work->field_898           = 2;
        work->field_89E           = 8;
        work->field_8B0           = 0;
        work->field_8AE           = 0;
        work->field_8A2           = work->field_8A4;
    }
    Actor01900_Fn01C94(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 7;
    }
}

void Actor01900_Fn0AB1C(Task* arg0)
{
    Actor01900Work* work;
    GpEnemy*        enemy;
    u32             rng;
    s16             timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        rng           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng;
        work->field_6 = ((rng >> 16) & 0xF) + work->field_C2C;
    }
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if (timer < 0) {
        work->field_0 = 0xF;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
}

void Actor01900_Fn0ABA0(GpEnemy* enemy, Task* task)
{
    u16             count;
    Actor01900Work* work;

    work          = (Actor01900Work*)task->work;
    count         = work->field_6 + 1;
    work->field_6 = count;
    if ((s16)count >= 3) {
        task->state++;
    }
}

void Actor01900_Fn0ABE4(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor01900_D0023C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
