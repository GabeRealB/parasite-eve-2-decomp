#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Psy-Q `RotMatrixY`.
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

typedef struct Actor311500Work {
    /// Animation context the block itself begins with: `func_actor_311500_80162F28`
    /// hands the block straight to `func_800B4114` / `Gp_AnimTickIndex`.
    /* 0x000 */ ActorAnimRig19 rig;
    /// List node `func_actor_311500_801630A4` unlinks on the first step of
    /// state 1. Sits directly in front of the collision table.
    /* 0x43C */ GpObj field_43C;
    /// One-entry contact table the enemy record points at; the damage check
    /// looks here for a hit.
    /* 0x45C */ GpRec18 rec18[1];
    /* 0x474 */ MATRIX  light;
    /* 0x494 */ MATRIX  color;
    /* 0x4B4 */ Task*   field_4B4;
    /* 0x4B8 */ MATRIX* field_4B8;
    /// The model's `flags` as they were when mode 2 began hiding it, put back
    /// by mode 0 while the mode it last recorded is nonzero.
    /* 0x4BC */ u32 field_4BC;
    /// Sub-state of the phase handler running this frame (idle, hit reaction
    /// or death); each phase change in `func_actor_311500_80163334` resets it
    /// to 0.
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ byte pad_4C2[0x2];
    /// Frame counter within a sub-state: the length of the idle pause, which
    /// ends after 0x1F frames, and the clock of the death sequence.
    /* 0x4C4 */ s16  field_4C4;
    /* 0x4C6 */ byte pad_4C6[0x2];
    /// Idle animation plays since the last pause. Each play that reaches its
    /// clip end raises it; from 2 on the idle always pauses instead of
    /// replaying, and the pause clears it.
    /* 0x4C8 */ s16  field_4C8;
    /* 0x4CA */ byte pad_4CA[0x2];
    /* 0x4CC */ s32  field_4CC;
    /* 0x4D0 */ s32  field_4D0;
    /* 0x4D4 */ u16  field_4D4;
    /// `Gp_StateF0.field_4` as the previous frame saw it, so a mode change can be
    /// detected.
    /* 0x4D6 */ u16 field_4D6;
} Actor311500Work;
STATIC_ASSERT_SIZEOF(Actor311500Work, 0x4D8);

extern GpPairSrcE D_actor_311500_801692C0;
extern u8         D_actor_311500_801692F4[];
extern u32        D_actor_311500_80169330;

