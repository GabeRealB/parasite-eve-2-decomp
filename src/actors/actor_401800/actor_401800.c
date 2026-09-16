#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_401800.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801323D4);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013271C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80132C68);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80132E0C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133558);

/// Per-frame animation tick: walk the actor's pose slots, copy each slot's
/// state byte down by three and, for the first ten slots, blend the pose pair
/// `func_800B3448` builds out of the pose and blend contexts — weighted by
/// `field_8AC` against its `0x1000` complement. Slots `0xB` and up only carry
/// the state byte and are advanced by `Gp_AnimTickIndex`.
/// Same body as `func_actor_401000_80132A84`.
void func_actor_401800_801337EC(Actor401800* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor401800AnimWork* work;

    work   = (Actor401800AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AA;
            work->slots[i].field_9      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133918);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133B78);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80133EB8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013423C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801348A8);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80134C94);

/// Walk-state body, split on the live flag. Live: hand the model back to
/// `Tmd_AllocBuffers`, restart the 0x898 slot, ramp `field_8A2` to 0x10, remap
/// the state at 0x89E (11 -> 0x17, 12/25 -> 0x18, anything else -> 0x17) and
/// hold the two `field_5A` countdowns open until the step helper has run its
/// course, then drop `field_8A2` to 0x20. Dead: clear the model's coordinate
/// flag, halve `field_8A2` with the 1 / -1 wrap, and once `Gp_TickObjFlag2`
/// reports 1 clear the enemy's node bit 1 and move to state 0x11.
/// Same body as `func_actor_401300_80135DDC`.
void func_actor_401800_80135DAC(Actor401800* arg0)
{
    Actor401800Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                 = arg0->field_2C;
        enemy->node.field_4 = 0;
        tmd->field_C        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A08.flags |= 0x4000;
        if (work->field_89E == 11) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            func_actor_401800_80133EB8(arg0);
        } while (!(work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) &&
                 !(work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9));
        work->field_8A2 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    work->field_8A2              = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    func_actor_401800_80133EB8(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->field_4C &= ~2;
        work->field_0    = 0x11;
    }
}