extern s32 D_80181E74;
extern s32 D_80181EC4;
extern s32 D_8018207C;
extern s32 D_actor_311500_801692FC;
extern s32 D_actor_311500_80169304;
extern s32 D_actor_311500_80169324;

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off `G_SCRATCH_HEAD`, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1` is 1.
s32 func_actor_311500_80161E38(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
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
/// nudges both `coord`'s translation and `*pos` a short step away from it.
/// `*pos` accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `Mc_SaveData.field_5C1`
/// is 1.
s32 func_actor_311500_80162180(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
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

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
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

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_311500_801626CC(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

void func_actor_311500_801629D8(Task* arg0)
{
    Actor311500Work* work;
    Actor311500Work* work2;
    Actor311500Work* work3;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coords;
    TmdObject*       tmd;
    GpAreaPlace*     place;
    s32              i;
    u8               rate;

    coords     = ((TmdObject*)arg0->extra)->coords;
    enemy      = arg0->spawnArg2;
    tmd        = arg0->extra;
    work       = (Actor311500Work*)memCalloc(0x4D8, 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work2 = (Actor311500Work*)arg0->work;
    Mem_Set(work2, 0, 0x4D8);
    coords->sub = &gGfxViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx = &work2->light;
    tmd->colorMtx = &work2->color;
    tmd->flags    = 0;
    func_800B3F84(&work2->rig.anim, D_actor_311500_801692F4, tmd, work2->rig.poses,
                  &work2->rig.slots[0]);
    work2->field_4B4 = gameGetPtrSlot(3);
    work2->field_4B8 = Player_Status.coordMtx;
    rate             = 0x10;
    i                = 1;
    work3            = (Actor311500Work*)arg0->work;
    do {
        work3->rig.slots[i & 0xFFFF].rate = rate;
        Gp_AnimResetSlot(&work3->rig.anim, i & 0xFFFF, 0);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)arg0->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->hp                 = 0x32;
    enemy->node.state.b.flags = 0;
    enemy->reactionFlags      = 0;
    enemy->param              = &D_actor_311500_801692C0;
    work2->field_43C.coord    = &((TmdObject*)arg0->extra)->coords[2];
    work2->field_43C.ctx.recs = &work2->rec18[0];
    work2->field_43C.pos.vx   = 0;
    work2->field_43C.pos.vy   = 0;
    work2->field_43C.pos.vz   = 0;
    work2->field_43C.key      = 0x3000A;
    work2->field_43C.radius   = 0x190;
    work2->field_43C.flags    = 1;
    Gp_LinkObj(2, &work2->field_43C);
    work2->field_43C.flags |= 0x8000;
    Gp_InitRec18Table(&work2->rec18[0], 1, 0);
    enemy->recs      = &work2->rec18[0];
    arg0->msgTable   = &D_actor_311500_80169330;
    work2->field_4D4 = 1;
    place            = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    while (place->entryId != 0xFF && place->entryId != 0xA) {
        place++;
    }
    Gp_SetTmdBytes(tmd, (s8)place->tpage, (s8)place->clut);
}

void func_actor_311500_80162C34(Task* arg0, TmdObject* arg1)
{
    Actor311500Work* work;
    Actor311500Work* anim;
    Actor311500Work* anim2;
    SVECTOR          probe;
    s32              i;
    u32              rng;
    u16              v;
    u16              count;
    u8               rate;

    work = arg0->work;

    switch (work->field_4C0) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rng         = (u32)Gp_LcgState >> 16;
            if (work->field_4C8 >= 2) {
                work->field_4C0 = (u16)work->field_4C0 + 1;
            } else if (rng & 1) {
                work->field_4C0 = (u16)work->field_4C0 + 1;
            } else {
                rate = 0x20;
                anim = arg0->work;
                i    = 1;
                do {
                    anim->rig.slots[i & 0xFFFF].rate = rate;
                    Gp_AnimResetSlot(&anim->rig.anim, i & 0xFFFF, 0);
                    i += 1;
                } while (((u32)(i & 0xFFFF)) < 0x13U);
                work->field_4C0 = (u16)work->field_4C0 + 2;
            }
            work->field_4C4 = 0;
            break;

        case 1:
            count           = (u16)work->field_4C4;
            work->field_4C4 = count + 1;
            if ((s16)count >= 0x1F) {
                work->field_4C8 = 0;
                work->field_4C0 = 0;
            }
            break;

        case 2:
            anim2 = work;
            i     = 1;
            do {
                Gp_AnimTickIndex(&anim2->rig.anim, i & 0xFFFF);
                i += 1;
            } while (((u32)(i & 0xFFFF)) < 0x13U);
            if (!(anim2->rig.slots[1].flags & 1)) {
                SOFT_BARRIER();
                v = 0;
            } else {
                v = 1;
            }
            if (v) {
                work->field_4C0 = 0;
                work->field_4C8 = (u16)work->field_4C8 + 1;
            }
            break;

        default:
            break;
    }
}

s16 func_actor_311500_80162DDC(Task* arg0)
{
    Actor311500Work* work = arg0->work;
    GpEnemy*         enemy;
    GpRec18*         recs;
    SVECTOR          pos;
    SVECTOR*         pp;
    s32              v;
    s32              damage;
    s16              i;

    enemy = arg0->spawnArg2;
    pp    = &pos;
    recs  = work->rec18;
    for (i = 0; i < 1; i++) {
        if (recs[i].key == 0) {
            break;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pp->vx = recs[i].point.vx;
            pp->vy = recs[i].point.vy;
            pp->vz = recs[i].point.vz;
            v      = recs[i].key;
            goto done;
        }
    }
    v = 0;
done:
    work->field_4CC = v;
    if (work->field_4CC != 0) {
        work->field_4D0 = v;
        damage          = Gp_ComputeDamage(work->field_4CC, 0, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, work->field_4CC, 0) != 0) {
            damage *= 5;
            Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 0, 0);
        }
        enemy->hp -= damage;
        Gp_ClearRec18Occupied(work->rec18);
        func_800DA6E8(&enemy->node, damage, 0);
    }
    return work->field_4CC;
}

/* clang-format off */
s32 func_actor_311500_80162F28(Task* arg0)
{
  Actor311500Work *work;
  Actor311500Work *anim;
  Actor311500Work *anim2;
  GpEnemy *enemy;
  SVECTOR pos;
  GpEffArg eff;
  s32 i;
  s32 var_v0;
  u16 var_v1;
  work = arg0->work;
  enemy = arg0->spawnArg2;
  switch (work->field_4C0)
  {
    case 0:
      anim = work;
      i = 1;
      do
    {
      func_800B4114(&anim->rig.anim, i & 0xFFFF, 1, 0, 0xA);
      i += 1;
    }
    while (((u32) (i & 0xFFFF)) < 0x13U);
      anim2 = arg0->work;
      eff.coord = &((TmdObject*)arg0->extra)->coords[2];
      eff.spawnArgLo = 0x100;
      eff.spawnArgHi = 2;
      pos.vx = 0x3C;
      pos.vy = -0xC;
      pos.vz = 0x1E;
      func_800FDB18(Gp_GetIdParam1(anim2->field_4D0) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[2], &pos, &eff);
      if (enemy->hp > 0)
    {
      work->field_4C0 = ((u16) work->field_4C0) + 1;
      goto block_12;
    }
      return -1;

    case 1:
      i = 1;
      do {
      Gp_AnimTickIndex(&work->rig.anim, i & 0xFFFF);
      i += 1;
      } while (((u32) (i & 0xFFFF)) < 0x13U);
      var_v1 = 1;
      if (!(work->rig.slots[1].flags & 1)) {
      asm("");
      var_v1 = 0;
      }
      var_v0 = 1;
      if (var_v1) {
      return var_v0;
      }
      break;

    return 0;
    default:

  }

  block_12:
  var_v0 = 0;

  return var_v0;
}
/* clang-format on */

s32 func_actor_311500_801630A4(Task* arg0)
{
    Actor311500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    MATRIX           mtx;
    VECTOR           scale;
    u16              m22;
    s32              state;
    s16              cur;
    s32              sy;
    s16              ang;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    state = work->field_4C0;

    switch (state) {
        case 0:
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(0x400A0008, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            work->field_4C4 = 0;
            work->field_4C0 = ((u16)work->field_4C0) + 1;
            break;

        case 1:
            switch (work->field_4C4) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->field_43C);
                    enemy->node.state.b.flags = state;
                    break;

                case 0xA:
                    Gp_SpawnEff(0x600A5, &((TmdObject*)arg0->extra)->coords[2], 3, NULL);
                    Gp_SetLightMode(enemy, 1);
                    break;

                case 0x16:
                    Gp_SetLightMode(enemy, 2);
                    break;

                case 0x1C:
                    ((TmdObject*)arg0->extra)->flags = 2;
                    break;

                case 0x50:
                    ((TmdObject*)arg0->extra)->flags = 0x80;
                    break;

                case 0x104:
                    return 1;
            }

            cur = work->field_4C4;
            if (cur >= 6) {
                coord = ((TmdObject*)arg0->extra)->coords;
                sy    = 0x1000 - (cur - 0x14) * 0xA;
                ang   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                Gfx_RotMatrixY(&mtx, ang, 1);
                scale.vx = 0x1000;
                scale.vy = (s16)sy;
                scale.vz = 0x1000;
                ScaleMatrix(&mtx, &scale);

                m22                  = *(u16*)&mtx.m[0][0];
                coord->coord.m[0][0] = m22;
                m22                  = *(u16*)&mtx.m[0][1];
                coord->coord.m[0][1] = m22;
                m22                  = *(u16*)&mtx.m[0][2];
                coord->coord.m[0][2] = m22;
                m22                  = *(u16*)&mtx.m[1][0];
                coord->coord.m[1][0] = m22;
                m22                  = *(u16*)&mtx.m[1][1];
                coord->coord.m[1][1] = m22;
                m22                  = *(u16*)&mtx.m[1][2];
                coord->coord.m[1][2] = m22;
                m22                  = *(u16*)&mtx.m[2][0];
                coord->coord.m[2][0] = m22;
                m22                  = *(u16*)&mtx.m[2][1];
                coord->coord.m[2][1] = m22;
                m22                  = *(u16*)&mtx.m[2][2];
                coord->flg           = 0;
                coord->coord.m[2][2] = m22;
            }

            work->field_4C4 = ((u16)work->field_4C4) + 1;
            break;

        default:
            return 0;
    }
    return 0;
}

/// Per-frame update. `Task::state` is the actor's phase: 0 sets the actor up,
/// 1 idles until it is hit, 2 plays the hit reaction and returns to 1 or, once
/// the hit points are gone, goes on to 3, which runs the death sequence; 4
/// does nothing.
void func_actor_311500_80163334(Task* arg0)
{
    Task*            actor = arg0;
    Actor311500Work* work;
    Actor311500Work* anim;
    GpEnemy*         enemy;
    TmdObject*       obj;
    VECTOR           pos;
    s32              state;
    s32              i;
    s32              pan;

    work  = actor->work;
    obj   = actor->extra;
    state = Gp_StateF0.field_4;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto case1;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto case1;

case0:
    if (work->field_4D6 != 0) {
        obj->flags = work->field_4BC;
    }
    switch (actor->state) {
        case 0:
            Mem_CopyUnaligned(&D_actor_311500_80169304, &D_80181EC4, 0x20);
            Mem_CopyUnaligned(&D_actor_311500_801692FC, &D_80181E74, 8);
            Mem_CopyUnaligned(&D_actor_311500_80169324, &D_8018207C, 0xC);
            func_actor_311500_801629D8(actor);
            work = actor->work;
            anim = work;
            i    = 1;
            do {
                Gp_AnimTickIndex(&anim->rig.anim, i & 0xFFFF);
                i += 1;
            } while (((u32)(i & 0xFFFF)) < 0x13U);
            actor->state += 1;
            goto case1;

        case 1:
            func_actor_311500_80162C34(actor, obj);
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                pan = (s8)Gp_GetObjPan(((TmdObject*)actor->extra)->coords);
                SndEvt_EnqueueType6(0x400A0007, pan,
                                    (s8)gpGetObjDepth(((TmdObject*)actor->extra)->coords));
                work->field_4C0 = 0;
                actor->state   += 1;
            }
            Gp_ClearRec18Occupied(work->rec18);
            goto case1;

        case 2:
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                work->field_4C0 = 0;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) > 0) {
                work->field_4C0 = 0;
                actor->state   -= 1;
                goto case1;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) < 0) {
                Mem_Set(&D_80181EC4, 0, 0x20);
                Mem_Set(&D_80181E74, 0, 8);
                Mem_Set(&D_8018207C, 0, 0xC);
                work->field_4D4 = 0;
                work->field_4C0 = 0;
                actor->state   += 1;
            }
            goto case1;

        case 3:
            if ((func_actor_311500_801630A4(actor) << 0x10) != 0) {
                actor->state += 1;
                return;
            }
            goto tail;

        case 4:
            return;
    }
    goto case1;

case2:
    if (work->field_4D6 != state) {
        work->field_4BC = obj->flags;
    }
    ((TmdObject*)actor->extra)->flags |= 0x80;
    goto case1;

case1:
    work->field_4D6 = Gp_StateF0.field_4;
tail:
    enemy = actor->spawnArg2;
    Gp_UpdateCoord(&((TmdObject*)actor->extra)->coords[1]);
    pos.vx = ((TmdObject*)actor->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)actor->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)actor->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    ((TmdObject*)actor->extra)->coords->flg = 0;
}

void func_actor_311500_801636A0(Task* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = ((Actor311500Work*)arg0->work)->field_4D4;
}