/// Inline bodies behind `func_actor_401800_80135F58`, which is the same
/// aim-then-rescale shape as `Actor01900_Fn080A8` / `func_actor_401300_80136238`
/// with these inlined rather than called. Splitting the `G_SCRATCH_HEAD` loads
/// out this way is what keeps each access out of a register CSE would share.
static __inline__ void Actor401800_ConfigPositionDelta(WipSysConfig* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->field_4->t[0] - coord->coord.t[0];
    pos->vy = config->field_4->t[1] - coord->coord.t[1];
    pos->vz = config->field_4->t[2] - coord->coord.t[2];
}

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor401800_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor401800_PositionYaw(Actor401800* actor, SVECTOR* pos, WipSysConfig* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor401800_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401800_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw`.
static __inline__ void Actor401800_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401800RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401800RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401800RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Aim the actor at the player and rescale its root coordinate. On the live
/// flag it resets the model buffers and hands back the pose the actor was
/// running; otherwise it takes a 0x10 scratch for the player offset and the
/// clamped turn, folds the turn into the coordinate's Y rotation and rebuilds
/// the matrix from the new yaw at scale 0x1194.
/// Same body as `Actor01900_Fn080A8`, with the aim and rescale helpers inlined.
void func_actor_401800_80135F58(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_8C8.field_1C = 0x12C;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401800_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_8AE = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Push the root coordinate out of a `GpRec18` table: take a 0x34 scratch, seed
/// its position from the second coordinate, then walk the records until `count`
/// or a zero `field_4`. Each kind 0x10000 / 0x30000 record contributes half its
/// offset along X and Z, normalised to length 0x96 first when it is longer than
/// that; `hit` reports whether one was seen.
/// Same body as `Actor01900_Fn03FF8` / `func_actor_401300_80132910`, with the
/// coordinate update written out in both arms of the length test.
s32 func_actor_401800_8013629C(Actor401800* arg0, GpRec18* recs, s16 count)
{
    Actor401800PushScratch* head;
    Actor401800PushScratch* s;
    Actor401800PushScratch* blk;

    if (D_80072729 == 1 || Game_Session->field_4D == 1) {
        return 0;
    }
    arg0->field_2C->field_8[1].flg            = 0;
    head                                      = *(Actor401800PushScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor401800PushScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
    s->pos.vx = arg0->field_2C->field_8[1].workm.t[0];
    s->pos.vy = arg0->field_2C->field_8[1].workm.t[1];
    s->pos.vz = arg0->field_2C->field_8[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].field_4 == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x96) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x96);
                gte_ldsv(&s->offset);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&s->offset);
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz / 2;
            }
            arg0->field_2C->field_8->flg = 0;
        }
    }
    *(Actor401800PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80136560);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80136EAC);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80137714);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80137DDC);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_801381E4);

/// Live-actor body: arms the animation slots and the two `field_8C8` /
/// `field_A08` nodes, then aims the actor at the `Game_GetPtrSlot(3)` task's
/// root position — the XZ offset normalized by `VectorNormalSS` and GPF-scaled
/// by 0x3E8, the heading taken through `ratan2` — sends it as message 0x3E9
/// and spawns the 0xC/8/0x8F pad-lerp. On work flag bit 0 while `field_89E` is
/// 5, restarts the actor's model (`field_0 = 0xD`, the 0x8B8 effect record for
/// the second coordinate). Same shape as `func_actor_401300_80138800`.
void func_actor_401800_80138C28(Actor401800* arg0)
{
    SVECTOR          dir;
    Actor401800Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                    = Game_GetPtrSlot(3);
        work->field_8C8.field_1C                  = 0x12C;
        work->field_B48.flags                    &= 0x7FFF;
        work->field_A08.flags                    |= 0x4000;
        enemy->node.field_4                       = 0;
        work->field_898                           = 1;
        work->field_8A2                           = 0x10;
        work->field_89E                           = 5;
        ((TmdObject*)player->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->field_8);
        D_actor_401800_80155AD8.field_0.vx = ((TmdObject*)player->extra)->field_8->coord.t[0];
        D_actor_401800_80155AD8.field_0.vy = ((TmdObject*)player->extra)->field_8->coord.t[1];
        D_actor_401800_80155AD8.field_0.vz = ((TmdObject*)player->extra)->field_8->coord.t[2];
        pdir                               = &dir;
        dir.vx                             = ((GpCoordXZ*)arg0->field_2C->field_8)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18;
        dir.vy                             = 0;
        dir.vz                             = ((GpCoordXZ*)arg0->field_2C->field_8)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->field_8->coord.t[0] = ((TmdObject*)player->extra)->field_8->coord.t[0] + dir.vx;
        arg0->field_2C->field_8->coord.t[2] = ((TmdObject*)player->extra)->field_8->coord.t[2] + dir.vz;
        arg0->field_2C->field_8->flg        = 0;
        D_actor_401800_80155AD8.field_10.vx = 0;
        D_actor_401800_80155AD8.field_10.vy = ratan2(dir.vx, dir.vz);
        D_actor_401800_80155AD8.field_10.vz = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_401800_80155AD8, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    if (work->field_89E == 5 && (work->field_68 & 1)) {
        work->field_0           = 0xD;
        work->field_8B8.field_0 = &arg0->field_2C->field_8[1];
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], 0, &work->field_8B8);
    }
}

/// On the live-actor flag, raises the three animation slots, sends the `0x3FF`
/// animation record and the `0x3F9` object pair to the `Game_GetPtrSlot(3)`
/// task, then spawns the 5/0xFF/8 pad-lerp. On work flag bit 0, restarts the
/// actor's model (`field_0 = 0xE`, the 0x8B8 effect record for the second
/// coordinate) and finally copies the `field_5A` clip id into `field_894` and
/// rebuilds the four coordinate parts the actor draws from.
void func_actor_401800_80138F5C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401800_80155A0C;
        msg->field_4    = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = Game_GetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_0           = 0xE;
        work->field_8B8.field_0 = &arg0->field_2C->field_8[1];
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], 0, &work->field_8B8);
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor01900_MoveForward`.
static __inline__ void Actor401800_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401800_MoveForwardNonzero` with the step applied through `vec`
/// rather than a second name for it. Same body as `Actor01900_StepForwardHead`.
static __inline__ void Actor401800_StepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12_real();
            gte_stsv(vec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Per-frame body of the live actor while it walks: on work flag bit 0 it
/// raises the `0x10`/7/2 render slots, re-sends the `0x3FF` animation record
/// with clip 3 to the `Game_GetPtrSlot(3)` task and seeds the walk step
/// `field_C04` to -0x78; otherwise, while the `field_5A` clip is one of
/// `0x10..0x16`, it advances the actor along its own local Z by `field_C04`
/// once `func_actor_401800_80133558` says the path is still clear and halves
/// that step each time the `field_A28` contact fires. Both paths then tick the
/// animation, and — on work bit 0 — pick `field_0` from the enemy's state byte
/// (`6`, or `0xA` when the enemy is not the one `func_actor_401800_80133918`
/// reports) and release the `0x3F1` message once.
void func_actor_401800_80139118(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    WipSysConfig*    config;
    u8               kind;

    work   = arg0->field_1C;
    enemy  = arg0->field_20;
    config = &Wip_SysConfig;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
        D_actor_401800_80155A0C.field_4 = 3;
        if (config->field_18 > 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&D_actor_401800_80155A0C, 0);
        }
        work->field_C04        = -0x78;
        work->field_6          = 0;
        work->field_A08.flags |= 0x4000;
        return;
    }
    if ((Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) && (config->field_18 > 0) && (work->field_C20 == 1)) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C20 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7) {
        if ((s16)func_actor_401800_80133558(arg0->field_2C->field_8, 0x12C, work->field_C04) != 0) {
            Actor401800_MoveForwardNonzero(arg0->field_2C->field_8, work->field_C04);
        }
        if (func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC) == 1) {
            work->field_C04 = work->field_C04 / 2;
        }
        arg0->field_2C->field_8->flg = 0;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        kind = enemy->node.field_5;
        if (kind == 1) {
            if (func_actor_401800_80133918(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if ((config->field_18 > 0) && (work->field_C20 == 1)) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C20 = 0;
        }
    }
}

/// Per-frame body of the live actor armed into state 1: raises the same
/// animation slots as `func_actor_401800_8013971C` but leaves `field_89E = 0xA`
/// (with `field_898 = 1` and `field_89A` cleared), then, while that slot is
/// still `0xA`, advances the actor along its own local Z by a fixed `-0x57`
/// once `func_actor_401800_80133558` says the path is clear. The `0xA` branch
/// then flips the slots to `0xB`/2 and ticks the animation a second time before
/// the two contact records are rebuilt, after which work bit 0 picks `field_0`
/// from the enemy's HP sign and its `field_4C` bit 1.
void func_actor_401800_8013945C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 1;
        work->field_89E          = 0xA;
        work->field_89A          = 0;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    if ((work->field_89E == 0xA) && ((s16)func_actor_401800_80133558(arg0->field_2C->field_8, 0x12C, -0x57) != 0)) {
        Actor401800_StepForward(arg0->field_2C->field_8, -0x57);
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) && (work->field_89E == 0xA)) {
        work->field_89E = 0xB;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
    }
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if ((work->field_68 & 1) && (work->field_89E == 0xB)) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Per-frame body of the live actor: arms the animation slots and the two
/// `field_8C8` / `field_A08` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `field_0` from the
/// enemy's HP sign and its `field_4C` bit 1. Same body as `Actor01900_Fn09BE8`.
void func_actor_401800_8013971C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0xB;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Second per-frame body of the live actor: as `func_actor_401800_8013971C`,
/// but it arms the animation slots with `field_89E = 0x19` and skips the
/// `field_5A` clip rebuild.
void func_actor_401800_80139870(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0x19;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Third per-frame body of the live actor: `func_actor_401800_8013971C` with
/// the animation slots armed at 1 / 0xC, and its `field_0` selector driven by
/// work bit 0 instead of bit 8. Same body as `func_actor_401800_8013971C`
/// apart from those three constants.
void func_actor_401800_801399C4(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 1;
        work->field_89E          = 0xC;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 1) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Grow-and-settle body of the live actor: while its flag is set it clears the
/// `field_C` overlay, drops the two `GpObj` flag bits the previous body raised,
/// marks the enemy node live and restarts the step counter. The counter then
/// runs to 0x401, firing the light-mode and `0x600A5` effect cues as it crosses
/// steps 0x18, 0x1D, 0x29, 0x2F and 0x3F, and from step 0x1A on rebuilds the
/// root coordinate's Y rotation from its current yaw at scale 0x1194 with the
/// Y component shedding 0xB a step. Same body as `Actor01900_Fn06904` with the
/// `Gp_ReleaseStateF0Add` argument and the actor types changed.
void func_actor_401800_80139B18(Actor401800* arg0)
{
    Actor401800Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    s16                    temp_v0;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    temp_v1;
    u16                    m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4   = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->field_8 + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->field_C = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                        = 0x1194;
            head                                     = scratch_base;
            head                                     = *(u8**)(head + 0x3FC);
            coord                                    = arg0->field_2C->field_8;
            blk                                      = (Actor401800RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor401800RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor401800RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor401800RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_80139D60);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401800_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401800RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401800RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401800RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401800RangeScratch*)(head - 0xC))->dx *= ((Actor401800RangeScratch*)(head - 0xC))->dx;
    *(Actor401800RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401800RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Walking body: on the live-actor flag re-allocates the model's buffers,
/// hands the actor the `D_actor_401800_80155124` animation block and zeroes the
/// step counter and the 0x8A2..0x8B0 pose slots, otherwise plays the actor's
/// 0x51030008 spawn sound once on the first frame. After the shared per-frame
/// tick, a `field_5A` state of 4 that differs from the last handled one
/// (`field_8B4`) sends the 0x200-scale effect for the second coordinate part.
/// Then, if the squared XZ offset to the camera target fits inside
/// `field_C0E`, the actor plays 0x51030008 and arms `Gp_StateF0` in state 6 —
/// bit 0x50000 of `Gp_StateF0` arms it the same way. Same shape as
/// `Actor01900_Fn06B4C` and `func_actor_401300_801397F8`.
void func_actor_401800_8013A034(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_401800_80155978 = &D_actor_401800_80155124;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->field_C            = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x10;
        work->field_8AE          = 0;
        work->field_6            = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_6 = 1;
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C0E)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013A2E8);

/// Aim the actor at the player, fold the clamped turn into the root
/// coordinate's Y rotation, then step it along its own local Z while
/// `func_actor_401800_80133558` says the path is clear — reloading
/// `field_0 = 9` once the `field_BFC` step countdown runs out. Same body as
/// `func_actor_401300_8013A208`, with the aim and step helpers inlined.
void func_actor_401800_8013AB64(Actor401800* arg0)
{
    Actor401800Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401800TurnScratch* turn;
    u16                     next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x1E;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401800TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401800_PositionYaw(arg0, &turn->delta, &Wip_SysConfig);
    work->field_8AE                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->field_8;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, turn->angle, 1);
    if (func_actor_401800_80132C68(arg0->field_2C->field_8, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    if ((s16)func_actor_401800_80133558(arg0->field_2C->field_8, 0x12C, work->field_BFC) != 0) {
        Actor401800_MoveForwardNonzero(arg0->field_2C->field_8, work->field_BFC);
    }
    if (work->field_BFC > 0) {
        next            = work->field_BFC - 0xA;
        work->field_BFC = next;
        if ((s16)next < 0) {
            work->field_BFC = 0;
        }
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) || work->field_BFC == 0) {
        work->field_0 = 9;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013AF1C);

/// Aim the actor at the player and rescale its root coordinate. Same body as
/// `Actor01900_Fn080A8`, and as `func_actor_401800_80135F58` apart from the
/// live-flag block's constants and mask, the 0x12C it arms `field_8C8` with,
/// and a turn clamp whose two tests both write zero where its sibling clamps
/// to +-0x10: on the live flag it resets the model buffers and re-arms the
/// step countdown; otherwise it hands the player offset and the new yaw to
/// the actor's state body and rebuilds the matrix at scale 0x1194.
void func_actor_401800_8013B444(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x12C;
        work->field_898          = 1;
        work->field_8A2          = 0x10;
        work->field_89E          = 9;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401800_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player and rescale its root coordinate, turning the
/// stored yaw toward the target by at most 0x28 a frame instead of the hard
/// clamp `func_actor_401800_80135F58` uses. On the live flag it resets the
/// model buffers and re-arms the step countdown; otherwise it hands the
/// player offset and the new yaw to the actor's state body and rebuilds the
/// matrix at scale 0x1194.
void func_actor_401800_8013B784(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.field_1C = 0x12C;
        work->field_898          = 2;
        work->field_8A2          = 0x10;
        work->field_89E          = 0x13;
        work->field_89A          = 0;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        func_actor_401800_80133EB8(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401800_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    if (work->field_8AE < aim->angle) {
        if (aim->angle - work->field_8AE >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = aim->angle;
        }
    } else if (work->field_8AE - aim->angle >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = aim->angle;
    }
    if (work->field_8AE == aim->angle && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
        work->field_0 = 0xB;
    }
    coord      = arg0->field_2C->field_8;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    work->field_898 = 2;
    func_actor_401800_80133EB8(arg0);
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Tint a freshly spawned effect model from the enemy's area record. Same body
/// as `Actor401300_TintEffect`, inlined at each of the four spawn sites below.
static __inline__ void Actor401800_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    GpAreaKey* keyPtr;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    TmdObject* model;
    s32        idx;
    u32        raw;

    if (eff != NULL) {
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        raw         = enemy->field_8;
        model       = (TmdObject*)eff->field_0->extra;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        areaByte0   = Game_Session->field_4;
        idx         = raw >> 12;
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec             = Gp_GetNestedAreaRec(&key);
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
}

/// Step-driven effect spawner for the actor's live ramp: while the spawn flag
/// is set the actor crouches (0x8C8 node pitched to 0x12C, 0xA08 flags bit
/// 0x4000 cleared), plays the 0x60030 debris burst and hands the task to the
/// state-F0 list; the step counter then fires the 0xA0005 effects at 3, 5, 7
/// and 9, each tinted from the enemy's area record, and parks the actor at 0x3D.
void func_actor_401800_8013BB10(Actor401800* arg0)
{
    SVECTOR          vec;
    Actor401800Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0x80;
        work->field_8C8.field_1C = 0x12C;
        work->field_A08.flags    = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.field_4      = 1;
        work->field_8AE          = 0;
        work->field_6            = 0U;
        vec.vx                   = 0x64;
        vec.vz                   = 0;
        vec.vy                   = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401800_80144434;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 9) {
        D_80114B78[0] = &D_actor_401800_80144F24;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013BF48);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013CD98);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800", func_actor_401800_8013D64C);

void func_actor_401800_8013DCB4(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401800/actor_401800", ActorsShared80135df4Table);

/* func_actor_401800_8013DCBC (the 0x7D3 message handler) lives in
 * actor_401800_2.c: the jump table GCC emits for its switch is what owns the
 * rodata run at 0x254, so the function belongs to the unit that starts there. */
